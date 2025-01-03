#include "StdAfx.h"
#include "VRManager.h"

#include "Cry_Camera.h"
#include "Fists.h"
#include "GameCVars.h"
#include "HandPoses.h"
#include "Hooks.h"
#include "OpenXRRuntime.h"
#include "VRRenderer.h"
#include "Weapon.h"
#include "Menus/FlashMenuObject.h"

VRManager s_VRManager;
VRManager* gVR = &s_VRManager;

int AI_SmartObjectEvent_Hook(IAISystem* self, const char* event, IEntity*& user, IEntity*& object, const Vec3* pExtraPoint, bool bHighPriority)
{
	CPlayer* player = gVR->GetLocalPlayer();
	if (player && (player->GetEntity() == user || player->GetEntity() == object))
	{
		if (strcmp("OnUsed", event) == 0 || strcmp("OnUsedRelease", event) == 0)
		{
			player->SignalUsedEntity();
		}
	}
	return hooks::CallOriginal(AI_SmartObjectEvent_Hook)(self, event, user, object, pExtraPoint, bHighPriority);
}

VRManager::~VRManager()
{
	// if Shutdown isn't properly called, we will get an infinite hang when trying to dispose of our D3D resources after
	// the game already shut down. So just let go here to avoid that
	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye].Detach();
		m_eyeTextures11[eye].Detach();
	}
	m_hudTexture.Detach();
	m_hudTexture11.Detach();
	m_swapchain.Detach();
	m_device.Detach();
	m_context11.Detach();
	m_device11.Detach();
}

bool VRManager::Init()
{
	if (m_initialized)
		return true;

	hooks::InstallVirtualFunctionHook("SmartObjectEvent", gEnv->pAISystem, &IAISystem::SmartObjectEvent, &AI_SmartObjectEvent_Hook);

	m_initialized = true;
	return true;
}

void VRManager::Shutdown()
{
	CryLogAlways("Shutting down VRManager...");

	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye].Reset();
		m_eyeTextures11[eye].Reset();
	}
	m_hudTexture.Reset();
	m_hudTexture11.Reset();
	m_swapchain.Reset();
	m_device.Reset();
	m_context11.Reset();
	m_device11.Reset();

	m_initialized = false;
}

void VRManager::AwaitFrame()
{
	if (!m_initialized)
		return;

	gXR->AwaitFrame();
}

void VRManager::CaptureEye(int eye)
{
	if (!m_swapchain)
		return;

	if (!m_device)
		InitDevice(m_swapchain.Get());

	if (!m_eyeTextures[eye])
	{
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	D3D10_TEXTURE2D_DESC desc;
	m_eyeTextures[eye]->GetDesc(&desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	// acquire and copy the current swap chain buffer to the eye texture
	CopyBackbufferToTexture(m_eyeTextures[eye].Get());
}

void VRManager::CaptureHUD()
{
	if (!m_swapchain)
		return;

	if (!m_device)
		InitDevice(m_swapchain.Get());

	if (!m_hudTexture)
	{
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	D3D10_TEXTURE2D_DESC desc;
	m_hudTexture->GetDesc(&desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	// acquire and copy the current swap chain buffer to the HUD texture
	CopyBackbufferToTexture(m_hudTexture.Get());
}

void VRManager::SetSwapChain(IDXGISwapChain *swapchain)
{
	if (swapchain != m_swapchain.Get())
	{
		m_device.Reset();
	}

	m_swapchain = swapchain;
	if (!m_device)
	  InitDevice(swapchain);
}

void VRManager::FinishFrame(bool didRenderThisFrame)
{
	if (!m_initialized || !m_device || !m_device11 || !m_hudTexture11)
		return;

	if (!didRenderThisFrame)
	{
		// bit late technically, but need to do this to be able to update the HUD
		gXR->AwaitFrame();
	}

	ComPtr<IDXGIKeyedMutex> mutexHud;
	m_hudTexture11->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutexHud.GetAddressOf());
	mutexHud->AcquireSync(1, 1000);
	gXR->SubmitHud(m_hudTexture11.Get());
	mutexHud->ReleaseSync(0);

	if (!didRenderThisFrame)
	{
		gXR->FinishFrame();
		return;
	}

	ComPtr<IDXGIKeyedMutex> mutexLeft, mutexRight;
	m_eyeTextures11[0]->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutexLeft.GetAddressOf());
	mutexLeft->AcquireSync(1, 1000);
	m_eyeTextures11[1]->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutexRight.GetAddressOf());
	mutexRight->AcquireSync(1, 1000);

	// game is currently using symmetric projection, we need to cut off the texture accordingly
	RectF leftBounds = GetEffectiveRenderLimits(0);
	RectF rightBounds = GetEffectiveRenderLimits(1);
	gXR->SubmitEyes(m_eyeTextures11[0].Get(), leftBounds, m_eyeTextures11[1].Get(), rightBounds);

	mutexLeft->ReleaseSync(0);
	mutexRight->ReleaseSync(0);

	gXR->FinishFrame();
}

Vec2i VRManager::GetRenderSize() const
{
	float ll, lr, lt, lb, rl, rr, rt, rb;
	gXR->GetFov(0, ll, lr, lt, lb);
	gXR->GetFov(1, rl, rr, rt, rb);
	if (ll == 0)
	{
		// XR is not running, yet
		return Vec2i(gEnv->pRenderer->GetWidth(), gEnv->pRenderer->GetHeight());
	}
	float verticalFov = max(max(fabsf(lt), fabsf(lb)), max(fabsf(rt), fabsf(rb)));
	float horizontalFov = max(max(fabsf(ll), fabsf(lr)), max(fabsf(rl), fabsf(rr)));
	float vertRenderScale = 2.f * verticalFov / min(fabsf(lt) + fabsf(lb), fabsf(rt) + fabsf(rb));

	Vec2i renderSize = gXR->GetRecommendedRenderSize();
	renderSize.y *= vertRenderScale;
	renderSize.x = renderSize.y * horizontalFov / verticalFov;
	return renderSize;
}

Vec3 VRManager::EstimateShoulderPosition(int side)
{
	CPlayer *player = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!player)
		return Vec3(0, 0, 0);

	CCamera view = gVRRenderer->GetCurrentViewCamera();
	ModifyViewCamera(side, view);

	return view.GetMatrix() * Vec3((-1.f + 2.f * side) * 0.2f, +0.05f, -0.3f);
}

void VRManager::ModifyViewCamera(int eye, CCamera& cam)
{
	if (IsEquivalent(cam.GetPosition(), Vec3(0, 0, 0), VEC_EPSILON))
	{
		// no valid camera set, leave it
		return;
	}

	if (!m_initialized)
	{
		if (eye == 1)
		{
			Vec3 pos = cam.GetPosition();
			pos.x += 0.1f;
			cam.SetPosition(pos);
		}
		return;
	}

	Ang3 angles = cam.GetAngles();
	Vec3 position = cam.GetPosition();

	// eliminate pitch and roll
	// switch around, because these functions do not agree on which angle is what...
	angles.z = angles.x;
	angles.y = 0;
	angles.x = 0;

	IViewSystem* system = g_pGame->GetIGameFramework()->GetIViewSystem();
	bool isCutscene = system && system->IsPlayingCutScene();

	if (eye == 0)
	{
		// manage the aiming deadzone in which the camera should not be rotated
		float yawDiff = angles.z - m_prevViewYaw;
		if (yawDiff < -gf_PI)
			yawDiff += 2 * gf_PI;
		else if (yawDiff > gf_PI)
			yawDiff -= 2 * gf_PI;

		float maxDiff = g_pGameCVars->vr_yaw_deadzone_angle * gf_PI / 180.f;
		if (g_pGameCVars->vr_enable_motion_controllers)
			maxDiff = 0;
		if (isCutscene)
		{
			maxDiff = g_pGameCVars->vr_cutscenes_angle_snap * gf_PI / 180.f;
			if (g_pGameCVars->vr_cutscenes_2d)
				maxDiff = 0;
			if (yawDiff > maxDiff || yawDiff < -maxDiff)
				m_prevViewYaw = angles.z;
		}
		else
		{
			if (yawDiff > maxDiff)
				m_prevViewYaw += yawDiff - maxDiff;
			if (yawDiff < -maxDiff)
				m_prevViewYaw += yawDiff + maxDiff;
			if (m_prevViewYaw > gf_PI)
				m_prevViewYaw -= 2*gf_PI;
			if (m_prevViewYaw < -gf_PI)
				m_prevViewYaw += 2*gf_PI;
		}

		CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
		if (pPlayer && pPlayer->GetLinkedVehicle())
		{
			// don't use this while in a vehicle, it feels off
			m_prevViewYaw = angles.z;
		}
	}
	angles.z = m_prevViewYaw;

	Matrix34 viewMat;
	viewMat.SetRotationXYZ(angles, position);

	Matrix34 eyeMat = GetEyeTransform(eye);
	viewMat = viewMat * eyeMat;

	cam.SetMatrix(viewMat);

	// we don't have obvious access to the projection matrix, and the camera code is written with symmetric projection in mind
	// for now, set up a symmetric FOV and cut off parts of the image during submission
	float tanl, tanr, tant, tanb;
	gXR->GetFov(eye, tanl, tanr, tant, tanb);
	float verticalFov = max(fabsf(tant), fabsf(tanb));
	float vertFov = atanf(verticalFov) * 2;
	Vec2i renderSize = GetRenderSize();
	cam.SetFrustum(renderSize.x, renderSize.y, vertFov, cam.GetNearPlane(), cam.GetFarPlane());

	// but we can set up frustum planes for our asymmetric projection, which should help culling accuracy.
	cam.UpdateFrustumFromVRRaw(tanl, tanr, tanb, tant);
}

void VRManager::ModifyViewCameraFor3DCinema(int eye, CCamera& cam)
{
	Matrix34 transform = cam.GetMatrix();
	Vec3 pos = transform.GetTranslation();
	pos += g_pGameCVars->vr_cinema_3d_eye_dist * transform.GetColumn0().GetNormalized() * (eye == 0 ? -1 : 1);
	transform.SetTranslation(pos);
	cam.SetMatrix(transform);
}

void VRManager::ModifyViewForBinoculars(SViewParams& view)
{
	float vr_binocular_size = 0.8f;
	bool leftHanded = g_pGameCVars->vr_weapon_hand == 0;

	Matrix34 controllerTransform = gXR->GetInput()->GetControllerTransform(leftHanded ? 1 : 0);
	Vec3 forward = controllerTransform.GetColumn1();
	controllerTransform.SetTranslation(controllerTransform.GetTranslation() + 0.5f * forward);
	controllerTransform = controllerTransform * Matrix34::CreateTranslationMat(Vec3((leftHanded ? 1 : -1) * vr_binocular_size / 2, 0, vr_binocular_size / 2));

	Matrix33 refTransform = GetReferenceTransform();
	Matrix34 adjustedControllerTransform = refTransform * (Matrix33)controllerTransform;
	adjustedControllerTransform.SetTranslation(refTransform * (controllerTransform.GetTranslation() - m_referencePosition));

	Ang3 viewAngles = Ang3(view.rotation);
	viewAngles.x = viewAngles.y = 0;

	Matrix34 viewMatrix = Matrix34::CreateRotationXYZ(viewAngles, view.position) * adjustedControllerTransform;
	view.rotation = GetQuatFromMat33((Matrix33)viewMatrix);
	view.position = viewMatrix.GetTranslation();
}

void VRManager::ModifyWeaponPosition(CPlayer* player, Ang3& weaponAngles, Vec3& weaponPosition, bool slave)
{
	if (!g_pGameCVars->vr_enable_motion_controllers
		|| g_pGame->GetMenu()->IsMenuActive()
		|| g_pGame->GetHUD()->GetModalHUD()
		|| !gVRRenderer->ShouldRenderVR())
	{
		return;
	}

	CWeapon* weapon = player->GetWeapon(player->GetCurrentItemId());
	if (!weapon)
		return;

	if (slave)
	{
		weapon = dynamic_cast<CWeapon*>(weapon->GetDualWieldSlave());
		if (!weapon)
			return;
	}

	int weaponHand = g_pGameCVars->vr_weapon_hand;
	if (weapon->IsDualWieldMaster())
		weaponHand = 1;
	if (weapon->IsDualWieldSlave())
		weaponHand = 0;

	Matrix34 adjustedControllerTransform = GetControllerWeaponTransform(weaponHand);
	// if we are two-handing the weapon and it's not a pistol, apply a two hand orientation
	if (IsOffHandGrabbingWeapon() && weapon->GetEntity()->GetClass() != CItem::sSOCOMClass)
	{
		adjustedControllerTransform = GetTwoHandWeaponTransform();
	}

	weaponAngles.x = weaponAngles.y = 0;
	Matrix34 weaponWorldTransform = Matrix34::CreateRotationXYZ(weaponAngles, weaponPosition);
	Matrix34 inverseWeaponGripTransform = weapon->GetInverseGripTransform();
	Matrix34 trackedTransform = weaponWorldTransform * adjustedControllerTransform * inverseWeaponGripTransform;
	weaponPosition = trackedTransform.GetTranslation();
	weaponAngles = Ang3(trackedTransform);
}

Matrix34 VRManager::GetControllerTransform(int side)
{
	Matrix34 controllerTransform = gXR->GetInput()->GetControllerTransform(side);
	Matrix33 refTransform = GetReferenceTransform();
	Matrix34 adjustedControllerTransform = refTransform * (Matrix33)controllerTransform;
	adjustedControllerTransform.SetTranslation(refTransform * (controllerTransform.GetTranslation() - m_referencePosition));
	return adjustedControllerTransform;
}

Matrix34 VRManager::GetControllerWeaponTransform(int side)
{
	Matrix34 controllerTransform = gXR->GetInput()->GetControllerWeaponTransform(side);
	Matrix33 refTransform = GetReferenceTransform();
	Matrix34 adjustedControllerTransform = refTransform * (Matrix33)controllerTransform;
	adjustedControllerTransform.SetTranslation(refTransform * (controllerTransform.GetTranslation() - m_referencePosition));
	return adjustedControllerTransform;
}

Matrix34 VRManager::GetTwoHandWeaponTransform()
{
	int weaponHand = g_pGameCVars->vr_weapon_hand;
	int offHand = 1 - weaponHand;

	Matrix34 mainHandTransform = GetControllerTransform(weaponHand);
	Matrix34 offHandTransform = GetControllerTransform(offHand);

	// build rotation from main hand towards off hand
	Vec3 fwd = offHandTransform.GetTranslation() - mainHandTransform.GetTranslation();
	fwd.Normalize();
	Vec3 right = mainHandTransform.GetColumn0().GetNormalized();
	Vec3 up = right.Cross(fwd).GetNormalized();
	right = fwd.Cross(up).GetNormalized();
	mainHandTransform.SetFromVectors(right, fwd, up, mainHandTransform.GetTranslation());

	// Weapon bones are offset to what our grip pose is, so we need to rotate the pose a bit
	Matrix33 correction = Matrix33::CreateRotationX(-gf_PI/2) * Matrix33::CreateRotationY(-gf_PI/2);
	
	return mainHandTransform * correction;
}

Matrix34 VRManager::GetWorldControllerWeaponTransform(int side)
{
	Matrix34 controllerTransform = GetControllerWeaponTransform(side);
	Matrix34 view = gVRRenderer->GetCurrentViewCamera().GetMatrix();
	Ang3 viewAngles (view);
	viewAngles.x = viewAngles.y = 0;
	view.SetRotationXYZ(viewAngles, view.GetTranslation());
	return view * controllerTransform;
}

void VRManager::ModifyPlayerEye(CPlayer* pPlayer, Vec3& eyePosition, Vec3& eyeDirection)
{
	if (!g_pGameCVars->vr_enable_motion_controllers
		|| g_pGame->GetMenu()->IsMenuActive()
		|| g_pGame->GetHUD()->GetModalHUD()
		|| !gVRRenderer->ShouldRenderVR())
	{
		return;
	}

	if (pPlayer->GetActorStats()->mountedWeaponID)
		return;

	CCamera left = gVRRenderer->GetCurrentViewCamera();
	ModifyViewCamera(0, left);
	CCamera right = gVRRenderer->GetCurrentViewCamera();
	ModifyViewCamera(1, right);

	eyePosition = 0.5f * (left.GetPosition() + right.GetPosition());
	eyeDirection = 0.5f * (left.GetViewdir() + right.GetViewdir());
}

Quat VRManager::GetHMDQuat()
{
	return 0.5f * (Quat(GetEyeTransform(0)) + Quat(GetEyeTransform(1)));
}

RectF VRManager::GetEffectiveRenderLimits(int eye)
{
	float l, r, t, b;
	gXR->GetFov(eye, l, r, t, b);
	float verticalFov = max(fabsf(t), fabsf(b));
	float horizontalFov = max(fabsf(l), fabsf(r));
	RectF result;
	if (verticalFov > 0)
	{
		result.x = 0.5f + 0.5f * l / horizontalFov;
		result.y = 0.5f - 0.5f * t / verticalFov;
		result.w = 0.5f + 0.5f * r / horizontalFov - result.x;
		result.h = 0.5f - 0.5f * b / verticalFov - result.y;
	}
	else
	{
		result.x = 0;
		result.y = 0;
		result.w = 1;
		result.h = 1;
	}
	return result;
}

Matrix33 VRManager::GetReferenceTransform() const
{
	Ang3 refAngles(0, 0, m_referenceYaw);
	return Matrix33::CreateRotationXYZ(refAngles).GetTransposed();
}

Vec3 VRManager::GetHmdOffset() const
{
	Vec3 position = gXR->GetHmdTransform().GetTranslation();
	position.z = 0;
	return GetReferenceTransform() * (position - m_referencePosition);
}

float VRManager::GetHmdYawOffset() const
{
	Ang3 angles(gXR->GetHmdTransform());
	return angles.z - m_referenceYaw;
}

void VRManager::UpdateReferenceOffset(const Vec3& offset)
{
	// added offset is in player space, convert back to raw HMD space
	Vec3 rawOffset = GetReferenceTransform().GetTransposed() * offset;
	rawOffset.z = 0;
	m_referencePosition += rawOffset;
}

void VRManager::UpdateReferenceYaw(float yaw)
{
	m_referenceYaw += yaw;
}

Matrix34 VRManager::GetEyeTransform(int eye) const
{
	Matrix34 rawEye = gXR->GetRenderEyeTransform(eye);
	Vec3 position = GetReferenceTransform() * (rawEye.GetTranslation() - m_referencePosition);
	Ang3 angles(rawEye);
	angles.z -= m_referenceYaw;
	return Matrix34::CreateRotationXYZ(angles, position);
}

void VRManager::Update()
{
	if (!g_pGameCVars->vr_enable_motion_controllers)
		return;

	if ((g_pGame->GetMenu()->IsMenuActive() || gEnv->pConsole->IsOpened()))
	{
		if (!m_wasInMenu)
		{
			m_wasInMenu = true;
			RecalibrateView();
		}
		SetHudInFrontOfPlayer();
		return;
	}

	if (m_wasInMenu)
	{
		m_wasInMenu = false;
		RecalibrateView();
	}

	bool showHudFixed= g_pGame->GetHUD() && g_pGame->GetHUD()->ShouldDisplayHUDFixed();
	if (gVRRenderer->AreBinocularsActive())
		SetHudAttachedToOffHand();
	else if (gVRRenderer->ShouldRenderVR() && !showHudFixed)
		SetHudAttachedToHead();
	else
		SetHudInFrontOfPlayer();
}

void VRManager::RecalibrateView()
{
	Matrix34 hmdTransform = Matrix34(gXR->GetHmdTransform());

	m_referencePosition = hmdTransform.GetTranslation();
	m_referencePosition.z = 0;

	Ang3 angles;
	angles.SetAnglesXYZ((Matrix33)hmdTransform);
	m_referenceYaw = angles.z;

	// recalibrate menu positioning
	angles.x = angles.y = 0;
	m_fixedHudTransform.SetRotationXYZ(angles, hmdTransform.GetTranslation());
	Vec3 dir = -m_fixedHudTransform.GetColumn1();
	Vec3 pos = m_fixedHudTransform.GetTranslation() + 5 * dir;
	m_fixedHudTransform.SetTranslation(pos);
}

extern XrPosef CrysisToOpenXR(const Matrix34& transform);

void VRManager::SetHudInFrontOfPlayer()
{
	if (!m_fixedPositionInitialized)
	{
		RecalibrateView();
		m_fixedPositionInitialized = true;
	}

	gXR->SetHudPose(CrysisToOpenXR(m_fixedHudTransform));

	Vec2i renderSize = GetRenderSize();
	gXR->SetHudSize(4.f, 4.f * renderSize.y / renderSize.x);
}

void VRManager::SetHudAttachedToHead()
{
	m_fixedPositionInitialized = false;

	Matrix34 hudTransform = Matrix34(gXR->GetHmdTransform());
	Vec3 pos = hudTransform.GetTranslation();
	Ang3 angles;
	angles.SetAnglesXYZ((Matrix33)hudTransform);
	angles.x = -angles.x;
	angles.y = -angles.y;
	pos.x = -pos.x;
	pos.y = -pos.y;
	hudTransform.SetRotationXYZ(angles, pos);
	Vec3 forward = -hudTransform.GetColumn1();
	hudTransform.SetTranslation(hudTransform.GetTranslation() + 2 * forward);
	gXR->SetHudPose(CrysisToOpenXR(hudTransform));

	Vec2i renderSize = GetRenderSize();
	gXR->SetHudSize(2.f, 2.f * renderSize.y / renderSize.x);
}

void VRManager::SetHudAttachedToOffHand()
{
	float vr_binocular_size = 0.8f;
	m_fixedPositionInitialized = false;
	bool leftHanded = g_pGameCVars->vr_weapon_hand == 0;
	Matrix34 transform = gXR->GetInput()->GetControllerTransform(leftHanded ? 1 : 0);

	Vec3 pos = transform.GetTranslation();
	Ang3 angles;
	angles.SetAnglesXYZ((Matrix33)transform);
	angles.x = -angles.x;
	angles.y = -angles.y;
	pos.x = -pos.x;
	pos.y = -pos.y;
	transform.SetRotationXYZ(angles, pos);

	Vec3 forward = -transform.GetColumn1();
	transform.SetTranslation(transform.GetTranslation() + 0.5f * forward);
	transform = transform * Matrix34::CreateTranslationMat(Vec3((leftHanded ? 1 : -1) * vr_binocular_size / 2, 0, vr_binocular_size / 2));

	XrPosef hudTransform = CrysisToOpenXR(transform);
	gXR->SetHudPose(hudTransform);

	Vec2i renderSize = GetRenderSize();
	gXR->SetHudSize(vr_binocular_size, vr_binocular_size * renderSize.y / renderSize.x);
}

void TwoBoneIKSolve(QuatT& a, QuatT& b, QuatT& c, const Vec3& t)
{
	QuatT wa = a;
	QuatT wb = wa * b;
	QuatT wc = wb * c;

	Vec3 a2b = wb.t - wa.t;
	Vec3 a2c = wc.t - wa.t;
	Vec3 b2c = wc.t - wb.t;
	Vec3 a2t = t - wa.t;

	float lab = a2b.GetLength();
	float lbc = b2c.GetLength();
	float lac = a2c.GetLength();
	float lat = clamp(a2t.GetLength(), 0, lab + lbc);

	// get current interior angles
	float ac_ab_0 = cry_acosf(a2c.Dot(a2b) / lab / lac);
	float ba_bc_0 = cry_acosf((-a2b).Dot(b2c) / lab / lbc);
	float ac_at_0 = cry_acosf(a2c.Dot(a2t) / lac / a2t.GetLength());

	// desired angles based on the cosine rule
	float ac_ab_1 = cry_acosf((lab*lab + lat*lat - lbc*lbc) / (2*lab*lat));
	float ba_bc_1 = cry_acosf((lab*lab + lbc*lbc - lat*lat) / (2*lab*lbc));

	// apply angles locally to the joints
	Vec3 axis0 = a2c.Cross(a2b).GetNormalized();
	Vec3 axis1 = a2c.Cross(a2t).GetNormalized();
	Quat r0 = Quat::CreateRotationAA(ac_ab_1 - ac_ab_0, wa.q.GetInverted() * axis0);
	Quat r1 = Quat::CreateRotationAA(ba_bc_1 - ba_bc_0, wb.q.GetInverted() * axis0);
	Quat r2 = Quat::CreateRotationAA(ac_at_0, wa.q.GetInverted() * axis1);
	a.q = a.q * (r0 * r2);
	b.q = b.q * r1;
}

void VRManager::CalcWeaponArmIK(int side, ISkeletonPose* skeleton, CWeapon* weapon)
{
	Matrix34 invEntityTrans = weapon->GetEntity()->GetWorldTM().GetInvertedFast();
	Vec3 shoulderWorldPos = gVR->EstimateShoulderPosition(side);
	Vec3 shoulderInWeaponPos = invEntityTrans.TransformPoint(shoulderWorldPos);
	
	int16 shoulderJointId = skeleton->GetJointIDByName(side == 1 ? "upperarm_R" : "upperarm_L");
	int16 elbowJointId = skeleton->GetJointIDByName(side == 1 ? "forearm_R" : "forearm_L");
	int16 handJointId = skeleton->GetJointIDByName(side == 1 ? "hand_R" : "hand_L");

	// the way this works is that the weapon and thus the hands are already positioned as intended
	// we merely set a new base position for the shoulder joint and then IK solve such that the hand joint
	// ends up in its previous position
	QuatT shoulderJoint = skeleton->GetAbsJointByID(shoulderJointId);
	shoulderJoint.t = shoulderInWeaponPos;
	QuatT elbowJoint = skeleton->GetDefaultRelJointByID(elbowJointId);
	QuatT handJoint = skeleton->GetDefaultRelJointByID(handJointId);
	QuatT target = skeleton->GetAbsJointByID(handJointId);

	if (side != g_pGameCVars->vr_weapon_hand && !m_offHandFollowsWeapon && !weapon->IsReloading() && !weapon->IsDualWieldSlave())
	{
		// off hand is detached from weapon, so position it towards the controller, instead
		Quat origHandRot = target.q;
		target = weapon->CalcHandFromControllerInWeapon(side);
		Quat rotDiff = target.q * origHandRot.GetInverted();
		shoulderJoint.q = rotDiff * shoulderJoint.q;
	}
	
	float maxDistance = elbowJoint.t.GetLength() + handJoint.t.GetLength();
	if (target.t.GetDistance(shoulderInWeaponPos) > maxDistance)
	{
		Vec3 dir = (shoulderInWeaponPos - target.t).GetNormalized();
		shoulderJoint.t = target.t + dir * maxDistance;
	}

	TwoBoneIKSolve(shoulderJoint, elbowJoint, handJoint, target.t);

	// make sure the hand joint really ends up in the same position and orientation, no matter what
	handJoint.q = (shoulderJoint.q * elbowJoint.q).GetInverted() * target.q;
	Vec3 diffToTarget = target.t - (shoulderJoint * elbowJoint * handJoint).t;
	shoulderJoint.t += diffToTarget;

	int16 parent = skeleton->GetParentIDByID(shoulderJointId);
	shoulderJoint = skeleton->GetAbsJointByID(parent).GetInverted() * shoulderJoint;
	skeleton->SetPostProcessQuat(shoulderJointId, shoulderJoint);
	skeleton->SetPostProcessQuat(elbowJointId, elbowJoint);
	skeleton->SetPostProcessQuat(handJointId, handJoint);

	if (side != g_pGameCVars->vr_weapon_hand && !m_offHandFollowsWeapon && !weapon->IsDualWield())
	{
		ApplyHandPose(side, skeleton, gXR->GetInput()->GetGripAmount(side));
	}
	if (side == g_pGameCVars->vr_weapon_hand && dynamic_cast<CFists*>(weapon) != nullptr && !weapon->IsDualWield())
	{
		ApplyHandPose(side, skeleton, gXR->GetInput()->GetGripAmount(side));
	}
}

void VRManager::TryGrabWeaponWithOffHand()
{
	CPlayer* player = GetLocalPlayer();
	CWeapon* weapon = player->GetWeapon(player->GetCurrentItemId());
	if (!weapon)
		return;
	if (weapon->IsDualWield() || dynamic_cast<CFists*>(weapon) != nullptr || dynamic_cast<COffHand*>(weapon) != nullptr)
		return;
	
	Vec3 controllerPos = GetWorldControllerWeaponTransform(1 - g_pGameCVars->vr_weapon_hand).GetTranslation();
	float controllerFromWeapon = weapon->GetOffHandGrabLocation().GetDistance(controllerPos);

	if (controllerFromWeapon <= 0.3f)
	{
		m_offHandFollowsWeapon = true;

		// apply benefits of the flat game's iron sights zoom for recoil and spread
		weapon->SetCurrentZoomMode(0);
		if (IZoomMode* zm = weapon->GetZoomMode(weapon->GetCurrentZoomMode()))
		{
			zm->ApplyZoomMod(weapon->GetFireMode(weapon->GetCurrentFireMode()));
		}
	}
}

void VRManager::DetachOffHandFromWeapon()
{
	m_offHandFollowsWeapon = false;
	
	CPlayer* player = GetLocalPlayer();
	CWeapon* weapon = player->GetWeapon(player->GetCurrentItemId());
	if (!weapon)
		return;

	weapon->SetCurrentZoomMode(0);
	weapon->GetFireMode(weapon->GetCurrentFireMode())->ResetRecoilMod();
	weapon->GetFireMode(weapon->GetCurrentFireMode())->ResetSpreadMod();
}

CPlayer* VRManager::GetLocalPlayer() const
{
	return static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
}

void VRManager::InitDevice(IDXGISwapChain* swapchain)
{
	m_hudTexture.Reset();
	m_eyeTextures[0].Reset();
	m_eyeTextures[1].Reset();

	CryLogAlways("Acquiring device...");
	swapchain->GetDevice(__uuidof(ID3D10Device1), (void**)m_device.ReleaseAndGetAddressOf());
	if (!m_device)
	{
		CryLogAlways("Failed to get game's D3D10.1 device!");
		return;
	}
	if (m_device->GetFeatureLevel() != D3D10_FEATURE_LEVEL_10_1)
	{
		CryLogAlways("Device only has feature level %i", m_device->GetFeatureLevel());
	}

	ComPtr<IDXGIDevice> dxgiDevice;
	m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf());
	if (dxgiDevice)
	{
		ComPtr<IDXGIAdapter> d3d10Adapter;
		dxgiDevice->GetAdapter(d3d10Adapter.GetAddressOf());
		DXGI_ADAPTER_DESC desc;
		d3d10Adapter->GetDesc(&desc);
		CryLogAlways("Game is rendering to device %ls", desc.Description);
	}

	CryLogAlways("Creating D3D11 device");
	LUID requiredAdapterLuid;
	D3D_FEATURE_LEVEL requiredLevel;
	gXR->GetD3D11Requirements(&requiredAdapterLuid, &requiredLevel);
	ComPtr<IDXGIFactory1> factory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("Failed to create DXGI factory: %i", hr);
		return;
	}
	ComPtr<IDXGIAdapter1> adapter;
	for (UINT idx = 0; factory->EnumAdapters1(idx, adapter.ReleaseAndGetAddressOf()) == S_OK; ++idx)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.AdapterLuid.HighPart == requiredAdapterLuid.HighPart && desc.AdapterLuid.LowPart == requiredAdapterLuid.LowPart)
		{
			CryLogAlways("Found adapter for XR rendering: %ls", desc.Description);
			break;
		}
	}

	hr = D3D11CreateDevice(adapter.Get(), adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_SINGLETHREADED, &requiredLevel, 1, D3D11_SDK_VERSION,
		m_device11.ReleaseAndGetAddressOf(), nullptr, m_context11.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("Failed to create D3D11 device: %i", hr);
	}

	gXR->CreateSession(m_device11.Get());
}

void VRManager::CreateEyeTexture(int eye)
{
	Vec2i size = GetRenderSize();
	CryLogAlways("Creating eye texture %i: %i x %i", eye, size.x, size.y);
	CreateSharedTexture(m_eyeTextures[eye], m_eyeTextures11[eye], size.x, size.y);
}

void VRManager::CreateHUDTexture()
{
	Vec2i size = GetRenderSize();
	CryLogAlways("Creating HUD texture: %i x %i", size.x, size.y);
	CreateSharedTexture(m_hudTexture, m_hudTexture11, size.x, size.y);
}

void VRManager::CreateSharedTexture(ComPtr<ID3D10Texture2D> &texture, ComPtr<ID3D11Texture2D> &texture11, int width, int height)
{
	if (!m_device || !m_device11)
		return;

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = 0;
	desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("D3D10 CreateTexture2D failed: %i", hr);
		return;
	}

	CryLogAlways("Sharing texture with D3D11...");
	ComPtr<IDXGIResource> dxgiRes;
	texture->QueryInterface(__uuidof(IDXGIResource), (void**)dxgiRes.GetAddressOf());
	HANDLE handle;
	dxgiRes->GetSharedHandle(&handle);
	hr = m_device11->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)texture11.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("D3D11 OpenSharedResource failed: %i", hr);
	}
}

void VRManager::CopyBackbufferToTexture(ID3D10Texture2D *target)
{
	// acquire and copy the current swap chain buffer to the HUD texture
	ComPtr<ID3D10Texture2D> backbuffer;
	m_swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)backbuffer.GetAddressOf());
	if (!backbuffer)
	{
		CryLogAlways("Error: failed to acquire current swapchain buffer");
		return;
	}

	ComPtr<IDXGIKeyedMutex> mutex;
	target->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutex.GetAddressOf());
	HRESULT hr = mutex->AcquireSync(0, 1000);
	if (hr != S_OK)
	{
		CryLogAlways("Error while waiting for mutex during copy: %i", hr);
	}

	D3D10_TEXTURE2D_DESC rtDesc;
	backbuffer->GetDesc(&rtDesc);
	if (rtDesc.SampleDesc.Count > 1)
	{
		m_device->ResolveSubresource(target, 0, backbuffer.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	else
	{
		m_device->CopyResource(target, backbuffer.Get());
	}

	mutex->ReleaseSync(1);
}

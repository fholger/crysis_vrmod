#pragma once
#include <d3d10_1.h>
#include <dxgi.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "EVRHand.h"
#include "HUD/HUD.h"

#undef GetUserName
#undef PlaySound
#undef min
#undef max
#undef small
#undef DrawText
#undef GetMessage

using Microsoft::WRL::ComPtr;

class VRManager
{
public:
	~VRManager();

	bool Init();
	bool IsInitialized() const { return m_initialized; }
	void Shutdown();

	void ReinitializeXR();

	void AwaitFrame();

	void CaptureEye(int eye);
	void CaptureHUD();

	IDXGISwapChain* GetSwapChain() const { return m_swapchain.Get(); }
	void SetSwapChain(IDXGISwapChain *swapchain);
	void FinishFrame(bool didRenderThisFrame);

	Vec2i GetRenderSize() const;

	Vec3 EstimateShoulderPosition(int side, const Vec3& handPos, float minDistance, float maxDistance);

	void ModifyViewCamera(int eye, CCamera& cam);
	void ModifyViewCameraFor3DCinema(int eye, CCamera& cam);
	void ModifyViewForBinoculars(SViewParams& view);
	void ModifyCameraFor2D(CCamera& cam);
	void ModifyWeaponPosition(CPlayer* player, Ang3& weaponAngles, Vec3& weaponPosition, bool slave = false);
	Matrix34 GetControllerTransform(int side);
	Matrix34 GetWorldControllerTransform(int side);
	Matrix34 GetControllerWeaponTransform(int side);
	Matrix34 GetTwoHandWeaponTransform();
	Matrix34 GetWorldControllerWeaponTransform(int side);
	Vec3 GetControllerVelocity(int side);
	Vec3 GetControllerWorldVelocity(int side);
	void ModifyPlayerEye(CPlayer* pPlayer, Vec3& eyePosition, Vec3& eyeDirection);
	Quat GetHMDQuat();

	RectF GetEffectiveRenderLimits(int eye);

	Matrix33 GetReferenceTransform() const;
	Vec3 GetHmdOffset() const;
	float GetHmdYawOffset() const;

	Matrix34 GetBaseVRTransform(bool smooth) const;

	void UpdateReferenceOffset(const Vec3& offset);
	void UpdateReferenceYaw(float yaw);

	Matrix34 GetEyeTransform(int eye) const;

	EStance GetPhysicalStance() const;

	void Update();
	bool RecalibrateView();

	void SetHudInFrontOfPlayer();
	void SetHudAttachedToHead();
	void SetHudAttachedToOffHand();
	void SetHudAttachedToWeaponHand();
	void SetVehicleHud();

	void CalcWeaponArmIK(int side, ISkeletonPose* skeleton, CWeapon* weapon);

	void TryGrabWeaponWithOffHand();
	void DetachOffHandFromWeapon();
	bool IsOffHandGrabbingWeapon() const { return m_offHandFollowsWeapon; }

	CPlayer* GetLocalPlayer() const;

	int GetHandSide(EVRHand hand) const;
	bool IsHandNearHead(EVRHand hand, float maxDist = .25f);
	bool IsHandNearShoulder(EVRHand hand);
	bool IsHandNearHip(EVRHand hand);
	bool IsHandBehindBack(EVRHand hand);
	bool IsHandNearChest(EVRHand hand);

	const ray_hit* GetPointAtPoint(float maxDist = 0) const;
	EntityId GetPointAtEntityId() const { return m_pointAtEntityId; }

private:
	bool m_initialized = false;
	ComPtr<ID3D10Device1> m_device;
	ComPtr<IDXGISwapChain> m_swapchain;
	ComPtr<ID3D10Texture2D> m_eyeTextures[2];
	ComPtr<ID3D10ShaderResourceView> m_eyeViews[2];
	ComPtr<ID3D10Texture2D> m_hudTexture;

	// D3D11 resources for OpenXR submission
	ComPtr<ID3D11Device> m_device11;
	ComPtr<ID3D11DeviceContext> m_context11;
	ComPtr<ID3D11Texture2D> m_eyeTextures11[2];
	ComPtr<ID3D11Texture2D> m_hudTexture11;
	bool m_acquiredRenderSyncs = false;

	void UpdateSmoothedPlayerHeight();

	float m_prevViewYaw = 0;
	mutable float m_updatedViewYaw = 0;

	void InitDevice(IDXGISwapChain* swapchain);
	void CreateEyeTexture(int eye);
	void CreateHUDTexture();
	void CreateSharedTexture(ComPtr<ID3D10Texture2D>& texture, ComPtr<ID3D11Texture2D>& texture11, int width, int height);
	void CopyBackbufferToTexture(ID3D10Texture2D *target);

	void AcquireTextureSync(ID3D10Texture2D* target, int key);
	void AcquireTextureSync(ID3D11Texture2D* target, int key);
	void ReleaseTextureSync(ID3D10Texture2D* target, int key);
	void ReleaseTextureSync(ID3D11Texture2D* target, int key);

	Ang3 m_smoothedWeaponAngles;

	Vec3 m_referencePosition;
	float m_referenceYaw = 0;
	Matrix34 m_fixedHudTransform;
	bool m_fixedPositionInitialized = false;
	float m_hmdReferenceHeight = 0;

	bool m_wasInMenu = false;

	bool m_offHandFollowsWeapon = false;

	int m_smoothHeightType = 0;
	float m_smoothedHeight = 0;
	float m_smoothedHeightOffset = 0;

	void UpdateOffHandRayQuery();
	ray_hit m_offHandRayHit;
	bool m_offHandRayHitAny = false;
	EntityId m_pointAtEntityId;

	int m_numFailedSyncs = 0;
	bool m_shownSyncFailureMessage = false;
};

extern VRManager* gVR;

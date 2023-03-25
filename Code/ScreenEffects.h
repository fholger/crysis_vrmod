// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
// ScreenEffects - Allows for simultaneous and queued blending of effects
//  John Newfield

#pragma once

// Key terms
// speed: progress/second accomplished
// progress: progress from 0 to 1 based on effect speed and frame time (the X of the equation)
// point: between 0-1, the power of the effect (the Y)

//-BlendTypes---------------------------
// Defines a curve to be used by the effect
// Progress is between 0 and 1. Consider it the X, and return the Y.
// The return should also be between 0 and 1.
typedef struct IBlendType
{
	virtual float Blend(float progress) = 0;
	virtual void Release() { delete this; };
} IBlendType;

// Standard linear blend
class CLinearBlend : public IBlendType
{
public:
	CLinearBlend(float slope) { m_slope = slope; };
	CLinearBlend() { CLinearBlend::CLinearBlend(1.0f); };
	virtual ~CLinearBlend() {};

	virtual float Blend(float progress) {
		return (m_slope * progress);
	}
private:
	float m_slope;
};

// A cosin-based curve blend
class CWaveBlend : public IBlendType
{
public:
	CWaveBlend() {};
	virtual ~CWaveBlend() {};
	virtual float Blend(float progress) {
		return ((cos((progress * g_PI) - g_PI) + 1.0f)/2.0f);
	}
};

//-BlendedEffects-----------------------
// BlendedEffect interface, derive from this.
typedef struct IBlendedEffect 
{
public:
	virtual void Init() {}; // called when the effect actually starts rather than when it is created.
	virtual void Update(float point) = 0; // point is from 0 to 1 and is the "amount" of effect to use.
	virtual void Release() { delete this; };
} IBlendedEffect;

// FOV effect
class CFOVEffect : public IBlendedEffect
{
public:
	CFOVEffect(EntityId ownerID, float goalFOV);
	virtual ~CFOVEffect() {};
	virtual void Init();
	virtual void Update(float point);
private:
	float m_currentFOV;
	float m_goalFOV;
	float m_startFOV;
	EntityId m_ownerID;
};

// Post process effect
class CPostProcessEffect : public IBlendedEffect
{
public:
	CPostProcessEffect(EntityId ownerID, string paramName, float goalVal);
	virtual ~CPostProcessEffect() {};
	virtual void Init();
	virtual void Update(float point);
private:
	float m_startVal;
	float m_currentVal;
	float m_goalVal;
	string m_paramName;
	EntityId m_ownerID;
};

//-BlendJobNode------------------------
// Has all of the information for one blend.
// That is, a blend type, an effect, and a speed.
// Speed is how fast progress goes from 0 to 1.
typedef struct SBlendJobNode
{
	IBlendedEffect *myEffect;
	SBlendJobNode *next;
	IBlendType *blendType;
	int blendGroup;
	float speed;
	float progress;
	SBlendJobNode()
	{
		blendGroup = 0;
		myEffect = 0;
		next = 0;
		speed = 0;
		progress = 0;
	}
	~SBlendJobNode()
	{
		if (blendType)
			blendType->Release();

		if (myEffect)
			myEffect->Release();
	}
	void Update(float frameTime)
	{
		float progressDifferential = speed * frameTime;
		progress = min(progress + progressDifferential, 1.0f);
		float point = blendType->Blend(progress);
		myEffect->Update(point);
	}
	bool Done()
	{
		return (progress == 1.0f);
	}
} SBlendJobNode;

// A blend group is a queue of blend jobs.
typedef struct SBlendGroup 
{
	SBlendGroup()
	{
		m_tail = 0;
		m_head = 0;
	}
	~SBlendGroup()
	{
		SBlendJobNode *cur = m_head;
		while (cur)
		{
			SBlendJobNode *next = cur->next;
			delete cur;
			cur = next;
		}
	}
	void Update(float frameTime)
	{
		if (m_head)
		{
			m_head->Update(frameTime);
			if (m_head->Done())
			{
				SBlendJobNode *next = m_head->next;
				if (m_head == m_tail)
					m_tail = next;
				delete m_head;
				m_head = next;
				// Call init when this effect is started
				if (m_head)
					m_head->myEffect->Init();
			}
		}
	}
	void Enqueue(SBlendJobNode *newNode)
	{
		if (m_tail)
		{
			m_tail->next = newNode;
		}
		else
		{
			m_head = newNode;
			// First effect in the list needs to be init here
			m_head->myEffect->Init();
		}
		m_tail = newNode;
	}
	bool Empty()
	{
		return (!m_head);
	}
	SBlendJobNode	*m_tail;
	SBlendJobNode	*m_head;
} SBlendGroup;

//-ScreenEffects------------------------
class CScreenEffects
{
public:
	CScreenEffects(IActor *owner);
	~CScreenEffects(void);
	// Start a blend
	virtual void StartBlend(IBlendedEffect *effect, IBlendType *blendType, float speed, int blendGroup);
	virtual bool HasJobs(int blendGroup);
	virtual int GetUniqueID();

	// Clear a blend group (deletes running blends)
	virtual void ClearBlendGroup(int blendGroup, bool resetScreen = false);
	virtual void ClearAllBlendGroups(bool resetScreen = false);

	// Reset the screen to default values
	virtual void ResetScreen();
	virtual void Update(float frameTime);
	virtual void PostUpdate(float frameTime);

	// Camera shake
	virtual void CamShake(Vec3 shiftShake, Vec3 rotateShake, float freq, float shakeTime, float randomness = 0, int shakeID = 5);

	virtual void EnableBlends(bool enable) {m_enableBlends = enable;};
	virtual void EnableBlends(bool enable, int blendGroup);

	// Update x/y coords
	virtual void SetUpdateCoords(const char *coordsXname, const char *coordsYname, Vec3 pos);

	// Util
	virtual float GetCurrentFOV();
private:
	// Maps blend group IDs to blend groups
	std::map<int, SBlendGroup*> m_blends;
	std::map<int, bool> m_enabledGroups;
	int     m_curUniqueID;
	bool    m_enableBlends;
	bool    m_updatecoords;
	string  m_coordsXname;
	string  m_coordsYname;
	Vec3    m_coords3d;
	IActor* m_ownerActor;
};
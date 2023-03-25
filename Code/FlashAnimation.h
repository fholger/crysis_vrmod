/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash animation base class

-------------------------------------------------------------------------
History:
- 02:27:2007: Created by Marco Koegler

*************************************************************************/
#ifndef __FLASHANIMATION_H__
#define __FLASHANIMATION_H__

//-----------------------------------------------------------------------------------------------------

struct IFlashPlayer;
struct SFlashVarValue;

enum EFlashDock
{
	eFD_Stretch	= (1 << 0),
	eFD_Center	= (1 << 1),
	eFD_Left		= (1 << 3),
	eFD_Right		= (1 << 4)
};

class CFlashAnimation
{
public:
	CFlashAnimation();
	virtual ~CFlashAnimation();

	IFlashPlayer*	GetFlashPlayer() const;

	void SetDock(uint32 eFDock);

	bool	LoadAnimation(const char* name);
	virtual void	Unload();
	bool	IsLoaded() const;
	void RepositionFlashAnimation();

	// these functions act on the flash player
	void SetVisible(bool visible);
	bool GetVisible() const;
	bool IsAvailable(const char* pPathToVar) const;
	bool SetVariable(const char* pPathToVar, const SFlashVarValue& value);
	bool CheckedSetVariable(const char* pPathToVar, const SFlashVarValue& value);
	bool Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);
	bool CheckedInvoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);
	// invoke helpers
	bool Invoke(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, 0, 0, pResult);
	}
	bool Invoke(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return Invoke(pMethodName, &arg, 1, pResult);
	}
	bool CheckedInvoke(const char* pMethodName, SFlashVarValue* pResult = 0)
	{
		return CheckedInvoke(pMethodName, 0, 0, pResult);
	}
	bool CheckedInvoke(const char* pMethodName, const SFlashVarValue& arg, SFlashVarValue* pResult = 0)
	{
		return CheckedInvoke(pMethodName, &arg, 1, pResult);
	}

private:

	IFlashPlayer*	m_pFlashPlayer;
	uint32	m_dock;

	// shared null player
	static IFlashPlayer*	s_pFlashPlayerNull;
};

// SUIWideString should be used when we pass sASCII+ISO Latin1 strings to Scaleform
// Scalefrom expects characters as UTF8 or wchar_t
struct SUIWideString
{
	SUIWideString(const char* sASCIIISOLatin1) { m_string.Format(L"%S", sASCIIISOLatin1); }
	const wchar_t* c_str() const { return m_string.c_str(); }
	CryFixedWStringT<128> m_string;
};

#endif //__FLASHANIMATION_H__


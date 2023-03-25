// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "HUD.h"
#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "HUDObituary.h"
#include "GameRules.h"
#include "HUDTextChat.h"
#include "PlayerInput.h"
#include "StringUtils.h"
#include "CryPath.h"
#include "IUIDraw.h"
#include "GameCVars.h"
#include "Menus/FlashMenuObject.h"

namespace NSKeyTranslation
{
	typedef CryFixedStringT<512> TFixedString;
	typedef CryFixedWStringT<512> TFixedWString;

	static const int MAX_KEYS = 3;
	SActionMapBindInfo gBindInfo = { 0, 0, 0 };

	// truncate wchar_t to char
	template<class T> void TruncateToChar(const wchar_t* wcharString, T& outString)
	{
		outString.resize(TFixedWString::_strlen(wcharString));
		char* dst = outString.begin();
		const wchar_t* src = wcharString;

		while (char c=(char)(*src++ & 0xFF))
		{
			*dst++ = c;
		}
	}

	// simple expand wchar_t to char
	template<class T> void ExpandToWChar(const char* charString, T& outString)
	{
		outString.resize(TFixedString::_strlen(charString));
		wchar_t* dst = outString.begin();
		const char* src = charString;
		while (const wchar_t c=(wchar_t)(*src++))
		{
			*dst++ = c;
		}
	}


	bool LookupBindInfo(const char* actionMap, const char* actionName, bool bPreferXI, SActionMapBindInfo& bindInfo)
	{
		if (bindInfo.keys == 0)
		{
			bindInfo.keys = new const char*[MAX_KEYS];
			for (int i=0; i<MAX_KEYS; ++i)
				bindInfo.keys[i] = 0;
		}
	
		IActionMapManager* pAmMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
		if (pAmMgr == 0)
			return false;
		IActionMap* pAM = pAmMgr->GetActionMap(actionMap);
		if (pAM == 0)
			return false;

		ActionId actionId (actionName);
		bool bFound = pAM->GetBindInfo(actionId, bindInfo, MAX_KEYS);

		// if no XI lookup required -> return result
		if (!bPreferXI)
			return bFound;

		// it's either found or not found, but XI needs to be looked up anyway

		// we found it, so now look if there are xi keys in
		if (bFound)
		{
			for (int i = 0; i<bindInfo.nKeys; ++i)
			{
				const char* keyName = bindInfo.keys[i];
				if (keyName && keyName[0] == 'x' && keyName[1] == 'i' && keyName[2] == '_')
				{
					// ok, this is an xi key, put it into front of list and return
					std::swap(bindInfo.keys[0], bindInfo.keys[i]);
					return true;
				}
			}
		}

		// we didn't find an XI key in the same action, so use for an action named xi_actionName
		CryFixedStringT<64> xiActionName ("xi_");
		xiActionName+=actionName;
		bFound = pAM->GetBindInfo(ActionId(xiActionName.c_str()), bindInfo, MAX_KEYS);
		if (bFound) // ok, we found an xi action
			return true;

		// no, we didn't find an xi key nor an xi action, re-do first lookup and return
		bFound = pAM->GetBindInfo(actionId, bindInfo, MAX_KEYS);
		return bFound;
	}	

	bool LookupBindInfo(const wchar_t* actionMap, const wchar_t* actionName, bool bPreferXI, SActionMapBindInfo& bindInfo)
	{
		CryFixedStringT<64> actionMapString;
		TruncateToChar(actionMap, actionMapString);
		CryFixedStringT<64> actionNameString;
		TruncateToChar(actionName, actionNameString);
		return LookupBindInfo(actionMapString.c_str(), actionNameString.c_str(), bPreferXI, bindInfo);
	}

	template<class T> 
	void InsertString(T& inString, size_t pos, const wchar_t* s)
	{
		inString.insert(pos, s);
	}

	template<size_t S> 
	void InsertString(CryFixedWStringT<S>& inString, size_t pos, const char* s)
	{
		CryFixedWStringT<64> wcharString;
		ExpandToWChar(s, wcharString);
		inString.insert(pos, wcharString.c_str());
	}

	/*
	template<size_t S>
	void InsertString(CryFixedStringT<S>& inString, size_t pos, const wchar_t* s)
	{
		CryFixedStringT<64> charString;
		TruncateToChar(s, charString);
		inString.insert(pos, charString.c_str());
	}

	template<size_t S>
	void InsertString(CryFixedStringT<S>& inString, size_t pos, const char* s)
	{
		inString.insert(pos, s);
	}
	*/

	// Replaces all occurrences of %[actionmap:actionid] with the current key binding
	template<size_t S>
	void ReplaceActions(ILocalizationManager* pLocMgr, CryFixedWStringT<S>& inString)
	{
		typedef CryFixedWStringT<S> T;
		static const typename T::value_type* actionPrefix = L"%[";
		static const size_t actionPrefixLen = T::_strlen(actionPrefix);
		static const typename T::value_type actionDelim = L':';
		static const typename T::value_type actionDelimEnd = L']';
		static const char* keyPrefix = "@cc_"; // how keys appear in the localization sheet
		static const size_t keyPrefixLen = strlen(keyPrefix);
		CryFixedStringT<32> fullKeyName;
		static wstring realKeyName;

		size_t pos = inString.find(actionPrefix, 0);
		const bool bPreferXI = g_pGame && g_pGame->GetMenu() && g_pGame->GetMenu()->IsControllerConnected() ? true : false;

		while (pos != T::npos)
		{
			size_t pos1 = inString.find(actionDelim, pos+actionPrefixLen);
			if (pos1 != T::npos)
			{
				size_t pos2 = inString.find(actionDelimEnd, pos1+1);
				if (pos2 != T::npos)
				{
					// found end of action descriptor
					typename T::value_type* t1 = inString.begin()+pos1;
					typename T::value_type* t2 = inString.begin()+pos2;
					*t1 = 0;
					*t2 = 0;
					const typename T::value_type* actionMapName = inString.begin()+pos+actionPrefixLen;
					const typename T::value_type* actionName = inString.begin()+pos1+1;
					// CryLogAlways("Found: '%S' '%S'", actionMapName, actionName);
					bool bFound = LookupBindInfo(actionMapName, actionName, bPreferXI, gBindInfo);
					*t1 = actionDelim; // re-replace ':'
					*t2 = actionDelimEnd; // re-replace ']'
					if (bFound && gBindInfo.nKeys >= 1)
					{
						inString.erase(pos, pos2-pos+1);
						const char* keyName = gBindInfo.keys[0]; // first key
						fullKeyName.assign(keyPrefix, keyPrefixLen);
						fullKeyName.append(keyName);
						bFound = pLocMgr->LocalizeLabel(fullKeyName.c_str(), realKeyName);
						if (bFound)
						{
							InsertString(inString, pos, realKeyName);
						}
						else
						{
							// not found, insert original (untranslated name from actionmap)
							InsertString(inString, pos, keyName);
						}
					}
				}
			}
			pos = inString.find(actionPrefix, pos+1);
		}
	}
}; // namespace NSKeyTranslation

const wchar_t*
CHUD::LocalizeWithParams(const char* label, bool bAdjustActions, const char* param1, const char* param2, const char* param3, const char* param4)
{
	static NSKeyTranslation::TFixedWString finalLocalizedString;

	if (label[0] == '@')
	{
		ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
		wstring localizedString, finalString;
		pLocMgr->LocalizeLabel(label, localizedString);
		const bool bFormat = param1 || param2 || param3 || param4;
		if(bFormat)
		{
			wstring p1, p2, p3, p4;
			if(param1)
			{
				if (param1[0] == '@')
					pLocMgr->LocalizeLabel(param1, p1);
				else
					NSKeyTranslation::ExpandToWChar(param1, p1);
			}
			if(param2)
			{
				if (param2[0] == '@')
					pLocMgr->LocalizeLabel(param2, p2);
				else
					NSKeyTranslation::ExpandToWChar(param2, p2);
			}
			if(param3)
			{
				if (param3[0] == '@')
					pLocMgr->LocalizeLabel(param3, p3);
				else
					NSKeyTranslation::ExpandToWChar(param3, p3);
			}
			if(param4)
			{
				if (param4[0] == '@')
					pLocMgr->LocalizeLabel(param4, p4);
				else
					NSKeyTranslation::ExpandToWChar(param4, p4);
			}
			pLocMgr->FormatStringMessage(finalString, localizedString, 
				p1.empty()?0:p1.c_str(),
				p2.empty()?0:p2.c_str(),
				p3.empty()?0:p3.c_str(),
				p4.empty()?0:p4.c_str());
		}
		else
			finalString = localizedString;

		finalLocalizedString.assign(finalString.c_str(), finalString.length());
		if (bAdjustActions)
		{
			NSKeyTranslation::ReplaceActions(pLocMgr, finalLocalizedString);
		}
	}
	else
	{
		// we expand always to wchar_t, as Flash will translate into wchar anyway
		NSKeyTranslation::ExpandToWChar(label, finalLocalizedString);
		// in non-localized case replace potential line-breaks
		finalLocalizedString.replace(L"\\n",L"\n");
		if (bAdjustActions)
		{
			ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
			NSKeyTranslation::ReplaceActions(pLocMgr, finalLocalizedString);
		}
	}
	return finalLocalizedString.c_str();
}


void CHUD::DisplayFlashMessage(const char* label, int pos /* = 1 */, const ColorF &col /* = Col_White */, bool formatWStringWithParams /* = false */, const char* paramLabel1 /* = 0 */, const char* paramLabel2 /* = 0 */, const char* paramLabel3 /* = 0 */, const char* paramLabel4 /* = 0 */)
{
	if(!label || m_quietMode)
		return;

	unsigned int packedColor = col.pack_rgb888();

	if (pos < 1 || pos > 4)
		pos = 1;

	if(pos == 2 && m_fMiddleTextLineTimeout <= 0.0f)
		m_fMiddleTextLineTimeout = gEnv->pTimer->GetFrameStartTime().GetSeconds() + 3.0f;

	const wchar_t* localizedText = L"";
	if(formatWStringWithParams)
		localizedText = LocalizeWithParams(label, true, paramLabel1, paramLabel2, paramLabel3, paramLabel4);
	else
		localizedText = LocalizeWithParams(label, true);

	if(m_animSpectate.GetVisible())
		m_animSpectate.Invoke("setInfoText", localizedText);
	else
	{
		SFlashVarValue args[3] = {localizedText, pos, packedColor};
		m_animMessages.Invoke("setMessageText", args, 3);
	}
}

void CHUD::DisplayOverlayFlashMessage(const char* label, const ColorF &col /* = Col_White */, bool formatWStringWithParams /* = false */, const char* paramLabel1 /* = 0 */, const char* paramLabel2 /* = 0 */, const char* paramLabel3 /* = 0 */, const char* paramLabel4 /* = 0 */)
{
	if(!label)
		return;

	unsigned int packedColor = col.pack_rgb888();

	if(m_fOverlayTextLineTimeout <= 0.0f)
		m_fOverlayTextLineTimeout = gEnv->pTimer->GetFrameStartTime().GetSeconds() + 3.0f;

	const wchar_t* localizedText = L"";
	if(formatWStringWithParams)
		localizedText = LocalizeWithParams(label, true, paramLabel1, paramLabel2, paramLabel3, paramLabel4);
	else
		localizedText = LocalizeWithParams(label, true);

	SFlashVarValue args[3] = {localizedText, 2, packedColor}; // hard-coded pos 2 = middle
	m_animOverlayMessages.Invoke("setMessageText", args, 3);

	if (localizedText && *localizedText == 0)
		m_animOverlayMessages.SetVisible(false);
	else
		if (m_animOverlayMessages.GetVisible() == false)
			m_animOverlayMessages.SetVisible(true);
}

void CHUD::FadeOutBigOverlayFlashMessage()
{
	if (m_bigOverlayText.empty() == false && m_animBigOverlayMessages.GetVisible())
	{
		m_fBigOverlayTextLineTimeout = -1.0f; // this will trigger fade-out of current text
		m_bigOverlayText.assign("");
	}
}

void CHUD::DisplayBigOverlayFlashMessage(const char* label, float duration, int posX, int posY, ColorF col)
{
	if(!label)
		return;

	unsigned int packedColor = col.pack_rgb888();

	const float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	if(duration <= 0.0f)
		m_fBigOverlayTextLineTimeout = 0.0f;
	else
		m_fBigOverlayTextLineTimeout = now + duration;

	const wchar_t* localizedText = L"";
	if (label && *label)
	{
		bool bFound = false;
		// see, if a gamepad is connected, then use gamepad specific messages if available
		const bool bLookForController = g_pGame->GetMenu() && g_pGame->GetMenu()->IsControllerConnected();
		if (bLookForController && label[0] == '@')
		{
			// look for a xi_label key
			CryFixedStringT<128> gamePadLabel ("@GamePad_");
			gamePadLabel += (label+1); // skip @
			ILocalizationManager::SLocalizedInfo tempInfo;
			// looking up the key (without @ sign)
			bFound = gEnv->pSystem->GetLocalizationManager()->GetLocalizedInfo(gamePadLabel.c_str()+1, tempInfo);
			if (bFound)
			{
				// this one needs the @ sign in front
				localizedText = LocalizeWithParams(gamePadLabel.c_str(), true);
			}
		}

		if (!bFound)
			localizedText = LocalizeWithParams(label, true);
	}

	SFlashVarValue pos[2] = {posX*1024/800, posY*768/512};
	m_animBigOverlayMessages.Invoke("setPosition", pos, 2);

	// Ok this is a big workaround, so here is an explanation.
	// A flow graph node using that function has been created to take coordinates relative to (800,600)
	// First problem: 512 has been used instead of 600, certainly as a bad copy/paste or as a temporary fix
	// and the node stayed like this at the point it couldn't be changed (it was already used everywhere "as this")
	// Second problem: in aspect ratio like 5/4 or 16/9, it does not work as the flash animation (and thus text) is clipped or badly placed.
	// Solution:
	// When the animation is used for a tutorial text (posX==400), we do nothing, it should work in all resolutions.
	// When the animation is used for a chapter title (posX<100, at least we hope!), we just move the animation in the bottom left corner.
	const char *szTextAlignment = NULL;
	if(posX<100)
	{
		m_animBigOverlayMessages.SetDock(eFD_Left);
		szTextAlignment = "left";
	}
	else
	{
		m_animBigOverlayMessages.SetDock(eFD_Center);
		szTextAlignment = "center";
	}
	m_animBigOverlayMessages.RepositionFlashAnimation();
	// End of the big workaround

	SFlashVarValue args[4] = {localizedText, 2, packedColor, szTextAlignment}; // hard-coded pos 2 = middle
	m_animBigOverlayMessages.Invoke("setMessageText", args, 4);

	if (localizedText && *localizedText == 0)
	{
		if (m_bigOverlayText.empty())
			m_animBigOverlayMessages.SetVisible(false);
		else
			m_fBigOverlayTextLineTimeout = -1.0f; // this will trigger fade-out of current text
	}
	else
		if (m_animBigOverlayMessages.GetVisible() == false)
			m_animBigOverlayMessages.SetVisible(true);
	m_bigOverlayText = label;
	m_bigOverlayTextColor = col;
	m_bigOverlayTextX = posX;
	m_bigOverlayTextY = posY;
}

void CHUD::DisplayTempFlashText(const char* label, float seconds, const ColorF &col)
{
	if(seconds > 60.0f)
		seconds = 60.0f;
	m_fMiddleTextLineTimeout = gEnv->pTimer->GetFrameStartTime().GetSeconds() + seconds;
	DisplayFlashMessage(label, 2, col);
}

void CHUD::SetQuietMode(bool enabled)
{
	m_quietMode = enabled;
}

void CHUD::BattleLogEvent(int type, const char *msg, const char *p0, const char *p1, const char *p2, const char *p3)
{
	wstring localizedString, finalString;
	ILocalizationManager *pLocalizationMan = gEnv->pSystem->GetLocalizationManager();
	pLocalizationMan->LocalizeString(msg, localizedString);

	if (p0)
	{
		wstring p0localized;
		pLocalizationMan->LocalizeString(p0, p0localized);

		wstring p1localized;
		if (p1)
			pLocalizationMan->LocalizeString(p1, p1localized);

		wstring p2localized;
		if (p2)
			pLocalizationMan->LocalizeString(p2, p2localized);

		wstring p3localized;
		if (p3)
			pLocalizationMan->LocalizeString(p3, p3localized);

		pLocalizationMan->FormatStringMessage(finalString, localizedString,
			p0localized.empty()?0:p0localized.c_str(),
			p1localized.empty()?0:p1localized.c_str(),
			p2localized.empty()?0:p2localized.c_str(),
			p3localized.empty()?0:p3localized.c_str());
	}
	else
		finalString=localizedString;

	const static int maxCharsInBattleLogLine = 50;
	int numLines = 1 + (finalString.length() / maxCharsInBattleLogLine);
	if(numLines > 1)
	{
		wstring partStringA = finalString.substr(0, maxCharsInBattleLogLine);
		wstring partStringB = finalString.substr(maxCharsInBattleLogLine, finalString.size());
		SFlashVarValue argsA[2] = {partStringA.c_str(), type};
		m_animBattleLog.Invoke("setMPLogText", argsA, 2);
		SFlashVarValue argsB[2] = {partStringB.c_str(), 0};
		m_animBattleLog.Invoke("setMPLogText", argsB, 2);
	}
	else
	{
		SFlashVarValue args[2] = {finalString.c_str(), type};
		m_animBattleLog.Invoke("setMPLogText", args, 2);
	}
}

const char *CHUD::GetPlayerRank(EntityId playerId, bool shortName)
{
	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		int rank=0;

		if(IScriptTable *pGameRulesTable=pGameRules->GetEntity()->GetScriptTable())
		{
			HSCRIPTFUNCTION pfnGetPlayerRank=0;
			if(pGameRulesTable->GetValue("GetPlayerRank", pfnGetPlayerRank) && pfnGetPlayerRank)
			{
				Script::CallReturn(gEnv->pScriptSystem,pfnGetPlayerRank,pGameRulesTable,ScriptHandle(playerId), rank);
				gEnv->pScriptSystem->ReleaseFunc(pfnGetPlayerRank);
			}
		}

		static string rankFormatter;
		if(rank)
		{
			if (!shortName)
				rankFormatter.Format("@ui_rank_%d", rank);
			else
				rankFormatter.Format("@ui_short_rank_%d", rank);
			return rankFormatter.c_str();
		}
	}

	return 0;
}

// returns ptr to static string buffer.
const char* GetSoundKey(const char* soundName)
{
	static string buf;
	static const char* prefix = "Languages/dialog/";
	static const int prefixLen = strlen(prefix);

	buf.assign("@");
	// check if it already starts Languages/dialog. then replace it
	if (CryStringUtils::stristr(soundName, prefix) == soundName)
	{
		buf.append (soundName+prefixLen);
	}
	else
	{
		buf.append (soundName);
	}
	PathUtil::RemoveExtension(buf);
	return buf.c_str();
}


void CHUD::ShowSubtitle(ISound *pSound, bool bShow)
{
	assert (pSound != 0);
	if (pSound == 0)
		return;

	const char* soundKey = GetSoundKey(pSound->GetName());
	InternalShowSubtitle(soundKey, pSound, bShow);
}

void CHUD::ShowSubtitle(const char* subtitleLabel, bool bShow)
{
	InternalShowSubtitle(subtitleLabel, 0, bShow);
}

void CHUD::SubtitleCreateChunks(CHUD::SSubtitleEntry& entry, const wstring& localizedString)
{
	// no time, no chunks
	if (entry.timeRemaining <= 0.0f)
		return;

	// look for tokens
	const wchar_t token[] = { L"##" };
	const size_t tokenLen = (sizeof(token) / sizeof(token[0])) - 1;

	size_t len = localizedString.length();
	size_t startPos = 0;
	size_t pos = localizedString.find(token, 0);
	size_t nChunks = 0;
	size_t MAX_CHUNKS = 10;

	static const bool bIgnoreSpecialCharsAfterToken = true;

	while (pos != wstring::npos)
	{
		SSubtitleEntry::Chunk& chunk = entry.chunks[nChunks];
		chunk.start = startPos;
		chunk.len = pos-startPos;
		++nChunks;

		if (nChunks == MAX_CHUNKS-1)
		{
			GameWarning("CHUD::SubtitleCreateChunks: Localization Entry '%s' exceeds max. number of chunks [%d]", entry.key.c_str(), MAX_CHUNKS);
			break;
		}
		startPos = pos+tokenLen;
		if (bIgnoreSpecialCharsAfterToken)
		{
			// currently ignore line-breaks and spaces after token ##
			size_t found = localizedString.find_first_not_of(L"\n ", startPos);
			startPos = found != wstring::npos ? found : startPos;
		}
		pos = localizedString.find(token, startPos);
	}


	// care about the last one, but only if we found at least one
	// otherwise there is no splitter at all, and it's only one chunk
	if (nChunks > 0)
	{
		{
			SSubtitleEntry::Chunk& chunk = entry.chunks[nChunks];
			chunk.start = startPos;
			chunk.len = len-startPos;
		}
		++nChunks;

		// now we have the total number of chunks, calc the string length without tokens
		size_t realCharLength = len - (nChunks-1) * tokenLen;
		float time = entry.timeRemaining;
		for (size_t i=0; i<nChunks; ++i)
		{
			SSubtitleEntry::Chunk& chunk = entry.chunks[i];
			chunk = entry.chunks[i];
			size_t realPos = chunk.start - i * tokenLen; // calculated with respect to realCharLength
			float pos = (float) realPos / (float) realCharLength; // pos = [0,1]
			chunk.time = time - time * pos; // we put in the remaining time
			if (g_pGameCVars->hud_subtitlesDebug)
			{
				wstring s = localizedString.substr(chunk.start, chunk.len);
				CryLogAlways("[SUB] %s chunk=%d time=%f '%S'", entry.key.c_str(), i, chunk.time, s.c_str());
			}
		}

		entry.localized = localizedString;
		entry.nCurChunk = -1;
	}

	entry.nChunks = nChunks;

	/*
	static const bool bDebug = true;
	if (bDebug && entry.nChunks > 0)
	{
		CryLogAlways("Key: %s SoundLength=%f NumChunks=%d", entry.key.c_str(), entry.timeRemaining, entry.nChunks);
		CryFixedWStringT<128> tmp;
		for (size_t i=0; i<entry.nChunks; ++i)
		{
			SSubtitleEntry::Chunk& chunk = entry.chunks[i];
			tmp.assign(entry.localized.c_str(), chunk.start, chunk.len);
			CryLogAlways("Chunk %d: Time=%f S=%d L=%d Text=%S", i, chunk.time, chunk.start, chunk.len, tmp.c_str());
		}
	}
	*/

}

namespace
{
	// language specific rules for CharacterName : Text
	struct LanguageCharacterPrefix
	{
		const char* language;
		const wchar_t* prefix;
	};

	LanguageCharacterPrefix g_languageCharacterPrefixes[] =
	{
		{ "French", L" : " },
	};
	size_t g_languageCharacterPrefixesCount = sizeof(g_languageCharacterPrefixes) / sizeof(g_languageCharacterPrefixes[0]);
};

void CHUD::SubtitleAssignCharacterName(CHUD::SSubtitleEntry& entry)
{
	if (g_pGame->GetCVars()->hud_subtitlesShowCharName)
	{
		const char* subtitleLabel = entry.key.c_str();
		ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
		ILocalizationManager::SLocalizedInfo locInfo;
		const char* key = (*subtitleLabel == '@') ? subtitleLabel+1 : subtitleLabel;
		if (pLocMgr->GetLocalizedInfo(key, locInfo) == true && locInfo.sWho && *locInfo.sWho)
		{
			entry.localized.assign(locInfo.sWho);
			const wchar_t* charPrefix = L": ";

			// assign language specific character name prefix
			const char* currentLanguage = pLocMgr->GetLanguage();
			for (int i = 0; i < g_languageCharacterPrefixesCount; ++i)
			{
				if (stricmp(g_languageCharacterPrefixes[i].language, currentLanguage) == 0)
				{
					charPrefix = g_languageCharacterPrefixes[i].prefix;
					break;
				}
			}
			entry.localized.append(charPrefix);
		}
	}
}

void CHUD::SubtitleAppendCharacterName(const CHUD::SSubtitleEntry& entry, CryFixedWStringT<1024>& locString)
{
	if (g_pGame->GetCVars()->hud_subtitlesShowCharName)
	{
		const char* subtitleLabel = entry.key.c_str();
		ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
		ILocalizationManager::SLocalizedInfo locInfo;
		const char* key = (*subtitleLabel == '@') ? subtitleLabel+1 : subtitleLabel;
		if (pLocMgr->GetLocalizedInfo(key, locInfo) == true && locInfo.sWho && *locInfo.sWho)
		{
			locString.append(locInfo.sWho);
			const wchar_t* charPrefix = L": ";

			// assign language specific character name prefix
			const char* currentLanguage = pLocMgr->GetLanguage();
			for (int i = 0; i < g_languageCharacterPrefixesCount; ++i)
			{
				if (stricmp(g_languageCharacterPrefixes[i].language, currentLanguage) == 0)
				{
					charPrefix = g_languageCharacterPrefixes[i].prefix;
					break;
				}
			}
			locString.append(charPrefix);
		}
	}
}

void CHUD::InternalShowSubtitle(const char* subtitleLabel, ISound* pSound, bool bShow)
{
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
	if (bShow)
	{
		TSubtitleEntries::iterator iter = std::find(m_subtitleEntries.begin(), m_subtitleEntries.end(), subtitleLabel);
		if (iter == m_subtitleEntries.end())
		{
			wstring localizedString;
			const bool bFound = pLocMgr->GetSubtitle(subtitleLabel, localizedString);
			if (bFound)
			{
				SSubtitleEntry entry;
				entry.key = subtitleLabel;
				if (pSound)
				{
					entry.soundId = pSound->GetId();
					float timeToShow = pSound->GetLengthMs() * 0.001f; // msec to sec
					if (g_pGameCVars->hud_subtitlesDebug)
					{
						float now = gEnv->pTimer->GetCurrTime();
						CryLogAlways("[SUB] Sound %s started at %f for %f secs, endtime=%f", pSound->GetName(), now, timeToShow, now+timeToShow);
					}
#if 1 // make the text stay longer than the sound
					// timeToShow = std::min(timeToShow*1.2f, timeToShow+2.0f); // 10 percent longer, but max 2 seconds
#endif
					entry.timeRemaining = timeToShow;
					entry.bPersistant = false;
				}
				else
				{
					entry.soundId = INVALID_SOUNDID;
					entry.timeRemaining = 0.0f;
					entry.bPersistant = true;
				}

				// replace actions
				NSKeyTranslation::TFixedWString finalDisplayString;
				finalDisplayString.assign(localizedString, localizedString.length());
				NSKeyTranslation::ReplaceActions(pLocMgr, finalDisplayString);

				if (pSound)
					SubtitleCreateChunks(entry, localizedString);

				// if we have no chunks
				if (entry.nChunks == 0)
				{
					entry.timeRemaining = std::max(entry.timeRemaining, 0.8f); // minimum is 0.8 seconds
					// entry.timeRemaining = std::min(entry.timeRemaining*1.3f, entry.timeRemaining+2.5f); // 30 percent longer, but max 2.5 seconds
					SubtitleAssignCharacterName(entry);
					entry.localized.append(localizedString.c_str(), localizedString.length());
				}

				static const bool bSubtitleLastIsFirst = false;
				const size_t MAX_SUBTITLES = g_pGameCVars->hud_subtitlesQueueCount;
				if (bSubtitleLastIsFirst)
				{
					m_subtitleEntries.push_front(entry);
					if (m_subtitleEntries.size() > MAX_SUBTITLES)
						m_subtitleEntries.resize(MAX_SUBTITLES);
				}
				else
				{
					m_subtitleEntries.push_back(entry);
					if (m_subtitleEntries.size() > MAX_SUBTITLES)
						m_subtitleEntries.erase(m_subtitleEntries.begin());
				}
				m_bSubtitlesNeedUpdate = true;
			}
		}
	}
	else // if (bShow)
	{
		tSoundID soundId = pSound ? pSound->GetId() : INVALID_SOUNDID;

		if (pSound && g_pGameCVars->hud_subtitlesDebug)
		{
			float now = gEnv->pTimer->GetCurrTime();
			CryLogAlways("[SUB] Sound %s requested to stop at %f", pSound->GetName(), now);
		}

		for (TSubtitleEntries::iterator iter = m_subtitleEntries.begin(); iter != m_subtitleEntries.end(); )
		{
			const SSubtitleEntry& entry = *iter;
			// subtitles without associated sound are erase
			// subtitles with associated sound are only erased if they contain chunks
			// non-chunked sounds timeout, when the sound's time has passed 
			// this may introduce some subtitles staying longer on screen than the sound can be heard, but is ok I think
			// [e.g. the sound is cancelled in the middle]
			if (iter->key == subtitleLabel && (soundId == INVALID_SOUNDID || (iter->soundId == soundId && iter->nChunks > 0 && iter->nCurChunk < iter->nChunks-1 && iter->timeRemaining > 0.8f)))
			{
				if (g_pGameCVars->hud_subtitlesDebug)
				{
					float now = gEnv->pTimer->GetCurrTime();
					CryLogAlways("[SUB] Entry '%s' stopped at %f", entry.key.c_str(), now);
				}
				iter = m_subtitleEntries.erase(iter);
				m_bSubtitlesNeedUpdate = true;
			}
			else
			{
				++iter;
			}
		}
	}
}

void CHUD::SetRadioButtons(bool active, int buttonNo /* = 0 */)
{
	if(active)
	{
		if(GetModalHUD() == &m_animPDA)
			ShowPDA(false);
		else if(GetModalHUD() == &m_animBuyMenu)
			ShowPDA(false, true);

		m_animRadioButtons.Invoke("showRadioButtons", buttonNo);
		m_animRadioButtons.SetVisible(true);
		wstring group0(LocalizeWithParams("@mp_radio_group_0"));
		wstring group1(LocalizeWithParams("@mp_radio_group_1"));
		wstring group2(LocalizeWithParams("@mp_radio_group_2"));
		wstring group3(LocalizeWithParams("@mp_radio_group_3"));
		SFlashVarValue args[4] = {group0.c_str(), group1.c_str(), group2.c_str(), group3.c_str()};
		m_animRadioButtons.Invoke("setRadioButtonText", args, 4);
	}
	else
	{
		m_animRadioButtons.Invoke("showRadioButtons", 0);
		m_animRadioButtons.SetVisible(false);
	}
}

void CHUD::ShowGamepadConnected(bool active)
{
	if(ICVar* v = gEnv->pConsole->GetCVar("i_xinput"))
	{
		if(v->GetIVal()>0)
		{
			m_animGamepadConnected.Reload();
			m_animGamepadConnected.Invoke("GamepadAvailable", active);
		}
	}		
}

void CHUD::ObituaryMessage(EntityId targetId, EntityId shooterId, const char *weaponClassName, int material, int hit_type)
{
	CGameRules *pGameRules=g_pGame->GetGameRules();
	if (!pGameRules)
		return;

	if(!m_animKillLog.IsLoaded())
		return;

	ILocalizationManager *pLM=gEnv->pSystem->GetLocalizationManager();

	// TODO: refactor by Jan N

	bool bMounted = false;
	bool bSuicide = false;
	bool bTurret = false;
	if (targetId == shooterId)
		bSuicide = true;

	if(!stricmp(weaponClassName, "AutoTurret"))
	{
		bTurret = true;
	}

	// code below is checking if hits are melee and headshot...
	// TODO: Jan N: use these bools
	bool melee=false;
	if (hit_type>0)
	{
		const char *hittypename=pGameRules->GetHitType(hit_type);
		melee=strstr(hittypename?hittypename:"", "melee") != 0;
	}

	bool headshot=false;
	if (material>0)
	{
		if (ISurfaceType *pSurfaceType=pGameRules->GetHitMaterial(material))
		{
			const char *matname=pSurfaceType->GetName();
			headshot=strstr(matname?matname:"", "head") != 0;
		}
	}

	const char *targetName=g_pGame->GetGameRules()->GetActorNameByEntityId(targetId);
	const char *shooterName=g_pGame->GetGameRules()->GetActorNameByEntityId(shooterId);
	wstring entity;

	SUIWideString shooter(shooterName);
	SUIWideString target(targetName);

	int shooterFriendly = 0;
	int targetFriendly = 0;

	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		if(pGameRules->GetEntity() && pGameRules->GetEntity()->GetClass() && pGameRules->GetEntity()->GetClass()->GetName() && !stricmp(pGameRules->GetEntity()->GetClass()->GetName(),"PowerStruggle"))
		{
			int ownteam = pGameRules->GetTeam(g_pGame->GetIGameFramework()->GetClientActorId());
			if(shooterId)
			{
				int team = pGameRules->GetTeam(shooterId);
				if(team!=0)
				{
					if(team==ownteam)
						shooterFriendly = 1;
					else
						shooterFriendly = 2;
				}    
			}
			if(targetId)
			{
				int team = pGameRules->GetTeam(targetId);
				if(team!=0)
				{
					if(team==ownteam)
						targetFriendly = 1;
					else
						targetFriendly = 2;
				}
			}
		}
	}

	bool processed = false;

	IEntityClass* pWeaponClass = NULL;

	pWeaponClass=gEnv->pEntitySystem->GetClassRegistry()->FindClass(weaponClassName);

	if(pWeaponClass == CItem::sSOCOMClass)
	{
		CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId));
		if(pPlayer)
		{
			if(pPlayer->GetCurrentItem() && pPlayer->GetCurrentItem()->IsDualWield())
			{
				pLM->LocalizeString("doubleSOCOM", entity, true);
				processed = true;
			}
		}
	}

	if (!processed)
	{
		if(pWeaponClass)
			pLM->LocalizeString(pWeaponClass->GetName(),entity, true);
		else
			pLM->LocalizeString(weaponClassName, entity, true);
	}

	// if there is no shooter, use the suicide icon
	if ((!shooterName || !shooterName[0]) && !g_pGame->GetIGameFramework()->GetIItemSystem()->IsItemClass(weaponClassName))
		bSuicide=true;
		
	if(bSuicide)
	{
		SFlashVarValue args[6] = {"", "Suicide", target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
	else if(bTurret)
	{
		SFlashVarValue args[6] = {"", "AutoTurret", target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
	else if(g_pGame->GetIGameFramework()->GetIVehicleSystem()->IsVehicleClass(weaponClassName))
	{
		SFlashVarValue args[6] = {shooter.c_str(), "RunOver", target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
	else if(bMounted)
	{
		SFlashVarValue args[6] = {shooter.c_str(), "Mounted", target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
	else if(melee)
	{
		SFlashVarValue args[6] = {shooter.c_str(), "Melee", target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
	else
	{
		SFlashVarValue args[6] = {shooter.c_str(), entity.c_str(), target.c_str(), headshot, shooterFriendly, targetFriendly};
		m_animKillLog.Invoke("addLog",args,6);
	}
}

void CHUD::ShowWarningMessage(EWarningMessages message, const char* optionalText)
{
	switch(message)
	{
	case EHUD_SPECTATOR:
		m_animWarningMessages.Invoke("showErrorMessage", "spectator");
		break;
	case EHUD_SWITCHTOTAN:
		m_animWarningMessages.Invoke("showErrorMessage", "switchtotan");
		break;
	case EHUD_SWITCHTOBLACK:
		m_animWarningMessages.Invoke("showErrorMessage", "switchtous");
		break;
	case EHUD_SUICIDE:
		m_animWarningMessages.Invoke("showErrorMessage", "suicide");
		break;
	case EHUD_CONNECTION_LOST:
		m_animWarningMessages.Invoke("showErrorMessage", "connectionlost");
		break;
	case EHUD_OK:
		m_animWarningMessages.Invoke("showErrorMessage", "Box1");
		if(optionalText)
			m_animWarningMessages.Invoke("setErrorText", optionalText);
		break;
	case EHUD_YESNO:
		m_animWarningMessages.Invoke("showErrorMessage", "Box2");
		if(optionalText)
			m_animWarningMessages.Invoke("setErrorText", optionalText);
		break;
	case EHUD_CANCEL:
		m_animWarningMessages.Invoke("showErrorMessage", "Box3");
		if(optionalText)
			m_animWarningMessages.Invoke("setErrorText", optionalText);
		break;
	default:
		return;
		break;
	}

	m_animWarningMessages.SetVisible(true);

	if(m_pModalHUD == &m_animPDA)
		ShowPDA(false);
	SwitchToModalHUD(&m_animWarningMessages,true);
	CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer && pPlayer->GetPlayerInput())
		pPlayer->GetPlayerInput()->DisableXI(true);
}

void CHUD::HandleWarningAnswer(const char* warning /* = NULL */)
{
	m_animWarningMessages.SetVisible(false);
	CPlayer *pPlayer = static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetClientActor());

	if(warning)
	{
		if(!strcmp(warning, "suicide"))
		{
			//SwitchToModalHUD(NULL,false);
			gEnv->pConsole->ExecuteString("kill me");
		}
		else if(!strcmp(warning, "spectate"))
		{
			ShowPDA(false);
			if(m_pHUDTextChat)
				m_pHUDTextChat->CloseChat();
			gEnv->pConsole->ExecuteString("spectator");
		}
		else if(!strcmp(warning, "switchTeam"))
		{
			CGameRules* pRules = g_pGame->GetGameRules();
			if(pRules->GetTeamCount() > 1)
			{
				const char* command = "team black";
				if(pRules->GetTeamId("black") == pRules->GetTeam(pPlayer->GetEntityId()))
					command = "team tan";
				gEnv->pConsole->ExecuteString(command);
			}
		}
	}

	SwitchToModalHUD(NULL,false);
	if(pPlayer && pPlayer->GetPlayerInput())
		pPlayer->GetPlayerInput()->DisableXI(false);
}


void CHUD::UpdateSubtitlesManualRender(float frameTime)
{
	if (m_subtitleEntries.empty() == false)
	{
		for (TSubtitleEntries::iterator iter = m_subtitleEntries.begin(); iter != m_subtitleEntries.end();)
		{
			SSubtitleEntry& entry = *iter;

			// chunk handling
			if (entry.nChunks > 0)
			{
				if (entry.nCurChunk < 0)
				{
					// first time
					entry.nCurChunk = 0;
					m_bSubtitlesNeedUpdate = true;
				}
				else if (entry.nCurChunk < entry.nChunks-1)
				{
					SSubtitleEntry::Chunk& nextChunk = entry.chunks[entry.nCurChunk+1];
					if (entry.timeRemaining <= nextChunk.time)
					{
						++entry.nCurChunk;
						m_bSubtitlesNeedUpdate = true;
						if (entry.nCurChunk == entry.nChunks-1)
						{
							// last chunk, if that's too small, make it a bit longer
							if (entry.timeRemaining < .8f)
							{
								entry.timeRemaining = 0.8f;
								if (g_pGameCVars->hud_subtitlesDebug)
								{
									float now = gEnv->pTimer->GetCurrTime();
									CryLogAlways("[SUB] Chunked entry '%s' last chunk end delayed by 0.8 secs [now=%f]", entry.key.c_str(), now);
								}
							}
						}
					}
				}
			}
			//

			if (entry.bPersistant == false)
			{
				entry.timeRemaining -= frameTime;
				const bool bDelete = entry.timeRemaining <= 0.0f;
				if (bDelete)
				{
					if (g_pGameCVars->hud_subtitlesDebug)
					{
						float now = gEnv->pTimer->GetCurrTime();
						CryLogAlways("[SUB] Chunked entry '%s' time-end delete at %f", entry.key.c_str(), now);
					}
					TSubtitleEntries::iterator toDelete = iter;
					++iter;
					m_subtitleEntries.erase(toDelete);
					m_bSubtitlesNeedUpdate = true;
					continue;
				}
			}
			++iter;
		}
	}

	if (g_pGameCVars->hud_subtitlesRenderMode == 0)
	{
		if(g_pGameCVars->hud_subtitles) //should be a switch
		{
			m_animSubtitles.Reload();
			if (m_bSubtitlesNeedUpdate)
			{
				int nToDisplay = g_pGameCVars->hud_subtitlesVisibleCount;
				// re-set text
				CryFixedWStringT<1024> subtitleString;
				bool bFirst = true;
				TSubtitleEntries::iterator iterEnd = m_subtitleEntries.end();
				for (TSubtitleEntries::iterator iter = m_subtitleEntries.begin(); iter != iterEnd && nToDisplay > 0; ++iter, --nToDisplay)
				{
					if (!bFirst)
						subtitleString+=L"\n";
					else
						bFirst = false;
					SSubtitleEntry& entry = *iter;
					if (entry.nChunks == 0)
						subtitleString+=entry.localized;
					else
					{
						assert (entry.nCurChunk >= 0 && entry.nCurChunk < entry.nChunks);
						if (entry.nCurChunk < 0 || entry.nCurChunk >= entry.nChunks)
						{
							CRY_ASSERT(0);
						}
						const SSubtitleEntry::Chunk& chunk = entry.chunks[entry.nCurChunk];
						if (entry.bNameShown == false) // only first visible chunk will display the character's name
						{
							SubtitleAppendCharacterName(entry, subtitleString);
							entry.bNameShown = true;
						}
						subtitleString.append(entry.localized.c_str() + chunk.start, chunk.len);
					}
				}
				m_animSubtitles.Invoke("setText", subtitleString.c_str());
				m_bSubtitlesNeedUpdate = false;
			}
			m_animSubtitles.GetFlashPlayer()->Advance(frameTime);
			m_animSubtitles.GetFlashPlayer()->Render();
		}
		else if(m_animSubtitles.IsLoaded())
		{
			m_animSubtitles.Unload();
		}
	}
	else // manual render
	{
		if (m_subtitleEntries.empty())
			return;

		IUIDraw* pUIDraw = g_pGame->GetIGameFramework()->GetIUIDraw();
		if (pUIDraw==0) // should never happen!
		{
			m_subtitleEntries.clear();
			m_bSubtitlesNeedUpdate = true;
			return;
		}

		IRenderer* pRenderer = gEnv->pRenderer;
		const float x = 0.0f;
		const float maxWidth = 700.0f;
		// float y = 600.0f - (g_pGameCVars->hud_panoramicHeight * 600.0f / 768.0f) + 2.0f;
		float y = 600.0f - (g_pGameCVars->hud_subtitlesHeight * 6.0f) + 1.0f; // subtitles height is in percent of screen height (600.0)

		pUIDraw->PreRender();

		int nToDisplay = g_pGameCVars->hud_subtitlesVisibleCount;

		CryFixedWStringT<1024> tmpString;

		// now draw 2D texts overlay
		for (TSubtitleEntries::iterator iter = m_subtitleEntries.begin(); iter != m_subtitleEntries.end() && nToDisplay > 0; --nToDisplay)
		{
			SSubtitleEntry& entry = *iter;
			ColorF clr = Col_White;
			static const float TEXT_SPACING = 2.0f;
			const float textSize = g_pGameCVars->hud_subtitlesFontSize;
			float sizeX,sizeY;
			const string& textLabel = entry.key;
			if (!textLabel.empty() && textLabel[0] == '@' && entry.localized.empty() == false)
			{
				const wchar_t* szLocText = entry.localized.c_str();
				if (entry.nChunks > 0)
				{
					SSubtitleEntry::Chunk& chunk = entry.chunks[entry.nCurChunk];
					tmpString.clear();
					if (entry.bNameShown == false)
					{
						SubtitleAppendCharacterName(entry, tmpString);
						entry.bNameShown = true;
					}
					tmpString.append(entry.localized.c_str() + chunk.start, chunk.len);
					szLocText = tmpString.c_str();
				}
				pUIDraw->GetWrappedTextDimW(m_pDefaultFont,&sizeX, &sizeY, maxWidth, textSize, textSize, szLocText);
				pUIDraw->DrawWrappedTextW(m_pDefaultFont,x, y, maxWidth, textSize, textSize, szLocText, clr.a, clr.r, clr.g, clr.b, 
					// UIDRAWHORIZONTAL_LEFT,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_LEFT,UIDRAWVERTICAL_TOP);
					UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP);
			}
			else
			{
				pUIDraw->GetTextDim(m_pDefaultFont,&sizeX, &sizeY, textSize, textSize, textLabel.c_str());
				pUIDraw->DrawText(m_pDefaultFont,x, y, textSize, textSize, textLabel.c_str(), clr.a, clr.r, clr.g, clr.b, 
					UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_CENTER,UIDRAWVERTICAL_TOP);
			}
			y+=sizeY+TEXT_SPACING;
			++iter;
		}

		pUIDraw->PostRender();
	}

}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowTutorialText(const wchar_t* text, int pos)
{
	// NB: text is displayed as passed - fetch the localised string before calling this.
	if(text != NULL)
	{
		m_animTutorial.Invoke("setFixPosition", pos);

		m_animTutorial.Invoke("showTutorial", true);
		m_animTutorial.Invoke("setTutorialTextNL",text);
	}
	else
	{
		m_animTutorial.Invoke("showTutorial", false);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetTutorialTextPosition(int pos)
{
	m_animTutorial.Invoke("setFixPosition", pos);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetTutorialTextPosition(float posX, float posY)
{
 SFlashVarValue args[2] = {posX*1024.0f, posY * 768.0f};
 m_animTutorial.Invoke("setPosition", args, 2);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::DisplayAmmoPickup(const char* ammoName, int ammoAmount)
{
	if(!m_bShow || m_quietMode)
		return;

	int type = stl::find_in_map(m_hudAmmunition, ammoName, 0);
	if(!type)
		type = 1;

	string ammoLoc = "@";
	ammoLoc.append(ammoName);
	SFlashVarValue args[3] = {ammoAmount, type, ammoLoc.c_str()};
	m_animAmmoPickup.Invoke("setPickup", args, 3);
}
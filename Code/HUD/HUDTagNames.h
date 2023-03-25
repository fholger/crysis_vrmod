/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	MP TagNames

-------------------------------------------------------------------------
History:
- 21:05:2007: Created by Julien Darre

*************************************************************************/
#ifndef __HUDTAGNAMES_H__
#define __HUDTAGNAMES_H__

//-----------------------------------------------------------------------------------------------------

class CHUDTagNames
{
public:

		CHUDTagNames();
	~	CHUDTagNames();

	void Update();

	//added in MP when client hit another player/vehicle
	void AddEnemyTagName(EntityId uiEntityId);

private:

	const char *GetPlayerRank(EntityId uiEntityId);

	bool ProjectOnSphere(Vec3 &rvWorldPos,const AABB &rvBBox);

	bool IsFriendlyToClient(EntityId uiEntityId);

	IUIDraw *m_pUIDraw;
	IFFont *m_pMPNamesFont;

	struct STagName
	{
		CryFixedStringT<64> strName;
		Vec3 vWorld;
		bool bDrawOnTop;
		ColorF rgb;
	};
	typedef std::vector<STagName> TTagNamesVector;
	TTagNamesVector m_tagNamesVector;

	struct SEnemyTagName
	{
		EntityId uiEntityId;
		float fSpawnTime;
	};
	typedef std::list<SEnemyTagName> TEnemyTagNamesList;
	TEnemyTagNamesList m_enemyTagNamesList;

	void DrawTagName(IActor *pActor,bool bLocalVehicle=false);
	void DrawTagName(IVehicle *pVehicle);
	void DrawTagNames();
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------

/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
Description: 	Wrapper for Logitech G15 SDK and functionality

-------------------------------------------------------------------------
History:
- 01:11:2007: Created by Marco Koegler

*************************************************************************/
#ifndef __LCDWRAPPER_H__
#define __LCDWRAPPER_H__

#include "ILCD.h"

#ifdef USE_G15_LCD

class CEzLcd;
class CLCDPage;
class CLCDImage;

class CG15LCD : public ILCD
{
public:
	CG15LCD();
	virtual ~CG15LCD();

	//ILCD
	virtual bool	Init();
	virtual void	Update(float frameTime);
	//~ILCD

	bool	IsConnected();

	CLCDImage* CreateImage(const char* name=0, bool visible=true);
	
	CEzLcd*	GetEzLcd() const	{	return m_pImpl;	};
	int			AddPage(CLCDPage* pPage, int pageId = -1);

	void		SetCurrentPage(int pageId = -1);
	int			GetCurrentPage() const	{	return m_currentPage;	}
	void		ShowCurrentPage();

	// state machine states
	int				LogoPage;
	int				LoadingPage;
	int				GameStatusPage;
	int				PlayerStatusPage;

private:
	typedef std::vector<CLCDPage*>	TLCDPages;

	CEzLcd*				m_pImpl;
	int						m_currentPage;
	TLCDPages			m_pages;
	float					m_tick;
};

#endif //USE_G15_LCD

#endif
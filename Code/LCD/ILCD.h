/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
Description: 	Stub interface for Logitech G15 SDK and functionality

-------------------------------------------------------------------------
History:
- 01:11:2007: Created by Marco Koegler

*************************************************************************/
#ifndef __ILCD_H__
#define __ILCD_H__

struct ILCD
{
	virtual ~ILCD(){}

	virtual bool	Init() = 0;
	virtual void	Update(float frameTime) = 0;
};

class CNullLCD : public ILCD
{
public:
	virtual bool	Init()	{	return true;	}
	virtual void	Update(float frameTime){}
};

#endif
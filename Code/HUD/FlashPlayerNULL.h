/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 10:11:2006: Created by Julien Darre

*************************************************************************/
#ifndef __FLASHPLAYERNULL_H__
#define __FLASHPLAYERNULL_H__

//-----------------------------------------------------------------------------------------------------

#include "IFlashPlayer.h"

//-----------------------------------------------------------------------------------------------------

class CFlashPlayerNULL : public IFlashPlayer
{
public:

	// IFlashPlayer
	virtual void Release() { delete this; }
	virtual bool Load( const char* pSWFFilePath, unsigned int options = RENDER_EDGE_AA | INIT_FIRST_FRAME ) { return true; }
	virtual void SetBackgroundColor( unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha ) {}
	virtual void SetBackgroundAlpha( float alpha ) {}
	virtual void SetViewport( int x0, int y0, int width, int height, float aspectRatio = 1.0f ) {}
	virtual void GetViewport( int& x0, int& y0, int& width, int& height, float& aspectRatio ) {}
	virtual void SetScissorRect(int x0, int y0, int width, int height) {}
	virtual void GetScissorRect(int& x0, int& y0, int& width, int& height) {}
	virtual void Advance( float deltaTime ) {}
	virtual void Render() {}
	virtual void SetCompositingDepth(float depth) {}
	virtual void SetFSCommandHandler( IFSCommandHandler* pHandler ) {}
	virtual void SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler) {};
	virtual void SetLoadMovieHandler(IFlashLoadMovieHandler* pHandler) {}
	virtual void SendCursorEvent( const SFlashCursorEvent& cursorEvent ) {}
	virtual void SendKeyEvent( const SFlashKeyEvent& keyEvent ) {}
	virtual void SetVisible(bool visible) {}
	virtual bool GetVisible() const { return false; }
	virtual bool SetVariable(const char* pPathToVar, const SFlashVarValue& value) { return true; }
	virtual bool GetVariable(const char* pPathToVar, SFlashVarValue* pValue) const { return true; }
	virtual bool IsAvailable(const char* pPathToVar) const { return true; }
	virtual bool SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count) const { return true; }
	virtual unsigned int GetVariableArraySize(const char* pPathToVar) const { return 0; }
	virtual bool GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count) { return true; }
	virtual bool Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult) { return true; }
	virtual int	GetWidth() const { return 0; }
	virtual int	GetHeight() const { return 0; }
	virtual size_t GetMetadata( char* pBuff, unsigned int buffSize ) const { return 0; }
	virtual int GetFlags() const { return 0; }
	virtual const char* GetFilePath() const { return 0; }
	virtual void ScreenToClient(int& x, int& y) const {}
	virtual void ClientToScreen(int& x, int& y) const {}

	virtual void SetVariableDouble(const char *,double) {}
	virtual void SetVariableArrayDouble(const char *,const double *,int) {}
	virtual double GetVariableDouble(const char *) const { return 0; }
	// ~IFlashPlayer

	CFlashPlayerNULL() {}
	~CFlashPlayerNULL()	{}
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------
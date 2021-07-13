#pragma once

#include "dta_lexicon.h"
#include <storage_device_lib.h>

class ITcgSession : virtual public IJCInterface
{
public:
	virtual BYTE L0Discovery(BYTE* buf) = 0;
	virtual BYTE StartSession(const TCG_UID SP, const char* host_challenge, const TCG_UID sign_authority, bool write=true) = 0;
	virtual BYTE EndSession(void)=0;
	virtual bool IsOpen(void) = 0;

	virtual BYTE GetDefaultPassword(std::string& password) = 0;
	virtual BYTE SetSIDPassword(const char* old_pw, const char* new_pw) = 0;
	virtual void Reset(void) = 0;


};

void CreateTcgSession(ITcgSession*& session, IStorageDevice* dev);

// éwíËêîêòï∂åèòHåa
void CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring& path);
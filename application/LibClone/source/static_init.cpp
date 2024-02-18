#include "pch.h"
#include "static_init.h"

#include <lib_authorizer.h>


LOCAL_LOGGER_ENABLE(L"libclone", LOGGER_LEVEL_NOTICE);

CStaticInit global_init;
extern bool authorized= false;



using namespace System;

CStaticInit::CStaticInit(void)
{
	//CApplicationAuthorize* authorizer = CApplicationAuthorize::Instance();
	//bool br = authorizer->CheckAuthority("com.kanenka.renewdisk.v01");
	//if (!br) throw gcnew System::ApplicationException(L"[err] authorization failed for storage manager");

	//wprintf_s(L"initialize LibClone\n");
	//m_module_guid = { 0x22431a3a, 0x2ca4, 0x4e29, { 0xbf, 0xf0, 0x9b, 0x17, 0x96, 0xc, 0x58, 0x17 } };
	//m_authorize = CSoftwareAuthorize::Instance();
	//authorized = m_authorize->CheckAuthority(m_module_guid);
	//wprintf_s(L"authorize =%d", authorized);

	//if (!authorized) throw gcnew System::ApplicationException(L"failed on authoritzing");

	//if (!br) THROW_ERROR(ERR_APP, L"failed on getting authorization.");
	//LOG_NOTICE(L"authorized");
}


#include "pch.h"

#include "authority.h"
//#include "../include/storage_manager.h"
#include "static_init.h"

LOCAL_LOGGER_ENABLE(L"authority", LOGGER_LEVEL_NOTICE);


void Clone::CloneAuthority::AddLicense(String^ license)
{
	const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(license)).ToPointer();
	std::string str_license;
	jcvos::UnicodeToUtf8(str_license, wstr);
	Authority::CApplicationAuthorize* authorizer = Authority::CApplicationAuthorize::Instance();
	if (authorizer == nullptr) throw gcnew Clone::CloneException(L"failed on getting authorize", DR_MEMORY);
	Authority::LICENSE_ERROR_CODE ir = authorizer->SetLicense(str_license);
	if (ir != Authority::LICENSE_OK) throw gcnew Clone::CloneException(L"failed on setting license", ir);
}

UINT64 Clone::CloneAuthority::GetLicenseList(void)
{
	Authority::CApplicationAuthorize* authorizer = Authority::CApplicationAuthorize::Instance();
	if (authorizer == nullptr) throw gcnew Clone::CloneException(L"failed on getting authorize", DR_MEMORY);
	return authorizer->GetLicenseList(g_app_name);
}
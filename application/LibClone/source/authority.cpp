#include "pch.h"

#include "authority.h"

LOCAL_LOGGER_ENABLE(L"authority", LOGGER_LEVEL_NOTICE);


bool authority::Authority::AddLicense(String^ license)
{
	const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(license)).ToPointer();
	std::string str_license;
	jcvos::UnicodeToUtf8(str_license, wstr);
	CApplicationAuthorize* authorizer = CApplicationAuthorize::Instance();
	if (authorizer == nullptr) THROW_ERROR(ERR_APP, L"failed on getting authorize");
	bool br = authorizer->SetLicense(str_license);
	return br;
}
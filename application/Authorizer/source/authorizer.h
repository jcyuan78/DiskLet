#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <lib_authorizer.h>


using namespace System;
using namespace System::Management::Automation;

namespace authorizer
{

	public ref class Authorize : public Object
	{
	public:

	public:
		static bool SetAuthorize(System::Guid ^ mguid)
		{
			return false;
		}
	};

	[CmdletAttribute(VerbsLifecycle::Register, "CloneLicense")]
	public ref class CRegisterModule : public Cmdlet
	{
	public:
		CRegisterModule(void) { license = nullptr; fn = nullptr; };
		~CRegisterModule(void) {};

	public:
		[Parameter(Position = 0, 
			ParameterSetName = "By_String",
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String ^ license;


		[Parameter(Position = 0, 
			ParameterSetName = "By_File",
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String ^ fn;

	public:
		virtual void ProcessRecord() override
		{
			Authority::CApplicationAuthorize* authorize = Authority::CApplicationAuthorize::Instance();
			std::string str_license;
			if (license)
			{
				const wchar_t * wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(license)).ToPointer();
				jcvos::UnicodeToUtf8(str_license, wstr);
			}
			else if (fn)
			{
				const wchar_t * wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(fn)).ToPointer();
//				str_license.reserve(1024);
				jcvos::auto_array<char> buf(1024, 0);
				FILE* file = NULL;
				_wfopen_s(&file, wstr, L"r+");
				size_t read = fread(buf.get_ptr(), 1024, 1, file);
				str_license = (char*)buf;
			}
			Authority::LICENSE_ERROR_CODE ir = authorize->SetLicense(str_license);
			wprintf_s(L"set license, result=%d", ir);
		}
	};

	[CmdletAttribute(VerbsDiagnostic::Test, "CloneLicense")]
	public ref class CTestLicense : public Cmdlet
	{
	public:
		CTestLicense(void) { };
		~CTestLicense(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			ParameterSetName = "By_String",
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String^ module_name;

	public:
		virtual void ProcessRecord() override
		{
			Authority::CApplicationAuthorize* authorize = Authority::CApplicationAuthorize::Instance();
			const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(module_name)).ToPointer();
			std::string str_module_name;
			jcvos::UnicodeToUtf8(str_module_name, wstr);
			Authority::LICENSE_ERROR_CODE ir = authorize->CheckAuthority(str_module_name);
			wprintf_s(L"check license, result=%d", ir);
		}
	};

	[CmdletAttribute(VerbsDiagnostic::Test, "DriveLicense")]
	public ref class CTestDriveLicese : public Cmdlet
	{
	public:
		CTestDriveLicese(void) { };
		~CTestDriveLicese(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String^ module_name;
		[Parameter(Position = 1, Mandatory = true,
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String^ drive_name;


	public:
		virtual void ProcessRecord() override
		{
			Authority::CApplicationAuthorize* authorize = Authority::CApplicationAuthorize::Instance();
			const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(module_name)).ToPointer();
			std::string str_module_name;
			jcvos::UnicodeToUtf8(str_module_name, wstr);
			const wchar_t* str_drive = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(drive_name)).ToPointer();
			Authority::LICENSE_ERROR_CODE ir = authorize->CheckDeviceLicense(str_module_name, str_drive);
			wprintf_s(L"check drive license, drive=%s, result=%d", str_drive, ir);
		}
	};

	[CmdletAttribute(VerbsCommon::Get, "CloneLicense")]
	public ref class CGetCloneLicense : public Cmdlet
	{
	public:
		CGetCloneLicense(void) { };
		~CGetCloneLicense(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			HelpMessage = "specify the index of disk, default returns all disks")]
		property String^ module_name;

	public:
		virtual void ProcessRecord() override
		{
			Authority::CApplicationAuthorize* authorize = Authority::CApplicationAuthorize::Instance();
			const wchar_t* wstr = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(module_name)).ToPointer();
			std::string str_module_name;
			jcvos::UnicodeToUtf8(str_module_name, wstr);
			UINT64 list = authorize->GetLicenseList(str_module_name);
	//		Authority::LICENSE_ERROR_CODE ir = authorize->CheckAuthority(str_module_name);
			wprintf_s(L"check license, result=0x%08llX", list);
		}
	};
}
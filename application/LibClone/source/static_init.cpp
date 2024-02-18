#include "pch.h"
#include "static_init.h"
#include <fstream>
//#include "../include/storage_manager.h"



#include <lib_authorizer.h>
#include <boost/property_tree/xml_parser.hpp>
#include <cryptopp/fips140.h>

LOCAL_LOGGER_ENABLE(L"libclone", LOGGER_LEVEL_NOTICE);

CStaticInit global_init;
extern bool authorized= false;

using namespace System;
//static const std::wstring log_config_file = L"libclone.cfg";
#define LOG_CONFIG_FILE L"libclone.cfg"

CStaticInit::CStaticInit(void)
	: m_force_mbr(false), m_source_is_system(false), m_replace_efi_part(false)
{
	// 初始化 Log
	jcvos::auto_array<wchar_t> dllpath(MAX_PATH, 0);
	// 获取DLL路径
	MEMORY_BASIC_INFORMATION mbi;
	memset(&mbi, 0, sizeof(mbi));
	static int dummy=0;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE this_dll = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	// dllpath: 获取dll的路径，包括dll文件名的全路径
	::GetModuleFileName(this_dll, dllpath, MAX_PATH);
	// 分解dllpath，目录的路径存放到m_module_path, 不包括最后的斜杠
	std::wstring fn;
	jcvos::ParseFileName((const wchar_t*)dllpath, m_module_path, fn);

	// Load log config
	LOGGER_CONFIG(LOG_CONFIG_FILE, m_module_path.c_str());
	LOG_DEBUG(L"log config for LibClone...");
	LOG_DEBUG(L"msg=%s", L"Initialize COM");

	CryptoPP::DoPowerUpSelfTest(NULLPTR, NULLPTR);

	// load configuration
	std::wstring cfg_fn = global_init.GetModulePath();
	cfg_fn += L"\\config.xml";
	LoadConfig(cfg_fn);
}

bool CStaticInit::LoadConfig(const std::wstring& fn)
{
	try
	{
		LOG_NOTICE(L"load config from file %s", fn.c_str());
		std::wfstream ff;
		ff.open(fn, std::ios::in);
		boost::property_tree::wptree root;
		boost::property_tree::read_xml(ff, root);
		m_force_mbr = root.get<bool>(L"configuration.disk_clone.force_mbr", false);
		m_source_is_system = root.get<bool>(L"configuration.disk_clone.source_is_system", false);
		m_replace_efi_part = root.get<bool>(L"configuration.disk_clone.replace_efi", false);
		LOG_NOTICE(L"configs: force_mbr=%d, source_system=%d, replace_efi=%d", 
			m_force_mbr, m_source_is_system, m_replace_efi_part);
		// load default license
		auto lpt = root.get_child_optional(L"configuration.license_set");
		if (lpt)
		{
			Authority::CApplicationAuthorize* authorizer = Authority::CApplicationAuthorize::Instance();
			if (authorizer == nullptr) THROW_ERROR(ERR_APP, L"failed on getting CApplicationAuthorize object");
			const boost::property_tree::wptree& license_pt = *lpt;
			for (auto it = license_pt.begin(); it != license_pt.end(); ++it)
			{
				if (it->first == L"license")
				{
					const std::wstring& license_name = it->second.get<std::wstring>(L"<xmlattr>.name", L"");
					LOG_NOTICE(L"load license name=%s", license_name.c_str());
					const std::wstring& wstr_license = it->second.get_value<std::wstring>();
					std::string str_license;
					jcvos::UnicodeToUtf8(str_license, wstr_license);
					Authority::LICENSE_ERROR_CODE ir = authorizer->SetLicense(str_license);
					if (ir != Authority::LICENSE_OK)
					{
						LOG_ERROR(L"[err] failed on register license %s", license_name.c_str());
					}
				}
			}
		}
	}
	catch (std::exception& err)
	{
		std::wstring err_msg;
		jcvos::Utf8ToUnicode(err_msg, err.what());
		LOG_WARNING(L"failed on loading config from %s, error=%s", fn.c_str(), err_msg.c_str());
		return false;
	}
	return true;
}

#pragma once

#include "../../StorageManagementLib/storage_management_lib.h"

extern const std::string g_app_name;

namespace Clone
{
	public ref class CloneException : public System::Exception
	{
	public:
		CloneException(System::String^ exp, int code) : System::Exception(exp)
		{
			error_code = code;
		}
	public:
		int error_code;
	};
}

class CStaticInit
{
public:
	CStaticInit(void);
	~CStaticInit(void) {};
public:
	const std::wstring& GetModulePath() { return m_module_path; }
	bool LoadConfig(const std::wstring& fn);


protected:
	std::wstring m_module_path;

public:
	// configurations
	bool m_force_mbr, m_source_is_system;
	bool m_replace_efi_part;
};

extern CStaticInit global_init;

//extern bool authorized;
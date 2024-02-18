#pragma once

#include "comm_structure.h"
#include <map>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <cryptopp/rsa.h>


namespace Authority
{
	enum LICENSE_ERROR_CODE
	{
		LICENSE_OK = 0, 
		LICENSE_EMPTY = 5001,			LICENSE_OUTOFDATE = 5002,	LICENSE_INVALID = 5003,
		LICENSE_UNMATCH = 5004,			LICENSE_OTHER = 5005,

		AUTHORIZE_USERNAME = 6001,				/*AUTHORIZE_USERNAME = 6002,*/
		AUTHORIZE_WRONG_DELIVERY = 6003,		AUTHORIZE_REGISTER = 6004,
		AUTHORIZE_LICENSE = 6005,
		
	};

	class CAuthority;

	class CApplicationAuthorize : public jcvos::CSingleTonBase
	{
	public:
		CApplicationAuthorize(void);
		~CApplicationAuthorize(void);

	public:
		//bool SetAuthority(const GUID & module_guid);
		LICENSE_ERROR_CODE SetLicense(const std::string& license);
		//	bool CheckAuthority(const GUID & module_guid);
		LICENSE_ERROR_CODE CheckAuthority(const std::string& app_name);
		LICENSE_ERROR_CODE CheckDeviceLicense(const std::string& app_name, const std::wstring& drive_name);
		UINT64 GetLicenseList(const std::string& app_name);

	protected:
		CAuthority* GetLicenseSet(const std::string& app_name);
		CryptoPP::RSA::PublicKey m_master_key_public;

		WORD m_license_ver;
		WORD m_license_num;
		std::map<DWORD, CAuthority*> m_license_set;

		static const std::string m_master_name;
		boost::uuids::uuid m_master_uuid;
		DWORD m_master_id;
		size_t m_rsa_block_size;

		// members for single tone
	public:
		static const GUID m_guid;
		static const GUID& Guid(void) { return m_guid; };
		virtual void Release(void) { delete this; }
		virtual const GUID& GetGuid(void) const { return Guid(); };

		static CApplicationAuthorize* Instance(void)
		{
			static CApplicationAuthorize* instance = NULL;
			if (instance == NULL)	CSingleTonEntry::GetInstance<CApplicationAuthorize >(instance);
			return instance;
		}
	};
}

#define LICENSE_ERROR(code, msg, ...) do {	\
	jcvos::auto_array<wchar_t> buf(256);		\
	swprintf_s(buf.get_ptr(), buf.len(), L"[err %04d] " msg, code, __VA_ARGS__); \
	LOG_ERROR(buf.get_ptr()); return (code); } while (0)



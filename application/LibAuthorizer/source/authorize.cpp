#include "pch.h"

#include "../include/authorize.h"

#include <boost/uuid/uuid_io.hpp>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>

#include "../include/crypto_feature.h"

using namespace Authority;


LOCAL_LOGGER_ENABLE(L"authorize", LOGGER_LEVEL_NOTICE);

BYTE public_key05[160] = {
	0x30, 0x81, 0x9D, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01,
	0x05, 0x00, 0x03, 0x81, 0x8B, 0x00, 0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xE0, 0x72, 0xFA,
	0xE0, 0x98, 0xC0, 0xBB, 0x07, 0xE9, 0xF0, 0x65, 0xF5, 0xF5, 0x6B, 0x05, 0x1B, 0x56, 0xCF, 0xE9,
	0xF6, 0xF2, 0xFD, 0x54, 0x17, 0xC9, 0x59, 0xE1, 0x27, 0x7A, 0xEA, 0x2F, 0x7F, 0x8B, 0x46, 0x9F,
	0x83, 0xAA, 0x45, 0x39, 0x90, 0xCF, 0xDD, 0xB7, 0x63, 0x8A, 0x2D, 0xC8, 0xA4, 0x66, 0x92, 0x1C,
	0x0C, 0xF9, 0xE6, 0x1E, 0x3C, 0xA9, 0x78, 0x13, 0x72, 0x34, 0xBD, 0xCA, 0x61, 0x2C, 0x83, 0x2C,
	0x08, 0x39, 0xC7, 0x79, 0x96, 0x7F, 0x7A, 0x9A, 0x6B, 0xFE, 0xE1, 0xDF, 0xEF, 0xF7, 0x6E, 0x76,
	0x43, 0xF2, 0x31, 0xFF, 0xCB, 0x7B, 0x85, 0x1C, 0x58, 0x39, 0x17, 0x25, 0xE9, 0xB0, 0xBA, 0x54,
	0xA3, 0x6E, 0x63, 0xA7, 0x0A, 0x9E, 0xE6, 0x18, 0x10, 0x29, 0x87, 0x3B, 0xB9, 0x77, 0x0D, 0xFB,
	0xD2, 0x49, 0xF5, 0xFD, 0x77, 0xE9, 0x79, 0xC3, 0x4A, 0xE6, 0xAE, 0x6F, 0x0B, 0x02, 0x01, 0x11
};

#define public_key public_key05

LOG_CLASS_SIZE(LICENSE_HEADER)
LOG_CLASS_SIZE(DELIVERY_LICENSE)



const std::string CApplicationAuthorize::m_master_name = "com.kanenka.master";


class Authority::CAuthority
{
public:
	CAuthority(void)
	{
		memset(&m_delivery, 0, sizeof(Authority::DELIVERY_LICENSE));
	}

public:
	DWORD m_app_id;
	DELIVERY_LICENSE m_delivery;
	std::vector<std::wstring> m_drive_list;
};



// {01F8A8F9-034D-41AB-A60F-AE445A394848}
const GUID CApplicationAuthorize::m_guid = { 0x1f8a8f9, 0x34d, 0x41ab, { 0xa6, 0xf, 0xae, 0x44, 0x5a, 0x39, 0x48, 0x48 } };

CApplicationAuthorize::CApplicationAuthorize(void)
{
	// 验证程序完整性
	// load public_key
	crypto::LoadPublicKey(m_master_key_public, public_key, sizeof(public_key));
	// 计算master id
	crypto::SetStringId(m_master_name, m_master_uuid, m_master_id);
	m_rsa_block_size = m_master_key_public.MaxImage().ByteCount();

}

CApplicationAuthorize::~CApplicationAuthorize(void)
{
	for (auto ii = m_license_set.begin(); ii != m_license_set.end(); ++ii)
	{
		delete (ii->second);
	}
}

LICENSE_ERROR_CODE CApplicationAuthorize::SetLicense(const std::string& license)
{
	// 解码：Base64 => binary
	// 	   估算长度
	size_t data_len = license.size() * 3 / 4 + 2 * m_rsa_block_size;
	data_len /= m_rsa_block_size;	// 取整
	data_len *= m_rsa_block_size;

	jcvos::auto_array<BYTE> plane_buf(data_len, 0);
	// Decode Base64
	size_t plane_len = crypto::HexDecode<CryptoPP::Base64Decoder>(license, plane_buf, data_len);
	// 解密
	jcvos::auto_array<BYTE> cipher_buf(plane_len, 0);
	crypto::RSAEncrypt(cipher_buf, plane_len, plane_buf, plane_len, m_master_key_public);
	// 解析头
	LICENSE_HEADER* header = reinterpret_cast<LICENSE_HEADER*>(cipher_buf.get_ptr());
	DWORD crc = header->CalculateCrc(1);
	if (crc != header->m_crc) LICENSE_ERROR(LICENSE_INVALID, 
		L"CRC does not match in header, org crc=%08X, check crc=%08X", header->m_crc, crc);

	LICENSE_HEADER* cur_license = header + 1;
	while (1)
	{	// 检查 当前License
		crc = cur_license->CalculateCrc(header->m_size);
		if (crc != cur_license->m_crc) LICENSE_ERROR(LICENSE_INVALID,
			L"CRC does not match, header crc=%08X, check crc=%08X", cur_license->m_crc, crc);
		if (cur_license->m_pre_crc != header->m_crc)  LICENSE_ERROR(LICENSE_INVALID,
			L"CRC does not match previous, pre crc=%08X, header=%08X", header->m_crc, cur_license->m_pre_crc);
		// 解码
		// 检查license是否存在
		CAuthority* license=nullptr;
		auto it = m_license_set.find(header->m_app_id);
		if (it == m_license_set.end())
		{
			license = new CAuthority;
			license->m_app_id = header->m_app_id;
			m_license_set.insert(std::make_pair(header->m_app_id, license));
		}
		else		license = it->second;

		if (header->m_type == LT_DELIVERY)
		{
			if (license->m_delivery.m_master_id != 0) LOG_NOTICE(L"delivery license has already set");
			DELIVERY_LICENSE* local_license = reinterpret_cast<DELIVERY_LICENSE*>(cur_license);
			memcpy_s(&license->m_delivery, sizeof(DELIVERY_LICENSE), local_license, sizeof(DELIVERY_LICENSE));
		}
		else if (header->m_type = LT_DRIVE)
		{
			Authority::DRIVE_LICENSE* local_license = reinterpret_cast<Authority::DRIVE_LICENSE*>(cur_license);
			char* dev = local_license->m_drives;
			while (*dev)
			{
				std::wstring str_device;
				jcvos::Utf8ToUnicode(str_device, dev);
//				std::string str(dev);
				license->m_drive_list.push_back(str_device);
				dev += (strlen(dev) + 1);
			}
		}
		LICENSE_HEADER* next = cur_license + header->m_size;
		header = cur_license;
		if (header->m_type == LT_ENDOF_LICENSE) break;
		cur_license = next;
	}
	return LICENSE_OK;
}

LICENSE_ERROR_CODE CApplicationAuthorize::CheckAuthority(const std::string& app_name)
{
	Authority::CAuthority* authority = GetLicenseSet(app_name);
	if (authority == nullptr) LICENSE_ERROR(LICENSE_EMPTY, L"cannot find license for %hs", app_name.c_str());

	const DELIVERY_LICENSE & license = authority->m_delivery;
	// 检查ID
	if (license.m_master_id != m_master_id) LICENSE_ERROR(LICENSE_INVALID,
		L"master id does not match, master id=%08X", license.m_master_id);

	// 检查有效期
	if (license.m_period != LICENSE_PERIOD_INFINITE)
	{
		boost::gregorian::date today = boost::gregorian::day_clock::universal_day();
		boost::gregorian::date_duration elaspe = today - license.m_start_date;
		if (elaspe.days() > (int)license.m_period) LICENSE_ERROR(LICENSE_OUTOFDATE,
			L"license expired, start date=%s, period=%d, today=%d",
			boost::gregorian::to_simple_wstring(license.m_start_date).c_str(), license.m_period, elaspe.days());
	}
	else LOG_NOTICE(L"ignor checing period");

	// 检查Computer Name
	if (license.m_computer_name != COMPUTER_NAME_IGNOR)
	{
		DWORD len = MAX_COMPUTERNAME_LENGTH+1;
		wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
		memset(name, 0, MAX_COMPUTERNAME_LENGTH + 1);
		BOOL br = GetComputerName(name, &len);
		if (!br) LICENSE_ERROR(LICENSE_OTHER, L"failed on getting computer name, code=%d, len=%d", GetLastError(), len);
		LOG_DEBUG(L"got computer name=%s, len=%d", name, len);
		UINT64 hash = crypto::ComputerNameHash(name);
		if (license.m_computer_name != hash) LICENSE_ERROR(LICENSE_UNMATCH, L"computer name does not match, name=%s", name);
	}
	else LOG_NOTICE(L"ignore checking computer name");

	return LICENSE_OK;
}


Authority::CAuthority* Authority::CApplicationAuthorize::GetLicenseSet(const std::string& app_name)
{
	boost::uuids::uuid app_uuid;
	DWORD app_id;
	crypto::SetStringId(app_name, app_uuid, app_id);

	std::wstring str_uuid = boost::uuids::to_wstring(app_uuid);
	LOG_DEBUG(L"app guid = %s", str_uuid.c_str());

	auto it = m_license_set.find(app_id);
	if (it == m_license_set.end() || it->second == NULL || it->second->m_app_id != app_id) return NULL;
	else return it->second;
}

Authority::LICENSE_ERROR_CODE Authority::CApplicationAuthorize::CheckDeviceLicense(const std::string& app_name, const std::wstring& drive_name)
{
	Authority::CAuthority* authority = GetLicenseSet(app_name);
	if (authority == nullptr) LICENSE_ERROR(LICENSE_EMPTY, L"cannot find license for %hs", app_name.c_str());
	for (auto it = authority->m_drive_list.begin(); it != authority->m_drive_list.end(); ++it)
	{
		const wchar_t * ch = wcsstr(drive_name.c_str(), it->c_str());
		if (ch != nullptr)
		{
			LOG_NOTICE(L"matches patern: %s", it->c_str());
			return LICENSE_OK;
		}
	}
	LOG_ERROR(L"[err 7210] no pattern matches for %s", drive_name.c_str());
	return Authority::LICENSE_UNMATCH;
}

UINT64 Authority::CApplicationAuthorize::GetLicenseList(const std::string& app_name)
{
	UINT64 list = 0;
	Authority::CAuthority* authority = GetLicenseSet(app_name);
	if (authority->m_delivery.m_version == DELIVERY_VERSION_INTERNAL)	list |= (1 << Authority::LT_INTERNAL);
	else if (authority->m_delivery.m_version == DELIVERY_VERSION_NORMAL) list |= (1 << Authority::LT_DELIVERY);
	return list;
}


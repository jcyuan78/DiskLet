#include "pch.h"

// property_tree的引用必须放在cryptopp的引用之前

#include "../include/application_info.h"

#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/cast.hpp>
#include <boost/functional/hash.hpp>

#include <cryptopp/hex.h>
#include <cryptopp/crc.h>
#include <cryptopp/base32.h>
#include <cryptopp/base64.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>
#include <cryptopp/rsa.h>

#include <lib_authorizer.h>

LOCAL_LOGGER_ENABLE(L"app_info", LOGGER_LEVEL_DEBUGINFO);

#define INIT_REBOUND_COUNT (3)


CApplicationInfo::CApplicationInfo(void): m_iterator(NULL)
{
//	m_delivery_store.OpenDb(L"authority.db");
// 验证大小
	JCASSERT(sizeof(Authority::LICENSE_HEADER) == LICENSE_BLOCK_SIZE);
	JCASSERT(sizeof(Authority::DELIVERY) == DELIVERY_LEN);
	JCASSERT(sizeof(Authority::DELIVERY_KEY) == DELIVERY_LEN);

}

CApplicationInfo::~CApplicationInfo(void)
{
	ClearDeliveryMap();
}

void CApplicationInfo::ClearDeliveryMap(void)
{

}

void CApplicationInfo::SetMasterId(const std::string &masterid)
{
	m_string_masterid = masterid;
	crypto::SetStringId(m_string_masterid, m_uuid_masterid, m_id_masterid);
}

void CApplicationInfo::SetAppId(const std::string& appid)
{
	m_string_appid = appid;
	crypto::SetStringId(m_string_appid, m_uuid_appid, m_id_appid);
}

bool CApplicationInfo::GenerateMasterKey(size_t key_len)
{
	// TODO: 在此添加控件通知处理程序代码
	CryptoPP::AutoSeededRandomPool rng;
	size_t key_len_bit = key_len * 8;
	m_master_key_private.GenerateRandomWithKeySize(rng, boost::numeric_cast<UINT>(key_len_bit));
	SetPrivateKey(m_master_key_private);
	return true;
}

bool CApplicationInfo::SaveMasterKey(const std::wstring& fn, KEY_FILE_FORMAT format)
{
	CryptoPP::BufferedTransformation* bt;
	CryptoPP::ByteQueue queue;
	m_master_key_private.Save(queue);
	CryptoPP::Base64Encoder encoder;

	if (format == KEY_FILE_TEXT)
	{
		queue.CopyTo(encoder);
		encoder.MessageEnd();
		bt = static_cast<CryptoPP::BufferedTransformation*>(&encoder);
	}
	else if (format == KEY_FILE_BINARY)
	{
		bt = static_cast<CryptoPP::BufferedTransformation*>(&queue);
	}
	else
	{
		LOG_ERROR(L"[err] Unknown file format: %d", (int)format);
		return false;
	}
	CryptoPP::FileSink file_sink(fn.c_str());
	bt->CopyTo(file_sink);
	file_sink.MessageEnd();

	return true;
}

bool CApplicationInfo::LoadMasterKey(const std::wstring& fn, KEY_FILE_FORMAT format)
{
	CryptoPP::FileSource file(fn.c_str(), true);
	CryptoPP::ByteQueue que;

	if (format == KEY_FILE_BINARY)
	{
		file.TransferTo(que);
		que.MessageEnd();
	}
	else if (format == KEY_FILE_TEXT)
	{
		CryptoPP::Base64Decoder decoder;
		file.TransferTo(decoder);
		decoder.MessageEnd();
		decoder.CopyTo(que);
	}
	else
	{
		LOG_ERROR(L"[err] Unknown file format: %d", (int)format);
		return false;
	}
	m_master_key_private.Load(que);
	SetPrivateKey(m_master_key_private);
	return true;
}

bool CApplicationInfo::ExportPublicKey(const std::wstring& fn, KEY_FILE_FORMAT format)
{
	CryptoPP::BufferedTransformation* bt;
	CryptoPP::ByteQueue queue;
	m_master_key_public.Save(queue);
	CryptoPP::Base64Encoder encoder;

	if (format == KEY_FILE_TEXT)
	{
		queue.CopyTo(encoder);
		encoder.MessageEnd();
		bt = static_cast<CryptoPP::BufferedTransformation*>(&encoder);
	}
	else if (format == KEY_FILE_BINARY)
	{
		bt = static_cast<CryptoPP::BufferedTransformation*>(&queue);
	}
	else
	{
		LOG_ERROR(L"[err] Unknown file format: %d", (int)format);
		return false;
	}
	CryptoPP::FileSink file_sink(fn.c_str());
	bt->CopyTo(file_sink);
	file_sink.MessageEnd();

	return true;
}

std::string CApplicationInfo::GetMasterFinger(void)
{
	std::string finger;
	crypto::HexEncode<CryptoPP::HexEncoder>(finger, m_master_finger, CryptoPP::SHA1::DIGESTSIZE);
	return finger;
}

bool CApplicationInfo::SetSnPassword(const std::string& password)
{
	m_sn_password = password;
	Sha1(m_snpassword_finger, (BYTE*)m_sn_password.c_str(), m_sn_password.size());
	memcpy_s(m_snpassword, sizeof(m_snpassword), m_snpassword_finger, sizeof(m_snpassword));
	memcpy_s(&m_scramble1, sizeof(DWORD), m_snpassword_finger + 12, sizeof(DWORD));	// 避免暴露
	memcpy_s(&m_scramble2, sizeof(DWORD), m_snpassword_finger + 16, sizeof(DWORD));	// 避免暴露

	return true;
}

std::string CApplicationInfo::GetSnPasswordFinger(void)
{
	std::string finger;
	crypto::HexEncode<CryptoPP::HexEncoder>(finger, m_snpassword_finger, CryptoPP::SHA1::DIGESTSIZE);
	return finger;
}

void CApplicationInfo::AppInfoToProperty(boost::property_tree::ptree& pt)
{
	pt.put("master_name", m_string_masterid);
	pt.put<std::string>("application_name", m_string_appid);
	pt.put<std::string>("serial_num_key", m_sn_password);
	std::string str_private;
	PrivateKeyToString(str_private, m_master_key_private);
	pt.put<std::string>("master_key", str_private);
}

void CApplicationInfo::PropertyToAppInfo(const boost::property_tree::ptree& pt, const std::wstring & pwd)
{
	auto p0 = pt.get_optional<std::string>("master_name");
	if (p0) SetMasterId(*p0);

	auto p1 = pt.get_optional<std::string>("application_name");
	if (p1) SetAppId(*p1);

	auto p2 = pt.get_optional<std::string>("serial_num_key");
	if (p2) SetSnPassword(*p2);

	auto p3 = pt.get_optional<std::string>("master_key");
	if (p3)
	{
		PrivateKeyFromString(m_master_key_private, *p3);
		SetPrivateKey(m_master_key_private);
	}

	m_delivery_store.CloseDb();
	auto p4 = pt.get_optional<std::string>("database");
	if (p4)
	{
		std::wstring str_db;
		jcvos::Utf8ToUnicode(str_db, *p4);
		m_database_path = pwd + L"\\"+ str_db;
	}
	else m_database_path = pwd + L"\\authority.db";
	m_delivery_store.OpenDb(m_database_path);
}

void CApplicationInfo::SaveAppInfo(const std::wstring& fn)
{
	boost::property_tree::ptree pt;
	AppInfoToProperty(pt);

	boost::property_tree::ptree root_pt;
	root_pt.put_child("application_info", pt);

	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, fn);
	boost::property_tree::write_xml(str_fn, root_pt);
}

void CApplicationInfo::LoadAppInfo(const std::wstring& path)
{
	// 获取全路径名
	std::wstring fullpath, fn;
	jcvos::ParseFileName(path, fullpath, fn);

	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, path);

	boost::property_tree::ptree root_pt;
	boost::property_tree::read_xml(str_fn, root_pt);
	const boost::property_tree::ptree& app_pt = root_pt.get_child("application_info");

	PropertyToAppInfo(app_pt, fullpath);
}

void CApplicationInfo::SaveDelivery(const std::wstring& fn)
{
	//boost::property_tree::ptree app_pt;
	//AppInfoToProperty(app_pt);
	//boost::property_tree::ptree delivery_pt;
	//for (auto it = m_delivery_map.begin(); it != m_delivery_map.end(); ++it)
	//{
	//	boost::property_tree::ptree dpt;
	//	CDeliveryInfo* dd = it->second;
	//	dpt.put("sn", dd->m_sn);
	//	dpt.put("app_id", dd->m_app_id);
	//	dpt.put("registered", dd->m_registered);
	//	if (dd->m_registered) 	dpt.put("register_date", dd->m_register_date);

	//	std::string key;
	//	HexEncode<CryptoPP::Base64Encoder>(key, dd->m_delivery_key, sizeof(DELIVERY_KEY));
	//	dpt.put("key", key);
	//	delivery_pt.add_child("delivery", dpt);
	//}
	//boost::property_tree::ptree root;
	//root.put_child("application_info", app_pt);
	//root.put_child("delivery_set", delivery_pt);
	//boost::property_tree::ptree pt;
	//pt.put_child("deliveries", root);

	//std::string str_fn;
	//jcvos::UnicodeToUtf8(str_fn, fn);
	//boost::property_tree::write_xml(str_fn, pt);
}

void CApplicationInfo::LoadDelivery(const std::wstring& fn)
{
	//std::string str_fn;
	//jcvos::UnicodeToUtf8(str_fn, fn);

	//boost::property_tree::ptree root_pt;
	//boost::property_tree::read_xml(str_fn, root_pt);
	//ClearDeliveryMap();

	//const boost::property_tree::ptree& app_pt = root_pt.get_child("deliveries.application_info");
	//PropertyToAppInfo(app_pt);
	//const boost::property_tree::ptree& delivery_pt = root_pt.get_child("deliveries.delivery_set");
	//for (auto it = delivery_pt.begin(); it != delivery_pt.end(); ++it)
	//{
	//	const boost::property_tree::ptree& dpt = it->second;
	//	DWORD sn = dpt.get<DWORD>("sn");
	//	const std::string& str_key = dpt.get<std::string>("key");
	//	DELIVERY_KEY delivery_key;
	//	HexDecode<CryptoPP::Base64Decoder>(str_key, delivery_key, sizeof(DELIVERY_KEY));
	//	CDeliveryInfo* pd = new CDeliveryInfo(this, sn, delivery_key);
	//	pd->m_registered = dpt.get<bool>("registered");
	//	if (pd->m_registered) pd->m_register_date = dpt.get<boost::gregorian::date>("register_date");
	//	m_delivery_map.insert(std::make_pair(sn, pd));
	//}
}

void CApplicationInfo::EncodeDelivery(Authority::DELIVERY_KEY& delivery_key,/* DELIVERY& delivery,*/ DWORD sn)
{
	Authority::DELIVERY delivery;
	//DWORD scramble1, scramble2;
	//memcpy_s(&scramble1, sizeof(DWORD), m_snpassword_finger + 12, sizeof(DWORD));	// 避免暴露
	//memcpy_s(&scramble2, sizeof(DWORD), m_snpassword_finger + 16, sizeof(DWORD));	// 避免暴露

	delivery.m_app_id = m_id_appid ^ m_scramble1;
	delivery.m_sn = sn ^ m_scramble2; 
	// 随机数，避免暴露明文
	int x = rand(), y = rand();
	delivery.m_hash = MAKELONG(x, y);
	// crc
	delivery.m_crc = 0;
	DWORD crc = CalculateCrc((BYTE*)(&delivery), sizeof(delivery));
	delivery.m_crc = crc;
	delivery.m_app_id ^= crc;	// 避免出现重复的内容
	// encrypt
	//const AES_KEY& key = m_app_info->GetSnPassword();
	AESEncrypt(delivery_key, DELIVERY_LEN, (BYTE*)&delivery, sizeof(delivery), m_snpassword);
}

bool CApplicationInfo::DecodeDelivery(Authority::DELIVERY_KEY& delivery_key, DWORD& sn, DWORD& app_id)
{
	Authority::DELIVERY delivery;
	AESDecrypt(delivery_key, DELIVERY_LEN, (BYTE*)&delivery, sizeof(delivery), m_snpassword);
	DWORD remote_crc = delivery.m_crc;
	delivery.m_app_id ^= remote_crc;
	app_id = (delivery.m_app_id ^ m_scramble1);
	sn = (delivery.m_sn ^ m_scramble2);

	delivery.m_crc = 0;
	DWORD local_crc = CalculateCrc((BYTE*)(&delivery), sizeof(delivery));
	if (remote_crc != local_crc)
	{
		LOG_ERROR(L"[err] CRC does not match in delivery key");
		return false;
	}
	if (app_id != m_id_appid)
	{
		LOG_ERROR(L"[err] App Id does not match in delivery key");
		return false;
	}
	return true;
}

void CApplicationInfo::GenerateDelivery(DWORD start, int count)
{
	DWORD sn = start;
	
	srand((UINT)time(0));
	for (int ii = 0; ii < count; ++ii, ++sn)
	{	// database 确保 sn 唯一，检查sn是否已经存在
		Authority::DELIVERY_KEY delivery_key;
		EncodeDelivery(delivery_key, sn);
		CDeliveryInfo pd(this, sn, delivery_key);
		m_delivery_store.InsterDelivery(pd);
	}
}

void CApplicationInfo::ResetCursor(void)
{
	if (m_iterator) m_delivery_store.ResetEnumerate(m_iterator);
	m_iterator = m_delivery_store.BeginEnumerate(m_id_appid);
}

bool CApplicationInfo::NextDeliveryItem(CDeliveryInfo& delivery)
{
	JCASSERT(m_iterator);
	return m_delivery_store.NextEnumerate(delivery, m_iterator);
}

bool CApplicationInfo::ExportDelivery(const std::wstring& fn, int format)
{	// csv
	FILE* file = NULL;
	_wfopen_s(&file, fn.c_str(), L"w+");
	if (file == NULL) THROW_ERROR(ERR_APP, L"failed on opening file %s", fn.c_str());
	// write head
	fprintf_s(file, "sn,app_id,delivery\n");

	sqlite3_stmt * stmt=m_delivery_store.BeginEnumerate();
	while (1)
	{
		CDeliveryInfo delivery(this);
		bool br = m_delivery_store.NextEnumerate(delivery, stmt);
		if (!br || !delivery.IsAvailable()) break;
		std::string key = delivery.GetDeliveryKey();
		fprintf_s(file, "%d,%08X,%s\n", delivery.m_sn, m_id_appid, key.c_str());
	}
	m_delivery_store.ResetEnumerate(stmt);
	fclose(file);
	return true;
}

//bool CApplicationInfo::GenerateLicense(std::string& license_out, const std::string& str_delivery_key, 
//		const std::wstring & computer_name, UINT32 period, bool internal_use)

// 此函数仅用于发行测试license, 并且不会记录
bool CApplicationInfo::GenerateLicense(std::string& license, const std::string& str_delivery_key, const std::wstring& computer_name, UINT32 period, bool internal_use)
{
	Authority::DELIVERY_KEY delivery_key;
	DWORD sn, app_id;
	crypto::HexDecode<CryptoPP::Base32Decoder>(str_delivery_key, delivery_key, sizeof(delivery_key));
	bool br = DecodeDelivery(delivery_key, sn, app_id);
	if (!br || (app_id != m_id_appid))
	{
		LOG_ERROR(L"[err] failed on decoding delivery key");
		return false;
	}

	CDeliveryInfo delivery;
	br = m_delivery_store.FindDelivery(delivery, app_id, sn);
	if (!br || !delivery.IsEqual(delivery_key))
	{
		LOG_ERROR(L"[err] serial number or delivery key does not match");
		return false;
	}
//	delivery.m_user_name.resize(user_name.size());
//	transform(user_name.begin(), user_name.end(), delivery.m_user_name.begin(), ::toupper);
	delivery.m_computer_name.resize(computer_name.size());
	transform(computer_name.begin(), computer_name.end(), delivery.m_computer_name.begin(), ::toupper);
	delivery.m_register_date = boost::gregorian::day_clock::universal_day();
	if (period == 0) delivery.m_period = LICENSE_PERIOD_INFINITE;
	else delivery.m_period = period;
//	delivery.m_period = period;
//	delivery.m_rebound_count = INIT_REBOUND_COUNT;

	br = GenerateLicense(license, delivery, internal_use);

	return br;
}

bool CApplicationInfo::GenerateLicense(std::string& license_out, CDeliveryInfo & delivery, bool internal_use)
{
	// Find delivery info by delivery key
	//Authority::DELIVERY_KEY delivery_key;
	//DWORD sn, app_id;
	//crypto::HexDecode<CryptoPP::Base32Decoder>(str_delivery_key, delivery_key, sizeof(delivery_key));
	//bool br = DecodeDelivery(delivery_key, sn, app_id);
	//if (!br || (app_id != m_id_appid))
	//{
	//	LOG_ERROR(L"[err] failed on decoding delivery key");
	//	return false;
	//}

	//CDeliveryInfo delivery;
	//br = m_delivery_store.FindDelivery(delivery, app_id, sn);
	//if (!br || !delivery.IsEqual(delivery_key) )
	//{
	//	LOG_ERROR(L"[err] serial number or delivery key does not match");
	//	return false;
	//}
	// 对于internal use，不检查，可以重复发行。
	/*
	if (!internal_use && delivery.IsRegistered())
	{	
		LOG_ERROR(L"[err] key has already registered");
		return false;
	}
	*/

	// 设置Header, 准备buffer
	jcvos::auto_array<BYTE> license_buf(4096, 0);
	BYTE* ptr = license_buf;
	size_t license_size = 0;
	Authority::LICENSE_HEADER* header = (Authority::LICENSE_HEADER*)(ptr);
	header->m_app_id = m_id_appid;
//	header->m_licnes_id = (WORD)sn;
	header->m_type = (BYTE)Authority::LT_DELIVERY;
	header->m_size = Authority::DELIVERY_LICENSE::GetSize();
	header->m_crc = header->CalculateCrc(1);
	ptr += LICENSE_BLOCK_SIZE;
	license_size++;

	Authority::DELIVERY_LICENSE* license = (Authority::DELIVERY_LICENSE*)(ptr);
	memcpy_s(license->m_delivery, DELIVERY_LEN, delivery.m_delivery_key, DELIVERY_LEN);
	if (internal_use) 	license->m_version = DELIVERY_VERSION_INTERNAL;
	else				license->m_version = DELIVERY_VERSION_NORMAL;
	license->m_master_id = m_id_masterid;
//	license->m_start_date = boost::gregorian::day_clock::universal_day();
	license->m_start_date = delivery.m_register_date;
	license->m_serial_num = (WORD)delivery.m_sn;
	//if (delivery.m_period == 0) license->m_period = LICENSE_PERIOD_INFINITE;
	//else license->m_period = delivery.m_period;
	license->m_period = delivery.m_period;

	if (delivery.m_computer_name.empty())	license->m_computer_name = COMPUTER_NAME_IGNOR;
	else license->m_computer_name = crypto::ComputerNameHash(delivery.m_computer_name);

	// set next
	license->m_pre_crc = header->m_crc;
	license->m_app_id = 0;
	//license-> = 0;
	license->m_type = (BYTE)Authority::LT_ENDOF_LICENSE;
	license->m_size = 0;
	license->m_crc = license->CalculateCrc(header->m_size);
	license_size += license->GetSize();

//	static const size_t fixed_len = 128;

	// 发行
	size_t cipher_len = crypto::FillSalt(license_buf, license_size * LICENSE_BLOCK_SIZE, m_rsa_block_size, license_buf.size());
	jcvos::auto_array<BYTE> out_buf(cipher_len, 0);
	crypto::RSADecrypt(license_buf, cipher_len, out_buf, cipher_len, m_master_key_private);
	crypto::HexEncode<CryptoPP::Base64Encoder>(license_out, out_buf, m_rsa_block_size);
	delivery.m_register_date = license->m_start_date;

	// 注册
//	m_delivery_store.RegisterLicense(delivery, *license, internal_use);
	return true;
}

bool CApplicationInfo::GenerateDriveLicense(std::string& license_out, const std::string& str_delivery_key, const std::vector<std::wstring>& drive_list)
{
	// Find delivery info by delivery key
	Authority::DELIVERY_KEY delivery_key;
	DWORD sn, app_id;
	crypto::HexDecode<CryptoPP::Base32Decoder>(str_delivery_key, delivery_key, sizeof(delivery_key));
	bool br = DecodeDelivery(delivery_key, sn, app_id);
	if (!br || (app_id != m_id_appid))
	{
		LOG_ERROR(L"[err] failed on decoding delivery key");
		return false;
	}

	CDeliveryInfo delivery;
	br = m_delivery_store.FindDelivery(delivery, app_id, sn);
	if (!br || !delivery.IsEqual(delivery_key))
	{
		LOG_ERROR(L"[err] serial number or delivery key does not match");
		return false;
	}

	// 设置Header, 准备buffer
	jcvos::auto_array<BYTE> license_buf(4096, 0);
	BYTE* ptr = license_buf;
	size_t license_size = 0;
	Authority::LICENSE_HEADER* header = (Authority::LICENSE_HEADER*)(ptr);
	header->m_app_id = m_id_appid;
//	header->m_licnes_id = (WORD)sn;
	header->m_type = (BYTE)Authority::LT_DRIVE;
	ptr += LICENSE_BLOCK_SIZE;
	license_size++;

	// 设置Drive List
	Authority::DRIVE_LICENSE* drive_license = (Authority::DRIVE_LICENSE*)(ptr);
	size_t remain = license_buf.len() - 2 * LICENSE_BLOCK_SIZE;
	size_t drive_buf_len=0;
	char* ch = drive_license->m_drives;
	for (auto it = drive_list.begin(); it != drive_list.end(); ++it)
	{
		std::string str_drive;
		jcvos::UnicodeToUtf8(str_drive, *it);
		size_t len = str_drive.size()+1;
		if (remain < len) THROW_ERROR(ERR_APP, L"licnese buffer is not enough");
		strcpy_s(ch, len, str_drive.c_str());
		ch += len;
		drive_buf_len += len;
		remain -= len;
	}
	if (remain <=0 ) THROW_ERROR(ERR_APP, L"licnese buffer is not enough");
	*ch = 0;
	drive_buf_len++;

	header->m_size = drive_license->GetSize();
	header->m_crc = header->CalculateCrc(1);

	// set nest
	drive_license->m_pre_crc = header->m_crc;
	drive_license->m_app_id = 0;
//	drive_license->m_licnes_id = 0;
	drive_license->m_type = (BYTE)Authority::LT_ENDOF_LICENSE;
	drive_license->m_size = 0;
	drive_license->m_crc = drive_license->CalculateCrc(header->m_size);
	license_size += drive_license->GetSize();

	// 发行
	size_t cipher_len = crypto::FillSalt(license_buf, license_size * LICENSE_BLOCK_SIZE, m_rsa_block_size, license_buf.size());
	jcvos::auto_array<BYTE> out_buf(cipher_len, 0);
	crypto::RSADecrypt(license_buf, cipher_len, out_buf, cipher_len, m_master_key_private);
	crypto::HexEncode<CryptoPP::Base64Encoder>(license_out, out_buf, m_rsa_block_size);

	return true;
}

bool CApplicationInfo::VerifyLicense(const std::string& license, boost::property_tree::wptree& authority)
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

//	authority.put(L"version", license_set->m_version);
//	authority.put(L"number", license_set->m_license_num);

	// 解析
	// 解析头
	Authority::LICENSE_HEADER* header = reinterpret_cast<Authority::LICENSE_HEADER*>(cipher_buf.get_ptr());
	DWORD crc = header->CalculateCrc(1);
	if (crc != header->m_crc)
	{
		LOG_ERROR(L"[err] CRC does not match, org crc=%08X, check crc=%08X", header->m_crc, crc);
		return false;
	}
	Authority::LICENSE_HEADER* cur_license = header + 1;
	while (1)
	{
		boost::property_tree::wptree license_pt;
		// 检查 当前License
		crc = cur_license->CalculateCrc(header->m_size);
		if (crc != cur_license->m_crc)
		{
			LOG_ERROR(L"[err] CRC does not match, header crc=%08X, check crc=%08X", cur_license->m_crc, crc);
			return false;
		}
		if (cur_license->m_pre_crc != header->m_crc)
		{
			LOG_ERROR(L"[err] CRC does not match previous, pre crc=%08X, header=%08X", header->m_crc, cur_license->m_pre_crc);
			return false;
		}
		// 解码
		license_pt.put(L"app_id", header->m_app_id);
		if (header->m_type == Authority::LT_DELIVERY)
		{
			license_pt.put(L"type", L"delivery");
			Authority::DELIVERY_LICENSE* local_license = reinterpret_cast<Authority::DELIVERY_LICENSE*>(cur_license);
			license_pt.put(L"version", local_license->m_version);
			license_pt.put(L"master_id", local_license->m_master_id);
			license_pt.put(L"date", local_license->m_start_date);
			license_pt.put(L"period", local_license->m_period);
			license_pt.put(L"serial_number", local_license->m_serial_num);

			license_pt.put(L"computer_name", local_license->m_computer_name);
		}
		else if (header->m_type = Authority::LT_DRIVE)
		{
			Authority::DRIVE_LICENSE* local_license = reinterpret_cast<Authority::DRIVE_LICENSE*>(cur_license);
			license_pt.put(L"type", L"drives");
			char* dev = local_license->m_drives;
			while(*dev)
			{
				std::wstring str_device;
				jcvos::Utf8ToUnicode(str_device, dev);
				boost::property_tree::wptree pt;
				pt.put(L"drive", str_device);
				license_pt.add_child(L"drives", pt);
				dev += (strlen(dev)+1);
			}
		}
		authority.put_child(L"license", license_pt);
		Authority::LICENSE_HEADER * next = cur_license + header->m_size;
		header = cur_license;
		if (header->m_type == Authority::LT_ENDOF_LICENSE) break;
		cur_license = next;

	}
	return true;
}

bool CApplicationInfo::AuthorizeRegister(std::string& license, const std::string& str_delivery_key, 
	const std::wstring& computer_name, const std::wstring& user_name)
{
	// 检查用户名没有注册过
	if (user_name.empty()) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_USERNAME, L"user name cannot be empty");

	CDeliveryInfo d0;
	bool br = m_delivery_store.FindDeliveryByName(d0, user_name);
	if (br) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_USERNAME,
		L"the user name (%s) has already registered", user_name.c_str());

	// 解码delivery key, 然后查找delievery info
	Authority::DELIVERY_KEY delivery_key;
	DWORD sn, app_id;

	crypto::HexDecode<CryptoPP::Base32Decoder>(str_delivery_key, delivery_key, sizeof(delivery_key));
	br = DecodeDelivery(delivery_key, sn, app_id);
	if (!br || (app_id != m_id_appid)) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_WRONG_DELIVERY, 
		L"failed on decoding delivery key");

	CDeliveryInfo delivery;
	br = m_delivery_store.FindDelivery(delivery, app_id, sn);
	if (!br || !delivery.IsEqual(delivery_key)) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_WRONG_DELIVERY,
		L"serial number or delivery key does not match");

	// 对于internal use，不检查，可以重复发行。
	if (delivery.IsRegistered()) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_REGISTER, L"key has already registered");

	// 转换成大写
	delivery.m_user_name.resize(user_name.size());
	transform(user_name.begin(), user_name.end(), delivery.m_user_name.begin(), ::toupper);
//	delivery.m_user_name = user_name_upper;
	delivery.m_computer_name.resize(computer_name.size());
	transform(computer_name.begin(), computer_name.end(), delivery.m_computer_name.begin(), ::toupper);
	delivery.m_register_date = boost::gregorian::day_clock::universal_day();
	delivery.m_period = LICENSE_PERIOD_INFINITE;
	delivery.m_rebound_count = INIT_REBOUND_COUNT;

//	br = GenerateLicense(license, str_delivery_key, computer_name, 0, false);
	br = GenerateLicense(license, delivery, false);
	if (!br) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_LICENSE, L"failed on generating license");
	//{
	//	LOG_ERROR(L"[err] failed on generating license");
	//	return false;
	//}

	br = m_delivery_store.RegisterLicense(delivery,/* *license,*/ false);
	if (!br) THROW_ERROR_EX(ERR_APP, Authority::AUTHORIZE_REGISTER, L"failed on register license");
	//{
	//	LOG_ERROR(L"[err] failed on register license");
	//	return false;
	//}
	return true;
}

bool CApplicationInfo::Reauthorize(std::string& license, const std::wstring& user_name)
{
	if (user_name.empty())
	{
		LOG_ERROR(L"user name cannot be empty");
		return false;
	}
	// 用户名装大写
	//std::wstring user_name_upper;
	//user_name_upper.resize(user_name.size());
	//transform(user_name.begin(), user_name.end(), user_name_upper.begin(), ::toupper);

	CDeliveryInfo delivery;
	bool br = m_delivery_store.FindDeliveryByName(delivery, user_name);
	if (!br || delivery.m_user_name.empty())
	{
		LOG_ERROR(L"[err] user (%s) does not exist", user_name.c_str());
		return false;
	}
	delivery.m_period = LICENSE_PERIOD_INFINITE;
	GenerateLicense(license, delivery, false);
	return true;
}

int CApplicationInfo::GetReboundCount(const std::wstring& user_name)
{
	if (user_name.empty())
	{
		LOG_ERROR(L"user name cannot be empty");
		return -1;
	}
	// 用户名装大写
	//std::wstring user_name_upper;
	//user_name_upper.resize(user_name.size());
	//transform(user_name.begin(), user_name.end(), user_name_upper.begin(), ::toupper);

	CDeliveryInfo delivery;
	bool br = m_delivery_store.FindDeliveryByName(delivery, user_name);
	if (!br || delivery.m_user_name.empty())
	{
		LOG_ERROR(L"[err] user (%s) does not exist", user_name.c_str());
		return -1;
	}
	return delivery.GetReboundCount();
}

#define BUF_SIZE 4096

void CApplicationInfo::SetPrivateKey(const CryptoPP::RSA::PrivateKey& key)
{
	m_master_key_public = CryptoPP::RSA::PublicKey(key);

	jcvos::auto_array<BYTE> key_buf(BUF_SIZE, 0);
	CryptoPP::ArraySink sink(key_buf, BUF_SIZE);
	m_master_key_public.Save(sink);
	size_t buf_size = BUF_SIZE - sink.AvailableSize();
//	m_master_key_private.GetValue<size_t>("KeySize", m_rsa_block_size);
	m_rsa_block_size = m_master_key_private.MaxImage().ByteCount();
	Sha1(m_master_finger, key_buf, buf_size);
}

void CApplicationInfo::Sha1(SHA1_OUT& out, BYTE* in, size_t in_len)
{
	JCASSERT(in);
	CryptoPP::SHA1 hash;
	hash.Update(in, in_len);
	hash.Final(out);
}

void CApplicationInfo::PrivateKeyToString(std::string &str, const CryptoPP::RSA::PrivateKey& key)
{
//	jcvos::auto_array<BYTE> key_buf(1024, 00);
//	CryptoPP::ArraySink sink(key_buf, 1024);
//	key.Save(sink);
//	size_t key_size = 1024 - sink.AvailableSize();
//	LOG_DEBUG(L"key size = %zd", key_size);

	CryptoPP::ByteQueue que;
	key.Save(que);

//	StringSink str_sink(str);
	CryptoPP::Base64Encoder encoder(new CryptoPP::StringSink(str));
//	key_size = encoder.Put(key_buf, key_size);
	que.CopyTo(encoder);
	encoder.MessageEnd();

	//.CopyTo(str_sink);
	//str_sink.MessageEnd();
}

void CApplicationInfo::PrivateKeyFromString(CryptoPP::RSA::PrivateKey& key, const std::string &str)
{
	// string to 
	CryptoPP::StringSource source(str, true);
	CryptoPP::Base64Decoder decoder;
	source.TransferTo(decoder);
	decoder.MessageEnd();

	CryptoPP::ByteQueue que;
	decoder.CopyTo(que);
	key.Load(que);
}

void CApplicationInfo::AESEncrypt(BYTE* cipher_buf, size_t cipher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	JCASSERT(plane_len <= CryptoPP::AES::BLOCKSIZE);
	JCASSERT(cipher_len >= CryptoPP::AES::BLOCKSIZE);
//	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(cipher_buf, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Encryption encryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	memcpy_s(cipher_buf, CryptoPP::AES::BLOCKSIZE, plane_buf, plane_len);
	encryption.ProcessBlock(cipher_buf);

	//BYTE vv[16];
	//memcpy_s(vv, 16, iv, 16);
	//CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	//decryption.ProcessBlock(vv);
}

void CApplicationInfo::AESDecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	JCASSERT(plane_len >= CryptoPP::AES::BLOCKSIZE);
	JCASSERT(cipher_len <= CryptoPP::AES::BLOCKSIZE);
	//BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(plane_buf, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	memcpy_s(plane_buf, plane_len, cipher_buf, cipher_len);
	decryption.ProcessBlock(plane_buf);
}

void CApplicationInfo::CBCAESEncrypt(BYTE* cipher_buf, size_t cipher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(iv, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Encryption encryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Encryption cbc_encryption(encryption, iv);
	CryptoPP::StreamTransformationFilter encryptor(cbc_encryption, new CryptoPP::ArraySink(cipher_buf, cipher_len));
	encryptor.Put(plane_buf, plane_len);
	encryptor.MessageEnd();
}

void CApplicationInfo::CBCAESDecrypt(const BYTE* cipher_buf, size_t cipher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key)
{
	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(iv, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Decryption decryption(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Decryption cbc_decryption(decryption, iv);
	CryptoPP::StreamTransformationFilter decryptor(cbc_decryption, new CryptoPP::ArraySink(plane_buf, plane_len));
	decryptor.Put(cipher_buf, cipher_len);
	decryptor.MessageEnd();
}

DWORD CApplicationInfo::CalculateCrc(BYTE* buf, size_t len)
{
	DWORD crc_out;
	CryptoPP::CRC32 crc;
	crc.Update(buf, len);
	crc.TruncatedFinal((BYTE*)(&crc_out), sizeof(DWORD));
	return crc_out;
}

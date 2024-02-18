#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <boost/uuid/uuid.hpp>
#include <cryptopp/rsa.h>


#include "../include/delivery_info.h"
#include <map>

class CApplicationInfo
{
public:	
	CApplicationInfo(void);
	~CApplicationInfo(void);

	enum KEY_FILE_FORMAT {KEY_FILE_TEXT, KEY_FILE_BINARY} ;

public:
	void SetMasterId(const std::string &masterid);
	const std::string& GetMasterName(void) const { return m_string_masterid; }
	const boost::uuids::uuid& GetMasterUuid(void) const { return m_uuid_masterid; }
	DWORD GetMasterId(void) const { return m_id_masterid; }

	void SetAppId(const std::string& appid);
	const std::string& GetAppName(void) const { return m_string_appid; }
	const boost::uuids::uuid& GetAppUuid(void) const { return m_uuid_appid; }
	DWORD GetAppId(void) const { return m_id_appid; }

	// key len: 字节单位
	bool GenerateMasterKey(size_t key_len);
	bool SaveMasterKey(const std::wstring& fn, KEY_FILE_FORMAT format);
	bool LoadMasterKey(const std::wstring& fn, KEY_FILE_FORMAT format);
	bool ExportPublicKey(const std::wstring& fn, KEY_FILE_FORMAT format);
	std::string GetMasterFinger(void);

	bool SetSnPassword(const std::string& password);
	const std::string& GetPassword(void) const { return m_sn_password; }
	std::string GetSnPasswordFinger(void);

	void SaveAppInfo(const std::wstring& fn);
	void LoadAppInfo(const std::wstring& fn);
	void SaveDelivery(const std::wstring& fn);
	void LoadDelivery(const std::wstring& fn);

	void GenerateDelivery(DWORD start, int count);
	void ResetCursor(void);// { m_iterator = m_delivery_map.begin(); }

	bool NextDeliveryItem(CDeliveryInfo& delivery);
	bool ExportDelivery(const std::wstring& fn, int format=0);

	//bool PrepareLicense(const std::string & delivery_key);
	//bool AddLicense(UINT32 period);
	//bool GenerateLicense(std::string& license);


	bool GenerateLicense(std::string &license, const std::string& delivery_key, 
		const std::wstring & computer_name, UINT32 period, bool internal_use);
	bool GenerateLicense(std::string &license, CDeliveryInfo & delivery, bool internal_use);
	bool GenerateDriveLicense(std::string& license, const std::string& delivery_key, 
		const std::vector<std::wstring>& drive_list);
	bool VerifyLicense(const std::string& license, boost::property_tree::wptree& authority);
	bool AuthorizeRegister(std::string& license, const std::string& delivery_key, const std::wstring& computer_name,
		const std::wstring& user_name);
	bool Reauthorize(std::string& license, const std::wstring& user_name);
	bool Rebound(std::string& license, const std::wstring& computer_name, const std::wstring& user_name);
	int GetReboundCount(const std::wstring& user_name);


public:
	friend class CDeliveryInfo;

protected:
	void ClearDeliveryMap(void);
	void AppInfoToProperty(boost::property_tree::ptree& pt);
	// pwd：config file的当前路径，用于读取database等
	void PropertyToAppInfo(const boost::property_tree::ptree& pt, const std::wstring & pwd);
	//void SetStringId(const std::string& string_id, boost::uuids::uuid& uuid, DWORD& id);
	void SetPrivateKey(const CryptoPP::RSA::PrivateKey& key);
	//const AES_KEY& GetSnPassword(void)const { return m_snpassword; }
	void EncodeDelivery(Authority::DELIVERY_KEY & delivery_key, /*DELIVERY& delivery,*/ DWORD sn);
	bool DecodeDelivery(Authority::DELIVERY_KEY & delivery_key, /*DELIVERY& delivery,*/ DWORD & sn, DWORD &app_id);


public:
	// 算法
	static void Sha1(SHA1_OUT& out, BYTE* in, size_t in_len);

	static void PrivateKeyToString(std::string &out, const CryptoPP::RSA::PrivateKey& key);
	static void PrivateKeyFromString(CryptoPP::RSA::PrivateKey& key, const std::string &in);
	static void AESEncrypt(BYTE* cihper_buf, size_t ciher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY & key);
	static void AESDecrypt(const BYTE* cihper_buf, size_t ciher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key);
	static void CBCAESEncrypt(BYTE* cihper_buf, size_t ciher_len, const BYTE* plane_buf, size_t plane_len, const AES_KEY & key);
	static void CBCAESDecrypt(const BYTE* cihper_buf, size_t ciher_len, BYTE* plane_buf, size_t plane_len, const AES_KEY& key);
	static DWORD CalculateCrc(BYTE* buf, size_t len);

protected:
	std::wstring m_database_path;		// 数据库路径
	CDeliveryStore m_delivery_store;
	// Master Id
	std::string m_string_masterid;
	boost::uuids::uuid m_uuid_masterid;
	DWORD m_id_masterid;						// 软件ID
	// App Id
	std::string m_string_appid;
	boost::uuids::uuid m_uuid_appid;
	DWORD m_id_appid;						// 软件ID
	// Master Key
//	size_t m_key_len_bit;		// key长度，bit单位
	size_t m_rsa_block_size;
	CryptoPP::RSA::PrivateKey m_master_key_private;
	CryptoPP::RSA::PublicKey m_master_key_public;
	BYTE m_master_finger[CryptoPP::SHA1::DIGESTSIZE];	// Master Key指纹 = public key的SHA1

	// 用于加密序列号的key
	std::string m_sn_password;
	BYTE m_snpassword_finger[CryptoPP::SHA1::DIGESTSIZE];	// Master Key指纹 = public key的SHA1
	AES_KEY m_snpassword;
	DWORD m_scramble1, m_scramble2;
	//BYTE m_keysn[CryptoPP::SHA1::DIGESTSIZE];
	//afx_msg void OnClickedSetKeySn();

	//std::map<DWORD, CDeliveryInfo*> m_delivery_map;
	//std::map<DWORD, CDeliveryInfo*>::const_iterator m_iterator;

	sqlite3_stmt* m_iterator;
};


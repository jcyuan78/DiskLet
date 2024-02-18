#pragma once

#include <lib_authorizer.h>
#include <sqlite3.h>

class CApplicationInfo;


class CDeliveryInfo
{
public:
	CDeliveryInfo(CApplicationInfo* app_info, DWORD sn, const Authority::DELIVERY_KEY & delivery_key);
	CDeliveryInfo(CApplicationInfo* app_info) : m_app_info(app_info), m_sn(0) {};
	CDeliveryInfo(void): m_app_info(NULL), m_sn(0) {};
	~CDeliveryInfo(void) {};
public:
	friend class CApplicationInfo;
	friend class CDeliveryStore;

public:
	bool IsAvailable(void) const { return m_sn != 0; }
	DWORD GetSerialNumber(void) const { return m_sn; }
	DWORD GetAppId(void) const { return m_app_id; }
	std::string GetDeliveryKey(void) const;
	void SetDeliveryKey(const std::string& key);
	bool IsRegistered(void) const { return m_registered; }
	const boost::gregorian::date& GetRegisterDate(void) const { return m_register_date; }
	const std::wstring& GetComputerName(void) const	{		return m_computer_name;	}
	//	std::wstring str;
	//	jcvos::Utf8ToUnicode(str, m_computer_name);
	//	return str;
	//}
	const std::wstring & GetUserName(void) const { return m_user_name; }
	//{
	//	std::wstring str;
	//	jcvos::Utf8ToUnicode(str, m_user_name);
	//	return str;
	//}
	int GetReboundCount() const { return m_rebound_count; }

protected:
	bool IsEqual(Authority::DELIVERY_KEY& key) const;
	UINT64 GetRegisterDateInt(void) const;
	//void Register(DELIVERY_LICENSE& license);

protected:
//	DELIVERY	m_delivery;
	DWORD m_sn;
	DWORD m_app_id;
//	BYTE m_delivery_key[DELIVERY_LEN];
	Authority::DELIVERY_KEY m_delivery_key;
	bool m_registered;
	boost::gregorian::date m_register_date;
	UINT m_period;
	CApplicationInfo* m_app_info;
	std::wstring m_computer_name;
	std::wstring m_user_name;
	int m_rebound_count = 0;
};


class CDeliveryStore
{
public:
	CDeliveryStore(void);
	~CDeliveryStore(void);

public:
	bool OpenDb(const std::wstring& fn);
	void CloseDb(void);
	void InsterDelivery(const CDeliveryInfo& delivery);
	bool FindDelivery(CDeliveryInfo& delivery, DWORD app_id, DWORD sn);
	bool FindDeliveryByName(CDeliveryInfo& delivery, const std::wstring& user_name);
	sqlite3_stmt* BeginEnumerate(DWORD app_id=0);
	bool NextEnumerate(CDeliveryInfo & delivery, sqlite3_stmt * stmt);
	void ResetEnumerate(sqlite3_stmt* stmt);
	void UpdateDelivery(const CDeliveryInfo& info);
	bool RegisterLicense(CDeliveryInfo& info, /*Authority::DELIVERY_LICENSE& license,*/ bool internal_use);
protected:
	void SetDeliveryByRow(CDeliveryInfo& delivery, sqlite3_stmt* stmt);
	bool ExecuteSQL(const char* sql);

protected:
	sqlite3* m_db;
//	sqlite3_stmt* m_stmt;

};

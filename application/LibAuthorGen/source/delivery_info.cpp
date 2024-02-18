#include "pch.h"
#include "../include/delivery_info.h"

#include "../include/application_info.h"

LOCAL_LOGGER_ENABLE(L"delivery", LOGGER_LEVEL_DEBUGINFO);


CDeliveryInfo::CDeliveryInfo(CApplicationInfo* app_info, DWORD sn, const Authority::DELIVERY_KEY& delivery_key)
{
	JCASSERT(app_info);
	m_app_info = app_info;
	m_app_id = m_app_info->GetAppId();
	m_sn = sn;
	memcpy_s(m_delivery_key, sizeof(m_delivery_key), delivery_key, sizeof(delivery_key));
	m_registered = false;
}

std::string CDeliveryInfo::GetDeliveryKey(void) const
{
	std::string str;
	crypto::HexEncode<CryptoPP::Base32Encoder>(str, m_delivery_key, DELIVERY_LEN, 4, "-");
	return str;
}

void CDeliveryInfo::SetDeliveryKey(const std::string& key)
{
	crypto::HexDecode<CryptoPP::Base32Decoder>(key, m_delivery_key, sizeof(Authority::DELIVERY_KEY));
}

bool CDeliveryInfo::IsEqual(Authority::DELIVERY_KEY& key) const
{
	int ir = memcmp(m_delivery_key, key, sizeof(key));
	return (ir==0);
}

UINT64 CDeliveryInfo::GetRegisterDateInt(void) const
{
	time_t time = 0;
	if (m_registered)
	{
		tm tt = boost::gregorian::to_tm(m_register_date);
		time = mktime(&tt);
	}
	return UINT64(time);
}

//void CDeliveryInfo::Register(DELIVERY_LICENSE& license)
//{
//	if (m_registered) THROW_ERROR(ERR_APP, L"delivery key has already registered");
//	m_registered = true;
//	m_register_date = license.m_start_date;
//}

//////////////////////////////////////////////////////////////////////////////////
// == 

CDeliveryStore::CDeliveryStore(void) : m_db(NULL)/*, m_stmt(NULL)*/
{
}

CDeliveryStore::~CDeliveryStore(void)
{
	if (m_db) sqlite3_close(m_db);
}

bool CDeliveryStore::OpenDb(const std::wstring& fn)
{
	JCASSERT(m_db == NULL);

	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, fn);
	int ir = sqlite3_open(str_fn.c_str(), &m_db);
	if (ir != SQLITE_OK)	THROW_ERROR(ERR_APP, L"failed on opening data base %s, res=%d", fn.c_str(), ir);
	return true;
}

void CDeliveryStore::CloseDb(void)
{
	if (m_db) sqlite3_close(m_db);
	m_db = NULL;
}

void CDeliveryStore::InsterDelivery(const CDeliveryInfo& delivery)
{
	jcvos::auto_array<char> sql(256, 0);
	UINT64 id = MAKEQWORD(delivery.m_app_id, delivery.m_sn);
	sprintf_s(sql, sql.size(),
		"INSERT INTO delivery (id, serial_num, app_id, delivery_key, register_date) VALUES (%lld, %d, %d, \"%s\", %lld);",
		id, delivery.m_sn, delivery.m_app_id, delivery.GetDeliveryKey().c_str(), delivery.GetRegisterDateInt());
	ExecuteSQL(sql);
}

bool CDeliveryStore::FindDelivery(CDeliveryInfo& delivery, DWORD app_id, DWORD sn)
{
	sqlite3_stmt* stmt = NULL;
	jcvos::auto_array<char> sql(256, 0);
	sprintf_s(sql, sql.size(), "SELECT * FROM delivery WHERE serial_num=%d AND app_id=%d;", sn, app_id);
	const char* tail = NULL;
	bool found = false;

	int ir = sqlite3_prepare_v2(m_db, sql, boost::numeric_cast<int>(strlen(sql)), &stmt, &tail);
	if (ir != SQLITE_OK || stmt==NULL) THROW_ERROR(ERR_APP, L"failed on preparing statement, code=%d", ir);
	ir = sqlite3_step(stmt);
	if (ir == SQLITE_ROW)
	{
		SetDeliveryByRow(delivery, stmt);
		found = true;
	}
	else if (ir == SQLITE_DONE)
	{
		LOG_ERROR(L"[err] cannot find delivery with sn=%d, app_id=%08X", sn, app_id);
	}
	else THROW_ERROR(ERR_APP, L"failed on calling step, ir=%d", ir);
	sqlite3_finalize(stmt);
	return found;
}

bool CDeliveryStore::FindDeliveryByName(CDeliveryInfo& delivery, const std::wstring& user_name)
{
	std::string str_user_name;
	jcvos::UnicodeToUtf8(str_user_name, user_name);
	// 用户名装大写
	std::string user_name_upper;
	user_name_upper.resize(str_user_name.size());
	transform(str_user_name.begin(), str_user_name.end(), user_name_upper.begin(), ::toupper);

	sqlite3_stmt* stmt = NULL;
	jcvos::auto_array<char> sql(256, 0);
	sprintf_s(sql, sql.size(), "SELECT * FROM delivery WHERE user_name='%s';", user_name_upper.c_str());
	const char* tail = NULL;
	bool found = false;

	int ir = sqlite3_prepare_v2(m_db, sql, boost::numeric_cast<int>(strlen(sql)), &stmt, &tail);
	if (ir != SQLITE_OK || stmt==NULL) THROW_ERROR(ERR_APP, L"failed on preparing statement, code=%d", ir);
	ir = sqlite3_step(stmt);
	if (ir == SQLITE_ROW)
	{
		SetDeliveryByRow(delivery, stmt);
		found = true;
	}
	else if (ir == SQLITE_DONE)
	{
		LOG_WARNING(L"[warning] cannot find delivery with user name=%s", user_name.c_str());
	}
	sqlite3_finalize(stmt);
	return found;
}

sqlite3_stmt * CDeliveryStore::BeginEnumerate(DWORD app_id)
{
	JCASSERT(m_db);

	sqlite3_stmt* stmt = NULL;
	jcvos::auto_array<char> sql(256, 0);
	if (app_id == 0)	sprintf_s(sql, sql.size(), "SELECT * FROM delivery;");
	else sprintf_s(sql, sql.size(), "SELECT * FROM delivery WHERE app_id=%d;", app_id);
	const char* tail = NULL;

	int ir = sqlite3_prepare_v2(m_db, sql, boost::numeric_cast<int>(strlen(sql)), &stmt, &tail);
	if (ir!=SQLITE_OK) THROW_ERROR(ERR_APP, L"failed on preparing statement, code=%d", ir);
	return stmt;
}

void CDeliveryStore::ResetEnumerate(sqlite3_stmt* stmt)
{
	int ir = sqlite3_finalize(stmt);
	if (ir != SQLITE_OK) THROW_ERROR(ERR_APP, L"failed on finalizing statement, code=%d", ir);
}

void CDeliveryStore::UpdateDelivery(const CDeliveryInfo& info)
{
	jcvos::auto_array<char> sql(256, 0);
	std::string str_computer_name, str_user_name;
	jcvos::UnicodeToUtf8(str_computer_name, info.m_computer_name);
	jcvos::UnicodeToUtf8(str_user_name, info.m_user_name);
	sprintf_s(sql, sql.size(), 
		"UPDATE delivery SET delivery_key='%s', register_date=%lld, computer_name='%s', user_name='%s', rebound_count=%d WHERE serial_num=%d AND app_id=%d;",
		info.GetDeliveryKey().c_str(), info.GetRegisterDateInt(), str_computer_name.c_str(), str_user_name.c_str(),
		info.m_rebound_count, 
		info.m_sn, info.m_app_id);
	JCASSERT(m_db);
	bool br = ExecuteSQL(sql);
	if (!br) THROW_ERROR(ERR_APP, L"failed on update db");
}

bool CDeliveryStore::RegisterLicense(CDeliveryInfo& info,/* Authority::DELIVERY_LICENSE& license,*/ bool internal_use)
{
	if (!internal_use && info.m_registered) THROW_ERROR(ERR_APP, L"delivery key has already registered");
	info.m_registered = true;
//	info.m_register_date = license.m_start_date;
	UpdateDelivery(info);
	return true;
}

bool CDeliveryStore::NextEnumerate(CDeliveryInfo& delivery, sqlite3_stmt * stmt)
{
	JCASSERT(stmt);
//	if (m_stmt == NULL) THROW_ERROR(ERR_APP, L"sql statement is not ready");
	int ir = sqlite3_step(stmt);

	if (ir == SQLITE_ROW /*|| ir == SQLITE_DONE*/)
	{
		SetDeliveryByRow(delivery, stmt);
		return true;
	}
	else if (ir == SQLITE_DONE) return false;
	else
	{
		if (ir!=SQLITE_OK) THROW_ERROR(ERR_APP, L"failed on executing statement, code=%d", ir);
		return false;	// End of the table
	}
	return false;
}



void CDeliveryStore::SetDeliveryByRow(CDeliveryInfo& delivery, sqlite3_stmt* stmt)
{
	JCASSERT(stmt);
	int col_count = sqlite3_column_count(stmt);
#ifdef _DEBUG
	// 显示所有的列
	for (int ii = 0; ii<col_count; ++ii)
	{
		std::wstring str_name;
		jcvos::Utf8ToUnicode(str_name, sqlite3_column_name(stmt, ii));
		LOG_DEBUG(L"column[%d], name=%s", ii, str_name.c_str());
	}
#endif
	delivery.m_sn = sqlite3_column_int(stmt, 1);						// SN
	delivery.m_app_id = sqlite3_column_int(stmt, 2);					// app_id

	// delivery key
	std::string str_key((const char*)sqlite3_column_text(stmt, 3));		
	delivery.SetDeliveryKey(str_key);
	delivery.m_registered = false;
	// register date
	time_t time = sqlite3_column_int64(stmt, 4);
	if (time != 0)
	{
		delivery.m_registered = true;
		tm tt;
		gmtime_s(&tt, &time);
		delivery.m_register_date = boost::gregorian::date_from_tm(tt);
	}
	// computer name
	const char* name = (const char*)(sqlite3_column_text(stmt, 5));
	if (name) jcvos::Utf8ToUnicode(delivery.m_computer_name, name);
	// user name
	name = (const char*)(sqlite3_column_text(stmt, 6));
	if (name)	jcvos::Utf8ToUnicode(delivery.m_user_name, name);
	// rebound count
	delivery.m_rebound_count = sqlite3_column_int(stmt, 7);
}

bool CDeliveryStore::ExecuteSQL(const char* sql)
{
	JCASSERT(m_db);
	char* err_msg;
	int ir = sqlite3_exec(m_db, sql, NULL, NULL, &err_msg);
	if (ir != SQLITE_OK)
	{
		THROW_ERROR(ERR_APP, L"failed on exe sql, code=%d, msg=%d", ir, err_msg);
		sqlite3_free(err_msg);
	}
	return true;
}

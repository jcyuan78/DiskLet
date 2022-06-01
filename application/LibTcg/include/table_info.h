///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "dta_lexicon.h"

#include <vector>
#include <map>
#include <boost/property_tree/json_parser.hpp>

class CUidMap;
class CTcgTokenBase;

class TYPE_INFO
{
public:
	virtual ~TYPE_INFO(void) {};
public:
	static TYPE_INFO* LoadType(const boost::property_tree::wptree& pt, CUidMap* map);
	virtual void ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const;
public:
	enum BASE_TYPE { ALTERNATIVE, ENUMERATION, LIST, SET, SIMPLE, UIDREF, };
public:
	DWORD m_id;
	std::wstring m_name;
	BASE_TYPE m_base;
	UINT m_max_len = 0;
	CUidMap* m_uid_map = nullptr;
};

class TYPE_ALTE_BASE : public TYPE_INFO
{
public:
	virtual void ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const;
	const TYPE_INFO* m_type[2];
};


class TYPE_LIST_BASE : public TYPE_INFO
{
public:
	virtual void ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const;
	const TYPE_INFO* m_element_type;
};

class TYPE_ENUM_BASE : public TYPE_INFO
{
public:
	virtual void ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const;
	std::vector<std::wstring> m_enum_list;
};




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== parameter or column info ====

class PARAM_INFO
{
	//public:
	//	static PARAM_INFO* Load(const boost::property_tree::wptree& pt, CUidMap* map);
public:
	void LoadParameter(const boost::property_tree::wptree& pt, CUidMap* map);
	void ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const;
public:
	typedef std::vector<PARAM_INFO> PARAM_LIST;
	enum PARAM_TYPE
	{
		OtherType,
		Err_MissingRequest,	// dummy type: 表示错误，遗漏了相应的requested parameter
		UidRef,
	};
public:
	DWORD			m_num;
	PARAM_TYPE		m_type_id;
	std::wstring	m_name;
	std::wstring	m_type;
	const TYPE_INFO* m_type_info = nullptr;
	bool			m_unique = false;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== table info ====
class TABLE_INFO
{
public:
	~TABLE_INFO(void);
public:
	static TABLE_INFO* LoadTable(const boost::property_tree::wptree& pt, CUidMap* map);
	const PARAM_INFO* GetColumn(UINT index) const;
public:
	DWORD m_id;
	std::wstring m_name;
	std::vector<PARAM_INFO*> m_columns;
};

class METHOD_INFO
{
public:
	UINT64 m_id;
	std::vector<PARAM_INFO> m_required_param;
	std::vector<PARAM_INFO> m_option_param;
};


class UID_INFO
{
public:
	//	UID_INFO(void) {};
		//~UID_INFO(void);

public:
	enum UIDCLASS { Unkonw = 0, Involing = 0x01, Method = 0x02, Object = 0x04, Table = 0x08 };
public:
	UINT64 m_uid;
	std::wstring m_name;
	UIDCLASS m_class;

	METHOD_INFO* m_method;
};


class CUidMap
{
public:
	typedef std::map<UINT64, UID_INFO> UID_MAP;
	typedef std::map<std::wstring, TYPE_INFO*> TYPE_MAP;
	typedef std::map<DWORD, TABLE_INFO*> TABLE_MAP;
	~CUidMap(void);

public:
	/// <summary> Load Uid Infors from json file </summary>
	/// <param name="fn"></param>
	/// <returns></returns>
	bool Load(const std::wstring& fn);
	void Clear(void);
	const UID_INFO* GetUidInfo(UINT64 uid) const;
	UID_INFO* GetUidInfo(UINT64 uid);
	const TYPE_INFO* FindType(const std::wstring& type_name);
	const TABLE_INFO* FindTable(DWORD tid);

public:
	static UINT64 Uid2Uint(const TCG_UID uid);

public:
	template <typename T>
	static T StringToUid(const std::wstring& str)
	{
		T val = 0;
		size_t len = sizeof(T);
		const wchar_t* ch = str.c_str();
		for (size_t ii = 0; ii < len; ++ii)
		{
			val <<= 8;
			int dd;
			swscanf_s(ch, L"%X", &dd);
			val |= dd;
			ch += 3;
		}
		return val;
	}
protected:
	UID_INFO::UIDCLASS StringToClass(const std::wstring& str);

	void LoadParameter(const boost::property_tree::wptree& pt, std::vector<PARAM_INFO>& param);

protected:
	UID_MAP m_map;
	TYPE_MAP m_type_map;
	TABLE_MAP m_table_map;
};


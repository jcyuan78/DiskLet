///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "../include/tcg-token.h"
LOCAL_LOGGER_ENABLE(L"tcg.token", LOGGER_LEVEL_NOTICE);

///////////////////////////////////////////////////////////////////////////////
//// ==== Uid Map ====

void CUidMap::LoadParameter(boost::property_tree::wptree& param_pt, std::vector<PARAM_INFO>& param_list)
{
	auto end_it = param_pt.end();
	for (auto it = param_pt.begin(); it != end_it; ++it)
	{
		PARAM_INFO param;
		boost::property_tree::wptree& pt = (*it).second;
		param.m_num = pt.get<DWORD>(L"Number");
		param.m_name = pt.get<std::wstring>(L"Name");
		param.m_type = pt.get<std::wstring>(L"Type");
		if (param.m_type == L"uidref") param.m_type_id = PARAM_INFO::UidRef;
		else param.m_type_id = PARAM_INFO::OtherType;
		param_list.push_back(param);
	}
}

bool CUidMap::Load(const std::wstring& fn)
{
	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, fn);

	boost::property_tree::wptree root_pt;
	boost::property_tree::read_json(str_fn, root_pt);
	// Load UIDs
	boost::property_tree::wptree& uids_pt = root_pt.get_child(L"UIDs");
	auto end_it = uids_pt.end();
	for (auto it = uids_pt.begin(); it != end_it; ++it)
	{
		UID_INFO uinfo;
		std::wstring& str_uid = (*it).second.get<std::wstring>(L"UID");
		uinfo.m_uid = StringToUid(str_uid);
		uinfo.m_name = (*it).second.get<std::wstring>(L"Name");
		std::wstring& str_class = (*it).second.get<std::wstring>(L"class");
		uinfo.m_class = StringToClass(str_class);
		uinfo.m_method = nullptr;
		m_map.insert(std::make_pair(uinfo.m_uid, uinfo));
	}

	// Load method defines
	boost::property_tree::wptree& methods_pt = root_pt.get_child(L"Methods");
	end_it = methods_pt.end();
	for (auto it = methods_pt.begin(); it != end_it; ++it)
	{
		boost::property_tree::wptree& pt = (*it).second;
		UINT64 uid = StringToUid(pt.get<std::wstring>(L"UID"));
		METHOD_INFO* method = new METHOD_INFO;
		UID_INFO* uidinfo = GetUidInfo(uid);
		if (uidinfo)
		{
			uidinfo->m_method = method;
		}
		//		if (uidinfo == nullptr)
		else
		{
			UID_INFO _uidinfo;
			LOG_NOTICE(L"add new uid=%016llX, ", uid);
			_uidinfo.m_uid = uid;
			_uidinfo.m_name = pt.get<std::wstring>(L"Name");
			_uidinfo.m_class = UID_INFO::Method;
			_uidinfo.m_method = method;
			m_map.insert(std::make_pair(uid, _uidinfo));
		}

		method->m_id = uid;
		auto req_param = pt.get_child_optional(L"Requested");
		if (req_param) LoadParameter((*req_param), method->m_required_param);
		auto opt_param = pt.get_child_optional(L"Optional");
		if (opt_param)	LoadParameter((*opt_param), method->m_option_param);
	}

	return true;
}

UINT64 CUidMap::StringToUid(const std::wstring& str)
{
	UINT64 val = 0;
	const wchar_t* ch = str.c_str();
	for (size_t ii = 0; ii < 8; ++ii)
	{
		val <<= 8;
		int dd;
		swscanf_s(ch, L"%X", &dd);
		val |= dd;
		ch += 3;
	}
	return val;
}

UID_INFO::UIDCLASS CUidMap::StringToClass(const std::wstring& str)
{
	UINT cc = 0;
	wchar_t* context = 0;
	wchar_t* src = const_cast<wchar_t*>(str.c_str());

	wchar_t* token = wcstok_s(src, L";", &context);
	while (token)
	{
		//if (token == nullptr) break;
		if (0) {}
		else if (wcscmp(token, L"invoking") == 0) cc |= UID_INFO::Involing;
		else if (wcscmp(token, L"method") == 0) cc |= UID_INFO::Method;
		else if (wcscmp(token, L"object") == 0) cc |= UID_INFO::Object;
		token = wcstok_s(NULL, L";", &context);
	}
	return (UID_INFO::UIDCLASS)(cc);
}

const UID_INFO* CUidMap::GetUidInfo(UINT64 uid) const
{
	auto it = m_map.find(uid);
	if (it == m_map.end()) return nullptr;
	else return &(it->second);
}

UID_INFO* CUidMap::GetUidInfo(UINT64 uid)
{
	auto it = m_map.find(uid);
	if (it == m_map.end()) return nullptr;
	else return &(it->second);
	//if (it->first) return &(it->second);
	//else return nullptr;
}

CUidMap::~CUidMap(void)
{
	m_map.clear();
}

void CUidMap::Clear(void)
{
	auto end_it = m_map.end();
	for (auto it = m_map.begin(); it != end_it; ++it)
	{
		delete it->second.m_method;
	}
}

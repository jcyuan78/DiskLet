///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "../include/itcg.h"
#include "../include/l0discovery.h"
#include "../include/tcg_token.h"
#include "tcg-packet.h"
#include "../include/tcg_parser.h"

LOCAL_LOGGER_ENABLE(L"security_parse", LOGGER_LEVEL_NOTICE);

template <typename T> T* ConvertTokenType(boost::property_tree::wptree& pt, CTcgTokenBase* token)
{
	T* tt = dynamic_cast<T*>(token);
	if (tt == nullptr)
	{
		pt.put_value(L"[err] token type does not match");
		THROW_ERROR(ERR_APP, L"token type does not match");
	}
	return tt;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == protocol list
class CProtocolList : public tcg::ISecurityObject
{
public:
	virtual void ToString(std::wostream & out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual void GetPayload(jcvos::IBinaryBuffer*& data, int index) {};
	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name) {}

public:
	void AddProtocol(BYTE protocol) { m_protocol_list.push_back(protocol); }
protected:
	std::vector<BYTE> m_protocol_list;
};

void ProtocolToString(std::wstring& str, BYTE protocol)
{
	switch (protocol)
	{
	case 00: str = L"Protocol_List"; break;
	case 01: str = L"TCG_01"; break;
	case 02: str = L"TCG_02"; break;
	case 03: str = L"TCG_03"; break;
	case 04: str = L"TCG_04"; break;
	case 05: str = L"TCG_05"; break;
	case 06: str = L"TCG_06"; break;
	case 0xEF: str = L"Ata_Passthrough"; break;
	default: str = L"Unknown_Protocol(" + std::to_wstring(protocol) + L")"; break;
	}
}

void CProtocolList::ToString(std::wostream& out, UINT layer, int opt)
{
	out << L"<Protocols>: " << std::endl;
	if (layer <= 0) return;
	for (auto it = m_protocol_list.begin(); it != m_protocol_list.end(); ++it)
	{
		std::wstring str_prot;
		ProtocolToString(str_prot, *it);
		out << L"    " << str_prot << std::endl;
	}
	out << L"</Protocols>" << std::endl;
}

void CProtocolList::ToProperty(boost::property_tree::wptree& prop)
{
	for (auto it = m_protocol_list.begin(); it != m_protocol_list.end(); ++it)
	{
		std::wstring str_prot;
		ProtocolToString(str_prot, *it);
		boost::property_tree::wptree pp;
		pp.put_value(str_prot);
		prop.push_back(std::make_pair(L"protocol", pp));
	}
}



// Security Parser为Single tone
jcvos::CStaticInstance<CSecurityParser>		g_parser;

void tcg::GetSecurityParser(tcg::ISecurityParser*& parser)
{
	JCASSERT(parser == nullptr);
	parser = static_cast<tcg::ISecurityParser*>(&g_parser);
}

bool CSecurityParser::ParseProtocolInfo(tcg::ISecurityObject * & obj, DWORD comid, const BYTE * buf, size_t data_len)
{
	if (comid == 0)
	{	// protocol list
		CProtocolList* plist = jcvos::CDynamicInstance<CProtocolList>::Create();
		if (data_len < 8)
		{
			LOG_ERROR(L"[err] data length (%zd) should > 8 ", data_len);
			return false;
		}
		WORD protocol_num = MAKEWORD(buf[7], buf[6]);
		if (data_len < (protocol_num + 8))
		{
			LOG_ERROR(L"[err] data length (%zd) too small, should >= (%d)", data_len, protocol_num + 8);
			return false;
		}
		for (WORD ii = 0; ii < protocol_num; ++ii)
		{
			plist->AddProtocol(buf[ii + 8]);
		}
		obj = static_cast<tcg::ISecurityObject *>(plist);
		return true;
	}
	return false;
}

bool CSecurityParser::ParseL0Discovery(tcg::ISecurityObject*& obj, const BYTE* data, size_t data_len)
{
	CTcgFeatureSet* _fset = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
	if (_fset == nullptr) THROW_ERROR(ERR_APP, L"failed on createing feature set");

	m_feature_description.Parse(*_fset, data, data_len);
	obj = static_cast<tcg::ISecurityObject*>(_fset);
	return true;
}

bool CSecurityParser::ParseTcgCommand(tcg::ISecurityObject*& obj, const BYTE* buf, size_t data_len, DWORD comid, bool receive)
{
	// ComPacket Parse
	CTcgComPacket* packet = jcvos::CDynamicInstance<CTcgComPacket>::Create();
	if (packet == nullptr) THROW_ERROR(ERR_APP, L"failed on creating Tcg Com Packet");
	packet->ParseData(buf, data_len, comid, receive);
	obj = static_cast<tcg::ISecurityObject*>(packet);
	return true;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool CSecurityParser::Initialize(const std::wstring & uid_config, const std::wstring & l0_config)
{
	m_feature_description.LoadConfig(l0_config);
	m_uid_map.Load(uid_config);
	CTcgTokenBase::SetUidMap(&m_uid_map);
	return true;
}

//bool CSecurityParser::ParseSecurityCommand(std::vector<tcg::ISecurityObject*>& out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive)
bool CSecurityParser::ParseSecurityCommand(tcg::ISecurityObject*& out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive)
{
	size_t buf_len = payload->GetSize();
	BYTE* buf = payload->Lock();
	bool br = ParseSecurityCommand(out, buf, buf_len, protocol, comid, receive);
	payload->Unlock(buf);
	return br;
}


bool CSecurityParser::ParseSecurityCommand(tcg::ISecurityObject*& out, const BYTE* buf, size_t buf_len, DWORD protocol, DWORD comid, bool receive)
{
	bool br;
	jcvos::auto_interface<tcg::ISecurityObject> object;
	if (protocol == 0)
	{	// security protocol information
		br = ParseProtocolInfo(object, comid, buf, buf_len);
		//if (object) out.push_back(object);
	}
	else if (protocol >= 1 && protocol <= 6)
	{	// tcg protocol
		if (comid == 1)		{	br = ParseL0Discovery(object, buf, buf_len);		}
		else { br = ParseTcgCommand(object, buf, buf_len, comid, receive); }
		//if (object) out.push_back(object);
	}
	else if (protocol == 0xEF)
	{	// ata passthrough

	}
	out = object;
	if (out) out->AddRef();
	return br;
}



bool CSecurityParser::TableParse(boost::property_tree::wptree& res, const TCG_UID table_id, tcg::ISecurityObject* stream)
{
	ListToken* ls_stream =	dynamic_cast<ListToken*>(stream);
	if (ls_stream == nullptr)
	{
		LOG_ERROR(L"[err] wrong format: stream should be list token");
		res.add(L"[err]", "wrong format: stream should be list token");
		return false;
	}
	ListToken* cols = ConvertTokenType<ListToken>(res, ls_stream->GetSubToken(0));
	CStatePhrase* state = ConvertTokenType<CStatePhrase>(res, ls_stream->GetSubToken(1));

	boost::property_tree::wptree table;
	// get table
	UINT64 tt_id = CUidMap::Uid2Uint(table_id);
	DWORD tt_hid = (DWORD)((tt_id >> 32) & 0xFFFFFFFF);
	const TABLE_INFO* tab_info = m_uid_map.FindTable(tt_hid);
	if (tab_info == nullptr)
	{
		table.add(L"[err]", L"unknown table id=");
		return false;
	}

	int ii = 0;
	for (auto it = cols->m_tokens.begin(); it != cols->m_tokens.end(); ++it, ++ii)
	{	// for each column
		NameToken* name_token = dynamic_cast<NameToken*>(*it);
		if (name_token == nullptr)
		{
			LOG_ERROR(L"[err] item %d in the list is not a name token", ii);
			return false;
		}
		UINT col_id = name_token->GetName<UINT>();
		const PARAM_INFO * col_info = tab_info->GetColumn(col_id);
		if (col_info == nullptr)
		{
			LOG_ERROR(L"[err] item %d is out of column number, col_id=%d", ii, col_id);
			return false;
		}
		col_info->ToProperty(table, name_token->m_value);
	}
	table.add(L"xmlattr.name", tab_info->m_name);
	res.add_child(L"Table", table);

	return true;
}


void tcg::CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring& path) 
{
	// TODO: implement
	JCASSERT(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== definations ====
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
		uinfo.m_uid = StringToUid<UINT64>(str_uid);
		uinfo.m_name = (*it).second.get<std::wstring>(L"Name");
		std::wstring& str_class = (*it).second.get<std::wstring>(L"class");
		uinfo.m_class = StringToClass(str_class);
		uinfo.m_method = nullptr;
		m_map.insert(std::make_pair(uinfo.m_uid, uinfo));
	}

	// Load type defines
	const boost::property_tree::wptree& types_pt = root_pt.get_child(L"Types");
	for (auto it = types_pt.begin(); it != types_pt.end(); ++it)
	{
		TYPE_INFO* type = TYPE_INFO::LoadType((*it).second, this);
		m_type_map.insert(std::make_pair(type->m_name, type));
	}

	// Load method defines
	const boost::property_tree::wptree& methods_pt = root_pt.get_child(L"Methods");
	for (auto it = methods_pt.begin(); it != methods_pt.end(); ++it)
	{
		const boost::property_tree::wptree& pt = (*it).second;
		UINT64 uid = StringToUid<UINT64>(pt.get<std::wstring>(L"UID"));
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

	// Load table info
	const boost::property_tree::wptree& table_pt = root_pt.get_child(L"Tables");
	for (auto it = table_pt.begin(); it != table_pt.end(); ++it)
	{
		TABLE_INFO* table = TABLE_INFO::LoadTable((*it).second, this);
		m_table_map.insert(std::make_pair(table->m_id, table));
	}

	return true;
}

//UINT64 CUidMap::StringToUid(const std::wstring& str)
//{
//	UINT64 val = 0;
//	const wchar_t* ch = str.c_str();
//	for (size_t ii = 0; ii < 8; ++ii)
//	{
//		val <<= 8;
//		int dd;
//		swscanf_s(ch, L"%X", &dd);
//		val |= dd;
//		ch += 3;
//	}
//	return val;
//}
//
//DWORD CUidMap::StringToHuid(const std::wstring& str)
//{
//	UINT64 val = 0;
//	const wchar_t* ch = str.c_str();
//	for (size_t ii = 0; ii < 8; ++ii)
//	{
//		val <<= 8;
//		int dd;
//		swscanf_s(ch, L"%X", &dd);
//		val |= dd;
//		ch += 3;
//	}
//	return val;
//}

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

const TYPE_INFO* CUidMap::FindType(const std::wstring& type_name)
{
	auto it = m_type_map.find(type_name);
	if (it != m_type_map.end()) return (it->second);
	return nullptr;
}

const TABLE_INFO* CUidMap::FindTable(DWORD tid)
{
	auto it = m_table_map.find(tid);
	if (it != m_table_map.end()) return it->second;
	return nullptr;
}

UINT64 CUidMap::Uid2Uint(const TCG_UID uid)
{
	UINT64 val;
	BYTE* vv = (BYTE*)(&val);
	for (int ii = 0; ii < 8; ++ii) vv[7 - ii] = uid[ii];
	return val;
}

CUidMap::~CUidMap(void)
{
	m_map.clear();
	for (auto it = m_table_map.begin(); it != m_table_map.end(); ++it)
	{
		delete (it->second);
	}
	m_table_map.clear();

	for (auto it = m_type_map.begin(); it != m_type_map.end(); ++it)
	{
		delete (it->second);
	}
	m_type_map.clear();
}

void CUidMap::Clear(void)
{
	auto end_it = m_map.end();
	for (auto it = m_map.begin(); it != end_it; ++it)
	{
		delete it->second.m_method;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


TYPE_INFO* TYPE_INFO::LoadType(const boost::property_tree::wptree& pt, CUidMap * map)
{
	TYPE_INFO* info = nullptr;
	std::wstring base_type = pt.get<std::wstring>(L"BaseType");
	if (false) {}
	else if (base_type == L"List")
	{
		TYPE_LIST_BASE* tt = new TYPE_LIST_BASE;
		std::wstring element_type = pt.get<std::wstring>(L"ElementType");
		tt->m_element_type = map->FindType(element_type);
		info = static_cast<TYPE_INFO*>(tt);
		info->m_base = LIST;
	}
	else if (base_type == L"Set")
	{
		info = new TYPE_INFO;
		info->m_base = SET;
		info->m_max_len = pt.get<UINT>(L"MaxNumber", 0);
	}
	else if (base_type == L"Alternative")
	{
		TYPE_ALTE_BASE* tt = new TYPE_ALTE_BASE;
		info = static_cast<TYPE_INFO*>(tt);
		const boost::property_tree::wptree& tpt = pt.get_child(L"ElementTypes");
		int ii = 0;
		for (auto it = tpt.begin(); it!=tpt.end() && ii<2; ++it, ++ii)
		{
			tt->m_type[ii] = map->FindType(it->second.get_value<std::wstring>());
		}
		info->m_base = ALTERNATIVE;
	}
	else if (base_type == L"UidRef")
	{
		info = new TYPE_INFO;
		info->m_base = UIDREF;
	}
	else if (base_type == L"Enumeration")
	{
		TYPE_ENUM_BASE* tt = new TYPE_ENUM_BASE;
		const boost::property_tree::wptree& ee = pt.get_child(L"Enumeration");
		for (auto it = ee.begin(); it != ee.end(); ++it)
		{
			std::wstring val = it->second.get_value<std::wstring>();
			tt->m_enum_list.push_back(val);
		}
		info = static_cast<TYPE_INFO*>(tt);
		info->m_base = ENUMERATION;
	}
	else if (base_type == L"Simple")
	{
		info = new TYPE_INFO;
		info->m_base = SIMPLE;
		info->m_max_len = pt.get<UINT>(L"MaxBytes");
	}
	else
	{
		THROW_ERROR(ERR_APP, L"unknown type base name: %s", base_type.c_str());
	}
	info->m_name = pt.get<std::wstring>(L"Name");
	info->m_id = CUidMap::StringToUid<DWORD>(pt.get<std::wstring>(L"ID"));
	info->m_uid_map = map;
	return info;
}


void TYPE_INFO::ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const
{
	switch (m_base)
	{
	case SET: {
		ListToken* ll = ConvertTokenType<ListToken>(pt, token);
		int ii = 0;
		for (auto it = ll->m_tokens.begin(); it != ll->m_tokens.end(); ++it, ++ii)
		{
			MidAtomToken* val = ConvertTokenType<MidAtomToken>(pt, *it);
			UINT vv;
			val->GetValue(vv);
			pt.add(L"set", vv);
		}
		break;
	}

	//case ALTERNATIVE: {
	//	break;
	//}

	case UIDREF: {
		MidAtomToken* vv = dynamic_cast<MidAtomToken*>(token);
		if (vv == nullptr || vv->m_type != CTcgTokenBase::BinaryAtom)
		{
			pt.put_value(L"[err] Value is not a Binary Atom Token");
			LOG_ERROR(L"[err] value is not an binary atom token");
			return;
		}
		std::wstring str_uid;
		vv->FormatToByteString(str_uid);
		pt.add(L"uid", str_uid);
		UINT64 uid_val;
		vv->GetValue(uid_val);
		const UID_INFO* uid_info = m_uid_map->GetUidInfo(uid_val);
		if (uid_info) pt.add(L"obj", uid_info->m_name);
		break; 	}

	case SIMPLE: {
		MidAtomToken* vv = ConvertTokenType<MidAtomToken>(pt, token);
		std::wstring str;
		vv->FormatToByteString(str);
		pt.put_value(str);
		break;
	}

	default:
		LOG_ERROR(L"[err] unhandled base type: %d", m_base);
		pt.put(L"xmlattr.type", m_name);
		pt.put(L"xmlattr.base", (UINT)m_base);
		token->ToProperty(pt);
	}
}

void TYPE_ALTE_BASE::ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const
{
	NameToken* nn = ConvertTokenType<NameToken>(pt, token);
	DWORD type_id = nn->GetName<DWORD>();
	// find type
	const TYPE_INFO* type = nullptr;
	for (size_t ii = 0; ii < 2; ++ii)
	{
		if (m_type[ii] && m_type[ii]->m_id == type_id)
		{
			type = m_type[ii];
			break;
		}
	}
	if (!type) THROW_ERROR(ERR_APP, L"wrong type id=%08X", type_id);
	type->ToProperty(pt, nn->m_value);
}

void TYPE_LIST_BASE::ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const
{
	ListToken* ll = dynamic_cast<ListToken*>(token);
	if (ll == nullptr)
	{
		LOG_ERROR(L"[err] value is not a list token");
		pt.put_value(L"[err] value is not a list token");
		return;
	}
	int ii = 0;
	for (auto it = ll->m_tokens.begin(); it != ll->m_tokens.end(); ++it, ++ii)
	{
		boost::property_tree::wptree item_pt;
		m_element_type->ToProperty(item_pt, (*it));
		wchar_t item_id[8];
		swprintf_s(item_id, L"%03d", ii);
		pt.add_child(item_id, item_pt);
	}

}

void TYPE_ENUM_BASE::ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const
{	// token 一定是整数
	MidAtomToken* vv = dynamic_cast<MidAtomToken*>(token);
	if (vv == nullptr)	
	{
		pt.put_value(L"[err] Value is not an Atom Token");	
		LOG_ERROR(L"[err] value is not an atom token");
		return;
	}
	UINT xx;
	vv->GetValue<UINT>(xx);
	if (xx >= m_enum_list.size())
	{
		LOG_ERROR(L"[err] enum value is out scope, val=%d, num=%d", xx, m_enum_list.size());
		pt.put(L"index", xx);
		pt.put_value(L"[err] enum value is out scope");
		return;
	}
	pt.put_value(m_enum_list[xx]);
}

TABLE_INFO::~TABLE_INFO(void)
{
	for (auto it = m_columns.begin(); it != m_columns.end(); ++it)
	{
		delete (*it);
	}
	m_columns.clear();
}

TABLE_INFO* TABLE_INFO::LoadTable(const boost::property_tree::wptree& pt, CUidMap* map)
{
	TABLE_INFO* table = new TABLE_INFO;
	table->m_id = CUidMap::StringToUid<DWORD>(pt.get<std::wstring>(L"ID"));
	table->m_name = pt.get<std::wstring>(L"Name");
	const boost::property_tree::wptree& col_pt = pt.get_child(L"Columns");
	for (auto it = col_pt.begin(); it != col_pt.end(); ++it)
	{
		PARAM_INFO* param = new PARAM_INFO;
		param->LoadParameter((*it).second, map);
		table->m_columns.push_back(param);
	}
	return table;
}

const PARAM_INFO* TABLE_INFO::GetColumn(UINT index) const
{
	if (index >= m_columns.size()) { return nullptr; }
	return m_columns[index];
}


void PARAM_INFO::LoadParameter(const boost::property_tree::wptree& pt, CUidMap* map)
{
	m_num = pt.get<DWORD>(L"Number");
	m_name = pt.get<std::wstring>(L"Name");
	int unique = pt.get<int>(L"Unique", -1);
	m_unique = unique > 0;

	m_type = pt.get<std::wstring>(L"Type");
	m_type_info = map->FindType(m_type);
	if (m_type_info == nullptr)
	{
		if (m_type == L"uidref") m_type_id = PARAM_INFO::UidRef;
		else m_type_id = PARAM_INFO::OtherType;
	}
}

void PARAM_INFO::ToProperty(boost::property_tree::wptree& pt, CTcgTokenBase* token) const
{
	boost::property_tree::wptree value_pt;
	if (m_type_info) {	m_type_info->ToProperty(value_pt, token);	}
	else
	{
		token->ToProperty(pt);
	}

	pt.add_child(m_name, value_pt);
}


void CUidMap::LoadParameter(const boost::property_tree::wptree& param_pt, std::vector<PARAM_INFO>& param_list)
{
	auto end_it = param_pt.end();
	for (auto it = param_pt.begin(); it != end_it; ++it)
	{
		PARAM_INFO param;
		const boost::property_tree::wptree& pt = (*it).second;
		//param.m_num = pt.get<DWORD>(L"Number");
		//param.m_name = pt.get<std::wstring>(L"Name");
		//param.m_type = pt.get<std::wstring>(L"Type");
		//if (param.m_type == L"uidref") param.m_type_id = PARAM_INFO::UidRef;
		//else param.m_type_id = PARAM_INFO::OtherType;
		param.LoadParameter(pt, this);
		param_list.push_back(param);
	}
}
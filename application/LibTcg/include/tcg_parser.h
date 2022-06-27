///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "table_info.h"
#include "l0discovery.h"
#include "tcg_token.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Security Parser

class CSecurityParser : public tcg::ISecurityParser
{
public:
	virtual ~CSecurityParser(void) { CTcgTokenBase::SetUidMap(NULL); }
public:
	virtual bool Initialize(const std::wstring& uid_config, const std::wstring& l0_config);
	virtual bool ParseSecurityCommand(tcg::ISecurityObject*& out, jcvos::IBinaryBuffer* payload, DWORD protocol, 
		DWORD comid, bool receive);
	virtual bool ParseSecurityCommand(tcg::ISecurityObject*& out, const BYTE* payload, size_t len, DWORD protocol, 
		DWORD comid, bool receive);

	virtual bool TableParse(boost::property_tree::wptree& table, const TCG_UID table_id, tcg::ISecurityObject* stream);

protected:
	bool ParseProtocolInfo(tcg::ISecurityObject*& obj, DWORD comid, const BYTE* buf, size_t data_len);
	bool ParseL0Discovery(tcg::ISecurityObject*& obj, const BYTE* buf, size_t data_len);
	bool ParseTcgCommand(tcg::ISecurityObject*& obj, const BYTE* buf, size_t data_len, DWORD comid, bool receive);

protected:
	CL0DiscoveryDescription m_feature_description;
	CUidMap m_uid_map;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== support functions

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


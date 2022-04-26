///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "../include/itcg.h"
#include "../include/l0discovery.h"
#include "../include/tcg_token.h"
#include "tcg-packet.h"

LOCAL_LOGGER_ENABLE(L"security_parse", LOGGER_LEVEL_NOTICE);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == protocol list
class CProtocolList : public tcg::ISecurityObject
{
public:
	virtual void ToString(std::wostream & out, UINT layer, int opt);
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Security Parser
class CSecurityParser : public tcg::ISecurityParser
{
public:
	virtual ~CSecurityParser(void) { CTcgTokenBase::SetUidMap(NULL); }
public:
	virtual bool Initialize(const std::wstring& uid_config, const std::wstring& l0_config);
	//virtual bool ParseSecurityCommand(std::vector<tcg::ISecurityObject*>& out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive);
	virtual bool ParseSecurityCommand(tcg::ISecurityObject*& out, jcvos::IBinaryBuffer* payload, DWORD protocol, DWORD comid, bool receive);

protected:
	bool ParseProtocolInfo(tcg::ISecurityObject*& obj, DWORD comid, BYTE* buf, size_t data_len);
	bool ParseL0Discovery(tcg::ISecurityObject*& obj, BYTE* buf, size_t data_len);
	bool ParseTcgCommand(tcg::ISecurityObject*& obj, BYTE* buf, size_t data_len, bool receive);

protected:
	CL0DiscoveryDescription m_feature_description;
	CUidMap m_uid_map;
};

void tcg::CreateSecurityParser(ISecurityParser*& parser)
{
	JCASSERT(parser == nullptr);
	CSecurityParser* pp = jcvos::CDynamicInstance<CSecurityParser>::Create();
	if (!pp) THROW_ERROR(ERR_APP, L"failed on creating CSecurityParser");
	parser = static_cast<ISecurityParser*>(pp);
}

bool CSecurityParser::ParseProtocolInfo(tcg::ISecurityObject * & obj, DWORD comid, BYTE * buf, size_t data_len)
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

bool CSecurityParser::ParseL0Discovery(tcg::ISecurityObject*& obj, BYTE* data, size_t data_len)
{
	CTcgFeatureSet* _fset = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
	if (_fset == nullptr) THROW_ERROR(ERR_APP, L"failed on createing feature set");

	m_feature_description.Parse(*_fset, data, data_len);
	obj = static_cast<tcg::ISecurityObject*>(_fset);
	return true;
}

bool CSecurityParser::ParseTcgCommand(tcg::ISecurityObject*& obj, BYTE* buf, size_t data_len, bool receive)
{
	// ComPacket Parse
	CTcgComPacket* packet = jcvos::CDynamicInstance<CTcgComPacket>::Create();
	if (packet == nullptr) THROW_ERROR(ERR_APP, L"failed on creating Tcg Com Packet");
	packet->ParseData(buf, data_len, receive);
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
	bool br;
	jcvos::auto_interface<tcg::ISecurityObject> object;
	if (protocol == 0)
	{	// security protocol information
		br = ParseProtocolInfo(object, comid, buf, buf_len);
		//if (object) out.push_back(object);
	}
	else if (protocol >= 1 && protocol <= 6)
	{	// tcg protocol
		if (comid == 1)
		{
			br = ParseL0Discovery(object, buf, buf_len);
			//if (object) out.push_back(object);
		}
		else
		{
			br = ParseTcgCommand(object, buf, buf_len, receive);
			//if (object) out.push_back(object);
		}
	}
	else if (protocol == 0xEF)
	{	// ata passthrough

	}
	payload->Unlock(buf);
	out = object;
	if (out) out->AddRef();
	return br;
}




void tcg::CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring& path) 
{
	// TODO: implement
	JCASSERT(0);
}


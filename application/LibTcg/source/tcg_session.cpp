///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "tcg_session.h"
#include "../include/itcg.h"
#include "../include/tcg_token.h"
#include <boost/endian.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/cast.hpp>
#include <iostream>

LOCAL_LOGGER_ENABLE(L"tcg.session", LOGGER_LEVEL_DEBUGINFO);

using tcg::ITcgSession;

CTcgSession::CTcgSession(void) : m_dev(NULL), m_is_open(false), m_feature_set(NULL)
{
	m_session_auth = 0;
	m_hsn = 0;
	m_tsn = 0;
	m_will_abort = false;
#ifdef _DEBUG
	m_invoking_id = 0;
#endif
	m_feature_set = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
	if (m_feature_set == nullptr) THROW_ERROR(ERR_APP, L"failed on creating feature set");
}

CTcgSession::~CTcgSession(void)
{
	if (m_is_open) EndSession();
	RELEASE(m_feature_set);
	RELEASE(m_dev);

}

bool CTcgSession::ConnectDevice(IStorageDevice* dev)
{
	JCASSERT(dev);
	m_dev = dev;
	m_dev->AddRef();

	// 获取L0Discovery
	jcvos::auto_array<BYTE> buf(SECTOR_SIZE);
	bool br = L0Discovery(buf);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on getting L0 Discovery");
		return false;
	}

	RELEASE(m_feature_set);
	// 解析L0Discovery
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);

	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	parser->ParseSecurityCommand(sec_obj, buf, SECTOR_SIZE, tcg::PROTOCOL_ID_TCG, tcg::COMID_L0DISCOVERY, true);
	sec_obj.detach<CTcgFeatureSet>(m_feature_set);

	const CTcgFeature* feature_opal = m_feature_set->GetFeature(CTcgFeature::FEATURE_OPAL_SSC);
	if (feature_opal == nullptr) THROW_ERROR(ERR_APP, L"the device does not support opal");
	m_base_comid = feature_opal->m_features.get<WORD>(L"Base ComId");

	// 获取property
	boost::property_tree::wptree prop;
	std::vector<std::wstring> req;
	prop.add<UINT>(L"MaxComPacketSize", 2048);// 0x10000);
	prop.add<UINT>(L"MaxPacketSize", 2028); // 0xFFEC);
	prop.add<UINT>(L"MaxIndTokenSize", 1992); // 0xFFC8);
	prop.add<UINT>(L"MaxPackets", 1);
	prop.add<UINT>(L"MaxSubpackets", 1);
	prop.add<UINT>(L"MaxMethods", 1);
	Properties(prop, req);
#ifdef _ZERO_
	std::wcout << L"properties out:" << std::endl;
	auto setting = boost::property_tree::xml_writer_settings<std::wstring>('\t', 1);
	boost::property_tree::write_xml(std::wcout, prop, setting);
#endif
	const boost::property_tree::wptree& pt = prop.get_child(L"call.parameters.Properties");
	LOG_DEBUG(L"Tper property: MaxComPacketSize=%d", m_tperMaxPacket);
	LOG_DEBUG(L"Tper property: MaxIndTokenSize=%d", m_tperMaxToken);

	m_tperMaxPacket = pt.get<UINT32>(L"MaxComPacketSize", 0);
	m_tperMaxToken = pt.get<UINT32>(L"MaxIndTokenSize", 0);

	return true;
}

bool CTcgSession::GetProtocol(BYTE* buf, size_t buf_len)
{
	JCASSERT(m_dev);
	if (buf_len < SECTOR_SIZE) THROW_ERROR(ERR_APP, L"buf needs 512 bytes, buf_len=%d", buf_len);
//	BYTE ir = m_dev->SecurityReceive(buf, 512, 0x01, 0x01);
	BYTE ir = m_dev->SecurityReceive(buf, 512, tcg::PROTOCOL_INFO, 0);
	if (ir != SUCCESS) { LOG_ERROR(L"[err] failed on calling security receive command"); }
	return (ir==SUCCESS);
}

bool CTcgSession::GetFeatures(tcg::ISecurityObject*& feature, bool force)
{
	bool br = true;
	if (!m_feature_set || force)
	{
		jcvos::auto_array<BYTE> buf(SECTOR_SIZE);
		br = L0Discovery(buf);
		if (!br)
		{
			LOG_ERROR(L"[err] failed on getting L0 Discovery");
			return false;
		}
		RELEASE(m_feature_set);
		// 解析L0Discovery
		jcvos::auto_interface<tcg::ISecurityParser> parser;
		tcg::GetSecurityParser(parser);

		jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
		parser->ParseSecurityCommand(sec_obj, buf, SECTOR_SIZE, tcg::PROTOCOL_ID_TCG, tcg::COMID_L0DISCOVERY, true);
		sec_obj.detach<CTcgFeatureSet>(m_feature_set);
	}
	feature = static_cast<tcg::ISecurityObject*>(m_feature_set);
	if (feature) feature->AddRef();
	return true;
}

bool CTcgSession::L0Discovery(BYTE* buf)
{
	JCASSERT(m_dev);
//	BYTE ir = m_dev->SecurityReceive(buf, 512, 0x01, 0x01);
	BYTE ir = m_dev->SecurityReceive(buf, 512, tcg::PROTOCOL_ID_TCG, tcg::COMID_L0DISCOVERY);
	if (ir != SUCCESS) { LOG_ERROR(L"[err] failed on calling security receive command"); }
	return (ir==SUCCESS);
}

BYTE CTcgSession::Properties(boost::property_tree::wptree& props, const std::vector<std::wstring>& req)
{
	LOG_STACK_TRACE();
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_MEM, L"failed on creating dta command");

	jcvos::auto_interface<ListToken> parameters(ListToken::CreateToken());
	if (parameters == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating list token");

	DtaResponse response;
	cmd->reset(OPAL_SMUID_UID, METHOD_HOSTPROP);
	if (!props.empty())
	{
		//组装参数
		for (auto it = props.begin(); it != props.end(); ++it)
		{
			UINT val = it->second.get_value<UINT>();
			jcvos::auto_interface<NameToken> param(NameToken::CreateToken(it->first, val));
			parameters->AddToken(param);
		}
		cmd->addOptionalParam(0, parameters);
		cmd->makeOptionalParam();
	}
	cmd->complete();

	BYTE err = InvokeMethod(*cmd, response);
	if (err)
	{
		THROW_ERROR(ERR_APP, L"failed on invoking property call, error code=0x%X", err);
	}
// parse
	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	response.getResult(sec_obj, true);
	props.clear();
	sec_obj->ToProperty(props);
	return 0;
}

BYTE CTcgSession::StartSession(const TCG_UID sp, const char* host_challenge, const TCG_UID sign_authority, bool write)
{
	LOG_STACK_TRACE();
	bool settimeout = IsEnterprise();
	BYTE lastRC = 0;

	if (m_is_open || m_hsn != 0 || m_tsn != 0) THROW_ERROR(ERR_APP, L"session has already opened");
	//m_hsn = 0;
	//m_tsn = 0;

	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");
again:
	DtaResponse response;
	cmd->reset(OPAL_SMUID_UID, METHOD_STARTSESSION);
	// add request parameters
	cmd->addToken(OPAL_TOKEN::STARTLIST); // [  (Open Bracket)
	cmd->addToken(105); // HostSessionID : sessionnumber
	cmd->addToken(sp); // SPID : SP
	if (write)		cmd->addToken(OPAL_TINY_ATOM::UINT_01); // write
	else cmd->addToken(OPAL_TINY_ATOM::UINT_00);

	if ((NULL != host_challenge) && (!IsEnterprise())) 
	{
		if (host_challenge[0] != 0)
		{
			if (m_hash_password)
			{
				vector<uint8_t> hash;
				//			hash.clear();
				//			DtaHashPwd(hash, host_challenge, d);
							// 源代码再此处对password做hash处理。
							// 修改为：要求传入的password已经是hash的结果。对password进行hash的责任交给上层应用处理。
				PackagePasswordToken(hash, host_challenge);
				cmd->addNameToken(OPAL_TINY_ATOM::UINT_00, host_challenge);
			}
			else
			{
				cmd->addNameToken(OPAL_TINY_ATOM::UINT_00, host_challenge);
			}
		}
		cmd->addNameToken(OPAL_TINY_ATOM::UINT_03, sign_authority);
	}

	// w/o the timeout the session may wedge and require a power-cycle,
	// e.g., when interrupted by ^C. 60 seconds is inconveniently long,
	// but revert may require that long to complete.
	if (settimeout) 	cmd->addNameToken("SessionTimeout", 60000);

	cmd->addToken(OPAL_TOKEN::ENDLIST); // ]  (Close Bracket)
	cmd->complete();

	lastRC = InvokeMethod(*cmd, response);
	if (lastRC != SUCCESS)
	{
		if (settimeout) 
		{
			LOG_DEBUG(L"Session start with timeout failed rc = %", lastRC);
			settimeout = 0;
			goto again;
		}
		LOG_ERROR(L"[err] Session start failed rc = %d", lastRC);
		return lastRC;
	}
	// call user method SL HSN TSN EL EOD SL 00 00 00 EL
	//   0   1     2     3  4   5   6  7   8
	m_hsn = boost::endian::endian_reverse(response.getUint32(4));
	m_tsn = boost::endian::endian_reverse(response.getUint32(5));
	if ((NULL != host_challenge) && (IsEnterprise())) 
	{
		vector<uint8_t> auth;
		auth.push_back(OPAL_SHORT_ATOM::BYTESTRING8);
		for (int i = 0; i < 8; i++) auth.push_back(sign_authority[i]);
		return(Authenticate(auth, host_challenge));
	}
	m_is_open = true;
	LOG_NOTICE(L"started session, host session id=0x%X, SP session id=0x%X", m_hsn, m_tsn);
	return 0;
}

BYTE CTcgSession::EndSession(void)
{
	LOG_STACK_TRACE();
	DtaResponse response;
	BYTE err = 0;
	if (!m_will_abort)
	{
		jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
		DtaCommand* cmd = (DtaCommand*)_cmd;
		if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");
		cmd->reset();
		cmd->addToken(OPAL_TOKEN::ENDOFSESSION);
		cmd->complete(0);

		err = InvokeMethod(*cmd, response);
		if (err != 0) LOG_ERROR(L"[err] EndSession failed, code=%d", err);
	}
	m_is_open = false;
	// 清除 host和tper session id
	m_hsn = 0;
	m_tsn = 0;
	return err;
}

BYTE CTcgSession::GetTable(tcg::ISecurityObject*& res, const TCG_UID table, WORD start_col, WORD end_col)
{
	JCASSERT(res == nullptr);
	DtaResponse rr;
	BYTE err = GetTable(rr, table, start_col, end_col);
	rr.GetResToken(res);
//	if (err) return err;

	//jcvos::auto_interface<tcg::ISecurityObject> token;
	//res->GetSubItem(token, L"token");

	// for my parser
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);
	boost::property_tree::wptree table_prop;
	parser->TableParse(table_prop, table, res);
//	parser->ParseSecurityCommand(m_result, m_payload, m_data_len, m_protocol, m_comid, true);

#ifdef _DEBUG
	auto setting = boost::property_tree::xml_writer_settings<std::wstring>('\t', 1);
	boost::property_tree::write_xml(std::wcout, table_prop, setting);
#endif

	return err;
}

BYTE CTcgSession::GetTable(DtaResponse& response, const TCG_UID table, WORD start_col, WORD end_col)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;

	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* get = (DtaCommand*)_cmd;
	if (NULL == get)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	get->reset(table, METHOD_GET);
	get->addToken(OPAL_TOKEN::STARTLIST);
	get->addToken(OPAL_TOKEN::STARTLIST);

	get->addNameToken(OPAL_TOKEN::STARTCOLUMN, start_col);

	get->addNameToken(OPAL_TOKEN::ENDCOLUMN, end_col);

	get->addToken(OPAL_TOKEN::ENDLIST);
	get->addToken(OPAL_TOKEN::ENDLIST);
	get->complete();

	lastRC = InvokeMethod(*get, response);
	if (lastRC != 0)		return lastRC;
	return 0;
}

//BYTE CTcgSession::SetTable(const TCG_UID table, OPAL_TOKEN name, vector<BYTE>& value)
BYTE CTcgSession::SetTable(tcg::ISecurityObject*& res, const TCG_UID table, int name, const char * value)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* set = (DtaCommand*)_cmd;
	if (NULL == set)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	set->reset(table, METHOD_SET);
//	set->changeInvokingUid(table);
	set->addToken(OPAL_TOKEN::STARTLIST);

	set->addToken(OPAL_TOKEN::STARTNAME);
	set->addToken(OPAL_TOKEN::VALUES); // "values"
	set->addToken(OPAL_TOKEN::STARTLIST);

	set->addNameToken(name, value);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->addToken(OPAL_TOKEN::ENDNAME);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->complete();

	DtaResponse response;
	lastRC = InvokeMethod(*set, response);
	response.GetResToken(res);
	if (lastRC != 0)	LOG_ERROR(L"[err] set table failed, code=%d", lastRC);
	return lastRC;
}

BYTE CTcgSession::SetTable(tcg::ISecurityObject*& res, const TCG_UID table, int name, int val)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* set = (DtaCommand*)_cmd;
	if (NULL == set)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	set->reset(table, METHOD_SET);
	//	set->changeInvokingUid(table);
	set->addToken(OPAL_TOKEN::STARTLIST);

	set->addToken(OPAL_TOKEN::STARTNAME);
	set->addToken(OPAL_TOKEN::VALUES); // "values"
	set->addToken(OPAL_TOKEN::STARTLIST);

	set->addNameToken(name, val);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->addToken(OPAL_TOKEN::ENDNAME);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->complete();

	DtaResponse response;
	lastRC = InvokeMethod(*set, response);
	response.GetResToken(res);
	if (lastRC != 0)	LOG_ERROR(L"[err] set table failed, code=%d", lastRC);
	return lastRC;
}

//BYTE CTcgSession::SetTable(tcg::ISecurityObject*& res, const TCG_UID table, size_t offset, BYTE * buf, size_t buf_len)
BYTE CTcgSession::SetTableBase(tcg::ISecurityObject*& res, const TCG_UID table, CTcgTokenBase* _where, 
		CTcgTokenBase* value)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* set = (DtaCommand*)_cmd;
	if (NULL == set)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	set->reset(table, METHOD_SET);
	if (_where) set->addOptionalParam(OPAL_TOKEN::WHERE, _where);
	if (value) set->addOptionalParam(OPAL_TOKEN::VALUES, value);

	set->makeOptionalParam();
	set->complete();

	DtaResponse response;
	lastRC = InvokeMethod(*set, response);
	response.GetResToken(res);
	if (lastRC != 0)	LOG_ERROR(L"[err] set table failed, code=%d", lastRC);
	return lastRC;
}

BYTE CTcgSession::Revert(tcg::ISecurityObject*& res, const TCG_UID sp)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	cmd->reset(sp, METHOD_REVERT);
	cmd->addToken(OPAL_TOKEN::STARTLIST);
	cmd->addToken(OPAL_TOKEN::ENDLIST);
	cmd->complete();

	DtaResponse resp;
	lastRC = InvokeMethod(*cmd, resp);
	if (lastRC != 0) LOG_ERROR(L"[err] RevertTPer failed, code=%d", lastRC);
	m_will_abort = true;
	return lastRC;
}

BYTE CTcgSession::GenKey(tcg::ISecurityObject*& res, const TCG_UID obj, UINT public_exp, UINT pin_length)
{
	LOG_STACK_TRACE();
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	cmd->reset(obj, METHOD_GENKEY);

	if (public_exp != 0)
	{
		jcvos::auto_interface<MidAtomToken> pexp(MidAtomToken::CreateToken(public_exp));
		cmd->addOptionalParam(OPAL_TOKEN::PUBLIC_EXPONENT, pexp);
	}
	if (pin_length != 0)
	{
		jcvos::auto_interface<MidAtomToken> plen(MidAtomToken::CreateToken(pin_length));
		cmd->addOptionalParam(OPAL_TOKEN::PIN_LENGTH, plen);
	}
	cmd->makeOptionalParam();
	cmd->complete();

	DtaResponse response;
	BYTE err = InvokeMethod(*cmd, response);
	response.GetResToken(res);
	if (err != 0)	LOG_ERROR(L"[err] set table failed, code=%d", err);
	return err;
}

BYTE CTcgSession::TperReset(void)
{
	JCASSERT(m_dev);
	jcvos::auto_array<BYTE> buf(SECTOR_SIZE, 0);
	BYTE err = m_dev->SecuritySend(buf, SECTOR_SIZE, tcg::PROTOCOL_ID_TPER, tcg::COMID_TPER_RESET);
	if (err) THROW_ERROR(ERR_APP, L"failed on reset tper, code=0x%X", err);
	return 0;
}

void CTcgSession::Reset(void)
{
	m_session_auth = 0;
	m_hsn = 0;
	m_tsn = 0;
	m_will_abort = false;
}

BYTE CTcgSession::Activate(tcg::ISecurityObject*& response, const TCG_UID obj)
{
	LOG_STACK_TRACE();

	uint8_t lastRC;
	//vector<uint8_t> table;
	//table.push_back(OPAL_SHORT_ATOM::BYTESTRING8);
	//for (int i = 0; i < 8; i++)
	//{
	//	table.push_back(OPALUID[OPAL_UID::OPAL_LOCKINGSP_UID][i]);
	//}

	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	jcvos::auto_ptr<DtaResponse> _res(new DtaResponse);
	DtaResponse* res = (DtaResponse*)_res;
	if (NULL == res)	THROW_ERROR(ERR_APP, L"failed on creating dta response");


	//if ((lastRC = session->start(OPAL_UID::OPAL_ADMINSP_UID, password, OPAL_UID::OPAL_SID_UID)) != 0)
	//{
	//	delete cmd;
	//	delete session;
	//	return lastRC;
	//}
	//if ((lastRC = getTable(table, 0x06, 0x06)) != 0)
	//{
	//	LOG(E) << "Unable to determine LockingSP Lifecycle state";
	//	delete cmd;
	//	delete session;
	//	return lastRC;
	//}
	//if ((0x06 != response.getUint8(3)) || // getlifecycle
	//	(0x08 != response.getUint8(4))) // Manufactured-Inactive
	//{
	//	LOG(E) << "Locking SP lifecycle is not Manufactured-Inactive";
	//	delete cmd;
	//	delete session;
	//	return DTAERROR_INVALID_LIFECYCLE;
	//}
	cmd->reset(obj, METHOD_ACTIVATE);
	cmd->addToken(OPAL_TOKEN::STARTLIST);
	cmd->addToken(OPAL_TOKEN::ENDLIST);
	cmd->complete();

	lastRC = InvokeMethod(*cmd, *res);
	res->GetResToken(response);
	if (lastRC != SUCCESS)
	{
		LOG_ERROR(L"[err] failed on activate, rc=0x%X", lastRC);
		return lastRC;
	}
	return 0;
}

BYTE CTcgSession::GetDefaultPassword(std::string& password)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;

	vector<uint8_t> hash;
	lastRC = StartSession(OPAL_ADMINSP_UID, NULL, OPAL_UID_HEXFF);
	if (lastRC != SUCCESS)
	{
		LOG_ERROR(L"Unable to start Unauthenticated session, code=%d", lastRC);
		return lastRC;
	}
	DtaResponse response;
	lastRC = GetTable(response, OPAL_C_PIN_MSID, PIN, PIN);
	if (lastRC != 0)
	{
		LOG_ERROR(L"[err] GetTable failed, code=%d", lastRC);
		return lastRC;
	}
	password = response.getString(4);
	EndSession();
	return lastRC;
}

BYTE CTcgSession::SetSIDPassword(const char* old_pw, const char* new_pwd)
{
	vector<uint8_t> hash, table;
//	LOG(D1) << "Entering DtaDevOpal::setSIDPassword()";
	LOG_STACK_TRACE();
	uint8_t lastRC;
//	session = new DtaSession(this);
//	if (NULL == session) {
//		LOG(E) << "Unable to create session object ";
//		return DTAERROR_OBJECT_CREATE_FAILED;
//	}
	bool hasholdpwd = false, hashnewpwd = false;
//	if (!hasholdpwd) session->dontHashPwd();

	lastRC = StartSession(OPAL_ADMINSP_UID, old_pw, OPAL_SID_UID);
	if (lastRC != 0)
	{
		LOG_ERROR(L"[err] failed on open session code=%d", lastRC);
		return lastRC;
	}

	//if ((lastRC = session->start(OPAL_UID::OPAL_ADMINSP_UID,
	//	oldpassword, OPAL_UID::OPAL_SID_UID)) != 0) {
	//	delete session;
	//	return lastRC;
	//}
	table.clear();
	table.push_back(OPAL_SHORT_ATOM::BYTESTRING8);
	for (int i = 0; i < 8; i++) 	table.push_back(OPAL_C_PIN_SID[i]);

	//hash.clear();
	//if (hashnewpwd);// DtaHashPwd(hash, new_pwd, this);
	//else 
	//{
	//	hash.push_back(0xd0);
	//	hash.push_back((uint8_t)strnlen(new_pwd, 255));
	//	for (uint16_t i = 0; i < strnlen(new_pwd, 255); i++) hash.push_back(new_pwd[i]);
	//}
	jcvos::auto_interface<tcg::ISecurityObject> res;
	lastRC = SetTable(res, OPAL_C_PIN_SID, OPAL_TOKEN::PIN, new_pwd);
	if (lastRC != 0) LOG_ERROR(L"[err] unable to set new SID password, code=%d", lastRC);
	EndSession();
	return lastRC;
}

BYTE CTcgSession::RevertTPer(const char* password, const TCG_UID authority, const TCG_UID sp)
{
	jcvos::auto_interface<tcg::ISecurityObject> res;
	BYTE err = StartSession(OPAL_ADMINSP_UID, password, authority, true);
	if (err)
	{
		LOG_ERROR(L"[err] failed on start AdminSP session, code=%d", err);
		return err;
	}

	err = Revert(res, sp);
	if (err)
	{
		LOG_ERROR(L"[err] failed on reverting, code=%d", err);
		return err;
	}

	Abort();
	return 0;
}

BYTE CTcgSession::SetLockingRange(UINT range_id, UINT64 start, UINT64 length)
{
	// 获取Locking Info基本参数
	BYTE err = 0;
	jcvos::auto_interface<tcg::ISecurityObject> res;
	err = GetTable(res, OPAL_LOCKING_INFO, 2, 10);
	if (err || res==nullptr) THROW_ERROR(ERR_APP, L"failed on getting locking info");
	boost::property_tree::wptree locking_info_pt;
	res->ToProperty(locking_info_pt);
	UINT max_range = locking_info_pt.get<UINT>(L"name_3", 0);
	UINT alignment_required = locking_info_pt.get<UINT>(L"name_7", 0);
	UINT logical_block_size = locking_info_pt.get<UINT>(L"name_8", 0);
	UINT64 alignement_granularity = locking_info_pt.get<UINT64>(L"name_9", 0);
	UINT lowest_aligned_lba = locking_info_pt.get<UINT>(L"name_A", 0);

	// enable user
	TCG_UID user_uid;
	GetAuthorityUid(user_uid, 0x09, false, range_id);	// Authority tab
	//memcpy_s(user_uid, sizeof(user_uid), OPAL_AUTHORITY_USER, sizeof(OPAL_AUTHORITY_USER));
	//user_uid[7] = range_id;

	res.release();
	// Enable user authority in Locking SP
	err = SetTable(res, user_uid, 5, 1);
	if (err) THROW_ERROR(ERR_APP, L"failed on enable user %d", range_id);

	// set the range
	TCG_UID range_uid;
	memcpy_s(range_uid, sizeof(range_uid), OPAL_LOCKING_RANGE, sizeof(OPAL_LOCKING_RANGE));
	range_uid[7] = range_id;

	res.release();
	jcvos::auto_interface<ListToken> col_list(ListToken::CreateToken());
	UINT64 aligned_start = round_up(start, alignement_granularity);
	jcvos::auto_interface<NameToken> name_start(NameToken::CreateToken(OPAL_TOKEN::RANGESTART, aligned_start));
	UINT64 aligned_end = round_up((start + length), alignement_granularity);
	UINT64 aligned_len = aligned_end - aligned_start;
	jcvos::auto_interface<NameToken> name_length(NameToken::CreateToken(OPAL_TOKEN::RANGELENGTH, aligned_len));
	jcvos::auto_interface<NameToken> name_readlocked(NameToken::CreateToken(OPAL_TOKEN::READLOCKENABLED, 1));
	jcvos::auto_interface<NameToken> name_writelocked(NameToken::CreateToken(OPAL_TOKEN::WRITELOCKENABLED, 1));
	col_list->AddToken(name_start);
	col_list->AddToken(name_length);
	col_list->AddToken(name_readlocked);
	col_list->AddToken(name_writelocked);
//	jcvos::auto_interface<NameToken> name_ (NameToken::CreateToken(OPAL_TOKEN::, ));
//	jcvos::auto_interface<NameToken> name_length(NameToken::CreateToken)
	err = SetTableBase(res, range_uid, NULL, col_list);
	if (err) THROW_ERROR(ERR_APP, L"failed on setting locking range: start=0x%llX length=0x%llX", start, length);

	// rekey locking range
	res.release();
	GetTable(res, range_uid, OPAL_TOKEN::ACTIVEKEY, OPAL_TOKEN::ACTIVEKEY);

//	res.release();
//	GenKey(res, 0, 0, 0);

	return 0;
}

BYTE CTcgSession::WriteShadowMBR(jcvos::IBinaryBuffer* buf)
{
	LOG_STACK_TRACE();
	JCASSERT(m_tperMaxPacket && m_tperMaxToken);
	// 获取block size
	UINT32 block_size = (MAX_BUFFER_LENGTH > m_tperMaxPacket) ? m_tperMaxPacket : MAX_BUFFER_LENGTH;
	if (block_size > (m_tperMaxToken - 4)) block_size = m_tperMaxToken - 4;

	BYTE * _data = buf->Lock();
	BYTE* data = _data;
	size_t data_len = buf->GetSize();
	size_t offset = 0;

	while (data_len)
	{
		size_t write_size = min(data_len, block_size);
		jcvos::auto_interface<MidAtomToken> add(MidAtomToken::CreateToken((UINT32)offset));
		jcvos::auto_interface<MidAtomToken> value(MidAtomToken::CreateToken(data, write_size));
		jcvos::auto_interface<tcg::ISecurityObject> res;
		BYTE err = SetTableBase(res, OPAL_SHADOW_MBR, add, value);
		if (err)
		{
			LOG_ERROR(L"[err] failed on set MBR, offset=%d, len=%d", offset, write_size);
			break;
		}

		data += write_size;
		data_len -= write_size;
		offset += write_size;
	}
	buf->Unlock(_data);

	// Set MBR Done, 需要在Enable之前设置Done
	jcvos::auto_interface<tcg::ISecurityObject> res;
	BYTE err = SetTable(res, OPAL_MBR_CONTROL, OPAL_TOKEN::MBRDONE, OPAL_TOKEN::OPAL_TRUE);
	if (err) THROW_ERROR(ERR_APP, L"failed on set mbr done, codd=0x%X", err);
	res.release();
	// Set MBR Enable
	err = SetTable(res, OPAL_MBR_CONTROL, OPAL_TOKEN::MBRENABLE, OPAL_TOKEN::OPAL_TRUE);
	if (err) THROW_ERROR(ERR_APP, L"failed on enable, codd=0x%X", err);

	return 0;
}

void CTcgSession::SetPassword(UINT user_id, bool admin, const char* new_pw)
{
	LOG_STACK_TRACE();
	// get user cpin
	BYTE err = 0;
	jcvos::auto_interface<tcg::ISecurityObject> res;
	TCG_UID user_uid;
	GetAuthorityUid(user_uid, 0x0B, admin, user_id);	// for C_PIN

	err = SetTable(res, user_uid, OPAL_TOKEN::PIN, new_pw);
	if (err) THROW_ERROR(ERR_APP, L"failed on set password for user %d", user_id);
}

void CTcgSession::AssignRangeToUser(UINT range_id, UINT user_id, bool keep_admin)
{
	TCG_UID user_uid;
	GetAuthorityUid(user_uid, 0x09, false, user_id);

	jcvos::auto_interface<NameToken> ace_exp(NameToken::CreateToken(HTYPE_AUTHORITY_OBJ, user_id));	// ace_exp for user1
	jcvos::auto_interface<ListToken> ace(ListToken::CreateToken());		// ac_element;
	ace->AddToken(ace_exp);
	jcvos::auto_interface<NameToken> col(NameToken::CreateToken(3, ace));		// column
	jcvos::auto_interface<ListToken> col_list(ListToken::CreateToken());	// column list
	col_list->AddToken(col);


}

int CTcgSession::InvokeMethod(DtaCommand& cmd, DtaResponse& response)
{
	LOG_STACK_TRACE();
//	LOG_DEBUG(L"invoke method: ");
	cmd.setHSN(m_hsn);
	cmd.setTSN(m_tsn);
	cmd.setcomID(GetComId());

	int exec_rc = ExecSecureCommand(cmd, response, tcg::PROTOCOL_ID_TCG);
	if (0 < exec_rc)
	{
		LOG_ERROR(L"[err] Command failed on exec = %d", exec_rc);
		return exec_rc;
	}
	// Check out the basics that so that we know we have a sane reply to work with
	 // zero lengths -- these are big endian but it doesn't matter for uint = 0
	if ((0 == response.m_header.cp.outstandingData) &&
		(0 == response.m_header.cp.minTransfer) &&
		(0 == response.m_header.cp.length)) 
	{
		LOG_ERROR(L"[err] All Response(s) returned, no further data, request parsing error.");
		LOG_DEBUG(L"outstanding=%d, min-transfer=%d, length=%d", response.m_header.cp.outstandingData,
			response.m_header.cp.minTransfer, response.m_header.cp.length);
		return DTAERROR_COMMAND_ERROR;
	}
	if ((0 == response.m_header.cp.length) ||
		(0 == response.m_header.pkt.length) ||
		(0 == response.m_header.subpkt.length)) 
	{
		LOG_ERROR(L"[err] One or more header fields have 0 length");
		LOG_DEBUG(L"cp.length=%d, pkt.length=%d, subpkt.length=%d", response.m_header.cp.length,
			response.m_header.pkt.length, response.m_header.subpkt.length);
		return DTAERROR_COMMAND_ERROR;
	}
	// if we get an endsession response return 0
	if (OPAL_TOKEN::ENDOFSESSION == response.tokenIs(0)) return 0;

	// IF we received a method status return it
	if (!((OPAL_TOKEN::ENDLIST == response.tokenIs(response.getTokenCount() - 1)) &&
		(OPAL_TOKEN::STARTLIST == response.tokenIs(response.getTokenCount() - 5)))) 
	{	// no method status so we hope we reported the error someplace else
		LOG_ERROR(L"[err] Method Status missing.")
		return DTAERROR_NO_METHOD_STATUS;
	}

	BYTE status_code = response.GetMethodStatus();
	if (OPALSTATUSCODE::SUCCESS != status_code) 
	{
		const wchar_t * msg = DtaResponse::MethodStatusCodeToString(status_code);
		LOG_ERROR(L"[err] Method status error (code=%d): %s", status_code, msg);
	}
//	m_base_comid++;
	return status_code;
}

int CTcgSession::ExecSecureCommand(const DtaCommand& cmd, DtaResponse& resp, BYTE protocol)
{
	uint8_t lastRC;
	BYTE* send_buf = (BYTE*)cmd.getCmdBuffer();
	OPALHeader* hdr = (OPALHeader*)send_buf;
	size_t send_size = cmd.outputBufferSize();
	WORD comid = GetComId();

#ifdef _DEBUG
	auto setting = boost::property_tree::xml_writer_settings<std::wstring>('\t', 1);

	wchar_t fn[MAX_PATH];
	swprintf_s(fn, L"send-%04X.bin", m_invoking_id++);
	FILE* file = NULL;
	_wfopen_s(&file, fn, L"w+");
	if (!file) THROW_ERROR(ERR_APP, L"cannot opne data file: %s", fn);
	fwrite(send_buf, send_size, 1, file);
	fclose(file);
	file = NULL;
	LOG_DEBUG(L"saved send security data in %s", fn);

	jcvos::auto_interface<tcg::ISecurityObject> param_obj;
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);
	parser->ParseSecurityCommand(param_obj, send_buf, send_size, protocol, comid, false);
	std::wcout << L"invoking param:" << std::endl;
	param_obj->ToString(std::wcout, -1, 0);
	std::wcout << std::endl;
	boost::property_tree::wptree param_pt;
	param_obj->ToProperty(param_pt);
	boost::property_tree::write_xml(std::wcout, param_pt, setting);
#endif

	JCASSERT(m_dev);
	lastRC = m_dev->SecuritySend(send_buf, send_size, protocol, comid);
	if (lastRC != 0)
	{
		LOG_ERROR(L"[err] SecuritySend command failed, code=0x%X", lastRC);
		return lastRC;
	}
//<TODO> 优化：可以直接在response中申请缓存。
	jcvos::auto_array<BYTE> response_buf(MIN_BUFFER_LENGTH + IO_BUFFER_ALIGNMENT, 0);

	hdr = (OPALHeader*)(response_buf.get_ptr());
	comid = GetComId();
	do 
	{
//<TODO> 这里没有考虑到多笔cmd时，指针位置问题。
		Sleep(25);
//		osmsSleep(25);
//		memset(cmd.getRespBuffer(), 0, MIN_BUFFER_LENGTH);
		lastRC = m_dev->SecurityReceive(response_buf, MIN_BUFFER_LENGTH, protocol, comid);
//		lastRC = sendCmd(IF_RECV, protocol, comID(), cmd->getRespBuffer(), MIN_BUFFER_LENGTH);
	}     while ((0 != hdr->cp.outstandingData) && (0 == hdr->cp.minTransfer));

#ifdef _DEBUG
	file = NULL;
	swprintf_s(fn, L"receive-%04X.bin", m_invoking_id++);
	_wfopen_s(&file, fn, L"w+");
	if (!file) THROW_ERROR(ERR_APP, L"cannot opne data file: %s", fn);
	fwrite(response_buf, MIN_BUFFER_LENGTH, 1, file);
	fclose(file);
	LOG_DEBUG(L"saved recieved security data in %s", fn);
#endif

	if (0 != lastRC) 
	{
		LOG_ERROR(L"[err] SecureityRecieve command failed, code=0x%X", lastRC);
		return -lastRC;
	}
	resp.init(response_buf, MIN_BUFFER_LENGTH, protocol, comid);
#ifdef _DEBUG
	std::wcout << L"result out:" << std::endl;

	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	resp.getResult(sec_obj);
	sec_obj->ToString(std::wcout, -1, 0);
	std::wcout << std::endl;
//	boost::property_tree::wptree res_pt;
//	sec_obj->ToProperty(res_pt);
//	boost::property_tree::write_xml(std::wcout, res_pt, setting);
#endif
	WORD status_code = resp.getStatusCode();
	LOG_DEBUG(L"device returns status 0x%X", status_code);
	return 0;
}

void CTcgSession::PackagePasswordToken(vector<BYTE>& hash, const char* password)
{
	for (uint16_t i = 0; i < strnlen(password, 32); i++)	hash.push_back(password[i]);
	// add the token overhead
	hash.insert(hash.begin(), (uint8_t)hash.size());
	hash.insert(hash.begin(), 0xd0);
}

BYTE CTcgSession::Authenticate(vector<uint8_t> Authority, const char* Challenge)
{
//	LOG(D1) << "Entering DtaSession::authenticate ";
	LOG_STACK_TRACE();

	vector<uint8_t> hash;
	//DtaCommand* cmd = new DtaCommand();
	//if (NULL == cmd) {
	//	LOG(E) << "Unable to create session object ";
	//	return DTAERROR_OBJECT_CREATE_FAILED;
	//}
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)
	{
		LOG_ERROR(L"Unable to create session object");
		return DTAERROR_OBJECT_CREATE_FAILED;
	}

	DtaResponse response;
	cmd->reset(OPAL_THISSP_UID, IsEnterprise() ? METHOD_EAUTHENTICATE : METHOD_AUTHENTICATE);
	cmd->addToken(OPAL_TOKEN::STARTLIST); // [  (Open Bracket)
	cmd->addTokenByteList(Authority);
	if (Challenge && *Challenge)
	{
		cmd->addToken(OPAL_TOKEN::STARTNAME);
		if (IsEnterprise()) 	cmd->addToken("Challenge");
		else					cmd->addToken(OPAL_TINY_ATOM::UINT_00);
		if (m_hash_password) 
		{
			hash.clear();
//			DtaHashPwd(hash, Challenge, d);
			PackagePasswordToken(hash, Challenge);
			cmd->addTokenByteList(hash);
		}
		else			cmd->addToken(Challenge);
		cmd->addToken(OPAL_TOKEN::ENDNAME);
	}
	cmd->addToken(OPAL_TOKEN::ENDLIST); // ]  (Close Bracket)
	cmd->complete();
	BYTE lastRC = InvokeMethod(*cmd, response);
//	if ((lastRC = sendCommand(cmd, response)) != 0) {
	if (lastRC != 0)
	{
//		LOG(E) << "Session Authenticate failed";
		LOG_ERROR(L"Session Authenticate failed");
//		delete cmd;
		return lastRC;
	}
	if (0 == response.getUint8(1)) 
	{
//		LOG(E) << "Session Authenticate failed (response = false)";
		LOG_ERROR(L"Session Authenticate failed (response=ffalse)");
//		delete cmd;
		return DTAERROR_AUTH_FAILED;
	}

//	LOG(D1) << "Exiting DtaSession::authenticate ";
//	delete cmd;
	return 0;
}

void CTcgSession::Abort(void)
{
	m_is_open = false;
	m_tsn = 0;
	m_hsn = 0;
	m_will_abort = true;
}

void CTcgSession::GetAuthorityUid(TCG_UID& uid, UINT tab, bool admin_user, UINT id)
{
	memcpy_s(uid, sizeof(TCG_UID), OPAL_AUTHORITY_TABLE, sizeof(OPAL_AUTHORITY_TABLE));
	uid[3] = boost::numeric_cast<BYTE>(tab);
	if (admin_user) uid[5] = 1;	// for admin
	else uid[5] = 3;
	uid[7] = boost::numeric_cast<BYTE>(id);
}



//void DtaHashPwd(vector<uint8_t>& hash, char* password, DtaDev* d)
//{
//	//LOG(D1) << " Entered DtaHashPwd";
//	LOG_STACK_TRACE();
//	char* serNum;
//
//	if (d->no_hash_passwords) 
//	{
//		hash.clear();
//		for (uint16_t i = 0; i < strnlen(password, 32); i++)	hash.push_back(password[i]);
//		// add the token overhead
//		hash.insert(hash.begin(), (uint8_t)hash.size());
//		hash.insert(hash.begin(), 0xd0);
//		//LOG(D1) << " Exit DtaHashPwd";
//		return;
//	}
//	serNum = d->getSerialNum();
//	vector<uint8_t> salt(serNum, serNum + 20);
//	//	vector<uint8_t> salt(DEFAULTSALT);
//	DtaHashPassword(hash, password, salt);
////	LOG(D1) << " Exit DtaHashPwd"; // log for hash timing
//}

void tcg::CreateTcgSession(ITcgSession*& session, IStorageDevice* dev)
{
	JCASSERT(session == nullptr);
	CTcgSession* ss = jcvos::CDynamicInstance<CTcgSession>::Create();
	JCASSERT(ss);
	ss->ConnectDevice(dev);
	session = static_cast<ITcgSession*>(ss);
}

void CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring & path)
{
	JCASSERT(dev == nullptr);
	CTestDevice* dd = jcvos::CDynamicInstance<CTestDevice>::Create();
	JCASSERT(dd);
	dd->m_path = path;
	dev = static_cast<IStorageDevice*>(dd);
}

BYTE CTestDevice::SecurityReceive(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid)
{
	if ((protocolid == 1) && (comid == 1))
	{
		std::wstring fn = m_path + L"\\l0discovery.bin";
		FILE* file = NULL;
		_wfopen_s(&file, fn.c_str(), L"r");
		if (!file) THROW_ERROR(ERR_APP, L"cannot open l0discovery data: %s", fn.c_str());
		fread(buf, buf_len, 1, file);
		fclose(file);
	}
	else
	{
		wchar_t fn[MAX_PATH];
		swprintf_s(fn, L"%s\\receive-%04X.bin", m_path.c_str(), comid);
		FILE* file = NULL;
		_wfopen_s(&file, fn, L"r");
		if (!file) THROW_ERROR(ERR_APP, L"cannot open l0discovery data: %s", fn);
		fread(buf, buf_len, 1, file);
		fclose(file);
	}
//		memset(buf, 0, buf_len);
	return 0;
}

BYTE CTestDevice::SecuritySend(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid)
{
	wchar_t fn[MAX_PATH];
	swprintf_s(fn, L"%s\\send-%04X.bin", m_path.c_str(), comid);
	FILE* file = NULL;
	_wfopen_s(&file, fn, L"w+");
	if (!file) THROW_ERROR(ERR_APP, L"cannot opne data file: %s", fn);
	fwrite(buf, buf_len, 1, file);
	fclose(file);
	return 0;
}

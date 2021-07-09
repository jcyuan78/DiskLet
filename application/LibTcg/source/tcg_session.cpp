#include "pch.h"
#include "tcg_session.h"
#include <boost/endian.hpp>

LOCAL_LOGGER_ENABLE(L"tcg.session", LOGGER_LEVEL_NOTICE);


CTcgSession::CTcgSession(void) : m_dev(NULL), m_is_open(false)
{
	m_session_auth = 0;
	m_hsn = 0;
	m_tsn = 0;
	m_will_abort = false;
#ifdef _DEBUG
	m_invoking_id = 0;
#endif
}

CTcgSession::~CTcgSession(void)
{
	if (m_is_open) EndSession();
	RELEASE(m_dev);
}

bool CTcgSession::ConnectDevice(IStorageDevice* dev)
{
	JCASSERT(dev);
	m_dev = dev;
	m_dev->AddRef();

	jcvos::auto_array<BYTE> buf(SECTOR_SIZE);
	BYTE ir = L0Discovery(buf);
	if (ir != SUCCESS)
	{
		LOG_ERROR(L"[err] failed on getting L0 Discovery");
		return false;
	}

	CL0DiscoveryDescription desc;
	bool br = desc.LoadConfig(L"");
	if (!br)
	{
		LOG_ERROR(L"[err] failed on loading l0 discovery config");
		return false;
	}
	br = desc.Parse(m_feature_set, buf, SECTOR_SIZE);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on parsing l0 discovery");
		return false;
	}

	const CTcgFeature* feature_opal = m_feature_set.GetFeature(CTcgFeature::FEATURE_OPAL_SSC);
	if (feature_opal == NULL) THROW_ERROR(ERR_APP, L"the device does not support opal");
	m_base_comid = feature_opal->m_features.get<WORD>(L"Base ComId");
	return true;
}

BYTE CTcgSession::L0Discovery(BYTE* buf)
{
	JCASSERT(m_dev);
	BYTE ir = m_dev->SecurityReceive(buf, 512, 0x01, 0x01);
	if (ir != SUCCESS) { LOG_ERROR(L"[err] failed on calling security receive command"); }
	return ir;
}

BYTE CTcgSession::StartSession(const TCG_UID sp, const char* host_challenge, const TCG_UID sign_authority, bool write)
{
	LOG_STACK_TRACE();
	bool settimeout = IsEnterprise();
	BYTE lastRC = 0;

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
	return 0;
}

BYTE CTcgSession::EndSession(void)
{
	LOG_STACK_TRACE();
	DtaResponse response;
	if (!m_will_abort)
	{
		jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
		DtaCommand* cmd = (DtaCommand*)_cmd;
		if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");
		cmd->reset();
		cmd->addToken(OPAL_TOKEN::ENDOFSESSION);
		cmd->complete(0);

		BYTE ir = InvokeMethod(*cmd, response);
		if (ir != 0) LOG_ERROR(L"[err] EndSession failed, code=%d", ir);
		return ir;
	}
	m_is_open = false;
	return 0;
}

BYTE CTcgSession::GetTable(DtaResponse& response, const TCG_UID table, WORD start_col, WORD end_col)
{
//	LOG(D1) << "Entering DtaDevOpal::getTable";
	LOG_STACK_TRACE();
	uint8_t lastRC;
	//DtaCommand* get = new DtaCommand();
	//if (NULL == get) {
	//	LOG(E) << "Unable to create command object ";
	//	return DTAERROR_OBJECT_CREATE_FAILED;
	//}
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* get = (DtaCommand*)_cmd;
	if (NULL == get)	THROW_ERROR(ERR_APP, L"failed on creating dta command");
	//DtaResponse response;

//	get->reset(OPAL_AUTHORITY_TABLE, METHOD_GET);
	get->reset(table, METHOD_GET);
//	get->changeInvokingUid(table);
	get->addToken(OPAL_TOKEN::STARTLIST);
	get->addToken(OPAL_TOKEN::STARTLIST);

	//get->addToken(OPAL_TOKEN::STARTNAME);
	//get->addToken(OPAL_TOKEN::STARTCOLUMN);
	//get->addToken(start_col);
	//get->addToken(OPAL_TOKEN::ENDNAME);
	get->addNameToken(OPAL_TOKEN::STARTCOLUMN, start_col);

	//get->addToken(OPAL_TOKEN::STARTNAME);
	//get->addToken(OPAL_TOKEN::ENDCOLUMN);
	//get->addToken(end_col);
	//get->addToken(OPAL_TOKEN::ENDNAME);
	get->addNameToken(OPAL_TOKEN::ENDCOLUMN, end_col);

	get->addToken(OPAL_TOKEN::ENDLIST);
	get->addToken(OPAL_TOKEN::ENDLIST);
	get->complete();

	lastRC = InvokeMethod(*get, response);
	if (lastRC != 0)		return lastRC;
	return 0;
}

//BYTE CTcgSession::SetTable(const TCG_UID table, OPAL_TOKEN name, vector<BYTE>& value)
BYTE CTcgSession::SetTable(const TCG_UID table, OPAL_TOKEN name, const char * value)
{
//	LOG(D1) << "Entering DtaDevOpal::setTable";
	LOG_STACK_TRACE();
	uint8_t lastRC;
	//DtaCommand* set = new DtaCommand();
	//if (NULL == set) {
	//	LOG(E) << "Unable to create command object ";
	//	return DTAERROR_OBJECT_CREATE_FAILED;
	//}
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* set = (DtaCommand*)_cmd;
	if (NULL == set)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	set->reset(table, METHOD_SET);
//	set->changeInvokingUid(table);
	set->addToken(OPAL_TOKEN::STARTLIST);

	set->addToken(OPAL_TOKEN::STARTNAME);
	set->addToken(OPAL_TOKEN::VALUES); // "values"
	set->addToken(OPAL_TOKEN::STARTLIST);

	//set->addToken(OPAL_TOKEN::STARTNAME);
	//set->addToken(name);
	//set->addToken(value);
	//set->addToken(OPAL_TOKEN::ENDNAME);
	set->addNameToken(name, value);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->addToken(OPAL_TOKEN::ENDNAME);
	set->addToken(OPAL_TOKEN::ENDLIST);
	set->complete();

	DtaResponse response;
	lastRC = InvokeMethod(*set, response);
	if (lastRC != 0)	LOG_ERROR(L"[err] set table failed, code=%d", lastRC);
	return lastRC;

	//if ((lastRC = session->sendCommand(set, response)) != 0) {
	//	LOG(E) << "Set Failed ";
	//	delete set;
	//	return lastRC;
	//}
	//delete set;
	//LOG(D1) << "Leaving DtaDevOpal::setTable";
	//return 0;
}

BYTE CTcgSession::Revert(const TCG_UID sp)
{
	LOG_STACK_TRACE();
	uint8_t lastRC;
	jcvos::auto_ptr<DtaCommand> _cmd(new DtaCommand);
	DtaCommand* cmd = (DtaCommand*)_cmd;
	if (NULL == cmd)	THROW_ERROR(ERR_APP, L"failed on creating dta command");

	//session = new DtaSession(this);
	//if (NULL == session) {
	//	LOG(E) << "Unable to create session object ";
	//	delete cmd;
	//	return DTAERROR_OBJECT_CREATE_FAILED;
	//}
	//OPAL_UID uid = OPAL_UID::OPAL_SID_UID;
	//if (PSID) {
	//	session->dontHashPwd(); // PSID pwd should be passed as entered
	//	uid = OPAL_UID::OPAL_PSID_UID;
	//}
	//if ((lastRC = session->start(OPAL_UID::OPAL_ADMINSP_UID, password, uid)) != 0) {
	//	delete cmd;
	//	delete session;
	//	return lastRC;
	//}
	cmd->reset(sp, METHOD_REVERT);
	cmd->addToken(OPAL_TOKEN::STARTLIST);
	cmd->addToken(OPAL_TOKEN::ENDLIST);
	cmd->complete();

	DtaResponse resp;
	lastRC = InvokeMethod(*cmd, resp);
	if (lastRC != 0) LOG_ERROR(L"[err] RevertTPer failed, code=%d", lastRC);
	m_will_abort = true;
	return lastRC;
	//if ((lastRC = session->sendCommand(cmd, response)) != 0) {
	//	delete cmd;
	//	delete session;
	//	return lastRC;
	//}
	//LOG(I) << "revertTper completed successfully";
	//session->expectAbort();
	//delete cmd;
	//delete session;
	//LOG(D1) << "Exiting DtaDevOpal::revertTPer()";
	//return 0;
}

void CTcgSession::Reset(void)
{
	m_session_auth = 0;
	m_hsn = 0;
	m_tsn = 0;
	m_will_abort = false;
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
	lastRC = SetTable(OPAL_C_PIN_SID, OPAL_TOKEN::PIN, new_pwd);
	if (lastRC != 0) LOG_ERROR(L"[err] unable to set new SID password, code=%d", lastRC);
	EndSession();
	return lastRC;
	//if ((lastRC = setTable(table, OPAL_TOKEN::PIN, hash)) != 0) 
	//{
	//	LOG(E) << "Unable to set new SID password ";
	//	delete session;
	//	return lastRC;
	//}
	//LOG(I) << "SID password changed";
	//delete session;
	//LOG(D1) << "Exiting DtaDevOpal::setSIDPassword()";
	//return 0;
}

BYTE CTcgSession::InvokeMethod(DtaCommand& cmd, DtaResponse& response)
{
	LOG_STACK_TRACE();
	cmd.setHSN(m_hsn);
	cmd.setTSN(m_tsn);
	cmd.setcomID(GetComId());

	uint8_t exec_rc = ExecSecureCommand(cmd, response, PROTOCOL_ID_TCG);
	if (0 != exec_rc)
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
		LOG_ERROR(L"[err] All Response(s) returned, no further data, request parsing error.")
		return DTAERROR_COMMAND_ERROR;
	}
	if ((0 == response.m_header.cp.length) ||
		(0 == response.m_header.pkt.length) ||
		(0 == response.m_header.subpkt.length)) 
	{
		LOG_ERROR(L"[err] One or more header fields have 0 length")
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
		LOG_ERROR(L"[err] Method status error (code=%d): %s", DtaResponse::MethodStatusCodeToString(status_code));
	}
//	m_base_comid++;
	return status_code;
}

BYTE CTcgSession::ExecSecureCommand(const DtaCommand& cmd, DtaResponse& resp, BYTE protocol)
{
	uint8_t lastRC;
	BYTE* send_buf = (BYTE*)cmd.getCmdBuffer();
	OPALHeader* hdr = (OPALHeader*)send_buf;
	size_t send_size = cmd.outputBufferSize();

#ifdef _DEBUG
	wchar_t fn[MAX_PATH];
	swprintf_s(fn, L"send-%04X.bin", m_invoking_id++);
	FILE* file = NULL;
	_wfopen_s(&file, fn, L"w+");
	if (!file) THROW_ERROR(ERR_APP, L"cannot opne data file: %s", fn);
	fwrite(send_buf, send_size, 1, file);
	fclose(file);
#endif

	JCASSERT(m_dev);
	lastRC = m_dev->SecuritySend(send_buf, send_size, protocol, GetComId());
	if (lastRC != 0)
	{
		LOG_ERROR(L"[err] SecuritySend command failed, code=0x%X", lastRC);
		return lastRC;
	}

	jcvos::auto_array<BYTE> response_buf(MIN_BUFFER_LENGTH + IO_BUFFER_ALIGNMENT, 0);

	hdr = (OPALHeader*)(response_buf.get_ptr());
	do 
	{
		Sleep(25);
//		osmsSleep(25);
//		memset(cmd.getRespBuffer(), 0, MIN_BUFFER_LENGTH);
		lastRC = m_dev->SecurityReceive(response_buf, MIN_BUFFER_LENGTH, protocol, GetComId());
//		lastRC = sendCmd(IF_RECV, protocol, comID(), cmd->getRespBuffer(), MIN_BUFFER_LENGTH);
	}     while ((0 != hdr->cp.outstandingData) && (0 == hdr->cp.minTransfer));

#ifdef _DEBUG
	file = NULL;
	swprintf_s(fn, L"receive-%04X.bin", m_invoking_id++);
	_wfopen_s(&file, fn, L"w+");
	if (!file) THROW_ERROR(ERR_APP, L"cannot opne data file: %s", fn);
	fwrite(response_buf, MIN_BUFFER_LENGTH, 1, file);
	fclose(file);
#endif

	if (0 != lastRC) 
	{
		LOG_ERROR(L"[err] SecureityRecieve command failed, code=0x%X", lastRC);
		return lastRC;
	}
	resp.init(response_buf);
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

void CreateTcgSession(ITcgSession*& session, IStorageDevice* dev)
{
	JCASSERT(session == NULL);
	CTcgSession* ss = jcvos::CDynamicInstance<CTcgSession>::Create();
	JCASSERT(ss);
	ss->ConnectDevice(dev);
	session = static_cast<ITcgSession*>(ss);
}

void CreateTcgTestDevice(IStorageDevice*& dev, const std::wstring & path)
{
	JCASSERT(dev == NULL);
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

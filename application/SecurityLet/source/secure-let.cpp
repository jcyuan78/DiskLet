///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "secure-let.h"
#include "global.h"
#include <LibTcg.h>
#include <boost/property_tree/xml_parser.hpp>

LOCAL_LOGGER_ENABLE(L"security_let", LOGGER_LEVEL_NOTICE);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == global ==
StaticInit global;

StaticInit::StaticInit(void)
	: m_parser(NULL)
{
	std::wstring config = L"security.cfg";
	//Init for logs
	LOG_STACK_TRACE();
	// for test
	wchar_t dllpath[MAX_PATH] = { 0 };
//#ifdef _DEBUG
//	GetCurrentDirectory(MAX_PATH, dllpath);
//	wprintf_s(L"current path=%s\n", dllpath);
//	memset(dllpath, 0, sizeof(wchar_t) * MAX_PATH);
//#endif
	// 获取DLL路径
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE this_dll = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	::GetModuleFileName(this_dll, dllpath, MAX_PATH);
	// Load log config
	std::wstring fullpath, fn;
	jcvos::ParseFileName(dllpath, fullpath, fn);
//#ifdef _DEBUG
//	wprintf_s(L"module path=%s\n", fullpath.c_str());
//#endif
	LOGGER_CONFIG(config.c_str(), fullpath.c_str());
	LOG_DEBUG(_T("log config..."));

	//g_init = true;

	tcg::GetSecurityParser(m_parser);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Security Let ==

void SecureLet::InitSecurityParser::InternalProcessRecord()
{
	std::wstring uuid_fn;
	std::wstring l0_fn;

	ToStdString(uuid_fn, uuid);
	ToStdString(l0_fn, l0discovery);

	global.m_parser->Initialize(uuid_fn, l0_fn);
}

void SecureLet::GetTPerProperties::InternalProcessRecord()
{
	jcvos::auto_interface<IStorageDevice> dd;
	if (dev) dev->GetStorageDevice(dd);
//	else		global.GetDevice(dd);
	if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

	jcvos::auto_interface<tcg::ITcgSession> session;
	CreateTcgSession(session, dd);
	if (!session) throw gcnew System::ApplicationException(L"failed on creating tcg session");

	//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
	//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");
	boost::property_tree::wptree prop;
	std::vector<std::wstring> req;
	prop.add<UINT>(L"MaxComPacketSize", 0x10000);
	//prop.add<UINT>(L"MaxPacketSize", 0xFFEC);
	//prop.add<UINT>(L"MaxIndTokenSize", 0xFFC8);

	session->Properties(prop, req);


//	WriteObject(data);
}

void SecureLet::GetL0Discovery::InternalProcessRecord()
{
	jcvos::auto_interface<IStorageDevice> dd;
	if (dev) dev->GetStorageDevice(dd);
	//else		global.GetDevice(dd);
	if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

	jcvos::auto_interface<tcg::ITcgSession> tcg;
	CreateTcgSession(tcg, dd);
	if (!tcg) throw gcnew System::ApplicationException(L"failed on creating tcg session");

	//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
	//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");

	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(1));
	BYTE* _buf = buf->Lock();
	bool br = tcg->L0Discovery(_buf);
	buf->Unlock(_buf);
	if (!br) wprintf_s(L"[err] failed on calling L0Discovery");

	if (OutBinary.ToBool())
	{
		JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
		WriteObject(data);
		return;
	}
	// decode l0discovery

//			if (!data) throw gcnew System::ApplicationException(L"data cannot be null");
//			BYTE* _data = buf->Lock();
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);
	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	parser->ParseSecurityCommand(sec_obj, buf, tcg::PROTOCOL_ID_TCG, tcg::COMID_L0DISCOVERY, true);

	CTcgFeatureSet* _fset = sec_obj.d_cast<CTcgFeatureSet*>();
	if (_fset == NULL) THROW_ERROR(ERR_APP, L"failed on parsing L0Discovery");

	//CTcgFeatureSet* _fset = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
	//if (_fset == nullptr) THROW_ERROR(ERR_APP, L"failed on creating feature set");
	//global.m_feature_description.Parse(*_fset, _data, data->Length);
	//data->Unlock();
	// convert to .net object
	TcgFeatureSet^ feature_set = gcnew TcgFeatureSet(sec_obj);
//	feature_set->length = _fset->m_header.length_of_parameter;
//	feature_set->ver_major = _fset->m_header.major_version;
//	feature_set->ver_minor = _fset->m_header.minor_version;
//	//feature_set->vendor
//
//	for (auto it = _fset->m_features.begin(); it != _fset->m_features.end(); ++it)
//	{
//		TcgFeature^ feature = gcnew TcgFeature;
//		feature->name = gcnew String(it->m_name.c_str());
//		feature->code = it->m_code;
//		feature->version = it->m_version;
//		feature->length = it->m_length;
//		for (auto ff = (it->m_features.begin()); ff != (it->m_features.end()); ++ff)
//		{
//			feature->fields.Add(gcnew String(ff->first.c_str()), ff->second.get_value<int>());
//		}
//#ifdef USING_DIRECTORY_FOR_FEATURE
//		feature_set->features.Add(gcnew String(feature->name), feature);
//#else
//		feature_set->features.Add(feature);
//#endif
//	}
	WriteObject(feature_set);
}



void SecureLet::GetDefaultPassword::InternalProcessRecord()
{
	jcvos::auto_interface<IStorageDevice> dd;
	if (dev) dev->GetStorageDevice(dd);
	//	else		global.GetDevice(dd);
	if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

	jcvos::auto_interface<tcg::ITcgSession> session;
	CreateTcgSession(session, dd);
	if (!session) throw gcnew System::ApplicationException(L"failed on creating tcg session");

	std::string pw;
	session->GetDefaultPassword(pw);
	std::wstring str_pw;
	jcvos::Utf8ToUnicode(str_pw, pw);

	WriteObject(gcnew System::String(str_pw.c_str()));
}

void SecureLet::ConnectTcgDevice::InternalProcessRecord()
{
	jcvos::auto_interface<IStorageDevice> dd;
	if (dev_num >= 0)
	{
		IStorageDevice::CreateDeviceByIndex(dd, dev_num);
	}
	else if (dev)
	{
		if (dev) dev->GetStorageDevice(dd);
		//	else		global.GetDevice(dd);
	}

	if (!dd) throw gcnew System::ApplicationException(L"device is not selected");


	jcvos::auto_interface<tcg::ITcgSession> session;
	CreateTcgSession(session, dd);
	if (!session) throw gcnew System::ApplicationException(L"failed on creating tcg session");

	TcgSession ^ ss = gcnew TcgSession(session);
	WriteObject(ss);
}


//const TCG_UID& SecureLet::SpToUid(SecureLet::TCG_SP sp)
//{
//	switch (sp)
//	{
//	case SecureLet::TCG_SP::THISSP: return OPAL_THISSP_UID;
//	case SecureLet::TCG_SP::ADMINSP: return OPAL_ADMINSP_UID;
//	case SecureLet::TCG_SP::LOCKINGSP: return OPAL_LOCKINGSP_UID;
//
//	}
//	return OPAL_UID_HEXFF;
//}
//
//
//const TCG_UID& SecureLet::AuthorityToUid(SecureLet::TCG_AUTHORITY au)
//{
//	switch (au)
//	{
//	case SecureLet::TCG_AUTHORITY::SID:			return	OPAL_SID_UID;
//	case SecureLet::TCG_AUTHORITY::ANYBODY:		return	OPAL_ANYBODY;
//	case SecureLet::TCG_AUTHORITY::ADMIN1:		return  OPAL_ADMIN1;
//	case SecureLet::TCG_AUTHORITY::USER1:		return  OPAL_USER1;
//	//case SecureLet::TCG_AUTHORITY::			return  OPAL_
//	//case SecureLet::TCG_AUTHORITY::			return  OPAL_
//	case SecureLet::TCG_AUTHORITY::TCG_NONE: return OPAL_UID_HEXFF;
//	}
//	return OPAL_UID_HEXFF;
//}
//
//const TCG_UID& SecureLet::ToUid(TCG_TABLE obj)
//{
//	switch (obj)
//	{
//	case SecureLet::TCG_TABLE::LOCKING:			return OPAL_TABLE_LOCKING;
//	case SecureLet::TCG_TABLE::GLOBAL_RANGE:	return LOCKING_GLOBAL_RANGE;
//	}
//	return OPAL_UID_HEXFF;
//	// TODO: 在此处插入 return 语句
//}

const TCG_UID& SecureLet::ToUid(SecureLet::TCG_UID_INDEX index)
{
//	TCG_UID uu;
	switch (index)
	{
	case SecureLet::TCG_UID_INDEX::THISSP:			return tcg::opal_sp::THISSP;		
	case SecureLet::TCG_UID_INDEX::ADMINSP:			return tcg::opal_sp::ADMINSP;  	
	case SecureLet::TCG_UID_INDEX::LOCKINGSP:		return tcg::opal_sp::LOCKINGSP;	
	case SecureLet::TCG_UID_INDEX::SID:				return tcg::opal_authority::SID;		
	case SecureLet::TCG_UID_INDEX::PSID:			return OPAL_PSID_UID;
	case SecureLet::TCG_UID_INDEX::ANYBODY:			return tcg::opal_authority::ANYBODY;		
	case SecureLet::TCG_UID_INDEX::ADMIN1:			return tcg::opal_authority::ADMIN1;			
	case SecureLet::TCG_UID_INDEX::USER1:			return tcg::opal_authority::USER1;			
	//case SecureLet::TCG_UID_INDEX::				return OPAL_				
	//case SecureLet::TCG_UID_INDEX::				return OPAL_				
	case SecureLet::TCG_UID_INDEX::TCG_NONE:		return OPAL_UID_HEXFF;		
	case SecureLet::TCG_UID_INDEX::LOCKING:			return OPAL_TABLE_LOCKING;	
	case SecureLet::TCG_UID_INDEX::GLOBAL_RANGE:	return LOCKING_GLOBAL_RANGE;
	default: 										return OPAL_UID_HEXFF;
	}
}

//const TCG_UID& SecureLet::ObjToUid(TCG_OBJ obj)
//{
//	// TODO: 在此处插入 return 语句
//	switch (obj)
//	{
//	case SecureLet::TCG_OBJ::TCG_SP_THISSP: return OPAL_THISSP_UID;
//	case SecureLet::TCG_OBJ::TCG_SP_ADMINSP: return OPAL_ADMINSP_UID;
//	case SecureLet::TCG_OBJ::TCG_SP_LOCKINGSP: return OPAL_LOCKINGSP_UID;
//	case SecureLet::TCG_OBJ::TCG_SID: return OPAL_SID_UID;
//	case SecureLet::TCG_OBJ::TCG_NONE: return OPAL_UID_HEXFF;
//	case SecureLet::TCG_OBJ::TCG_C_PIN_MSID:	return OPAL_C_PIN_MSID;
//	case SecureLet::TCG_OBJ::TCG_C_PIN_SID:	return OPAL_C_PIN_SID;
//
//
//	}
//	return OPAL_UID_HEXFF;
//}

void SecureLet::CParseSecurityCmd::InternalProcessRecord()
{
	if (!data) throw gcnew System::ApplicationException(L"data cannot be null");
	BYTE* _data = data->LockData();
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);
	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	parser->ParseSecurityCommand(sec_obj, _data, data->Length, protocol, comid, true);

#ifdef _DEBUG
	boost::property_tree::wptree prop;
	sec_obj->ToProperty(prop);
	boost::property_tree::write_xml(std::wcout, prop);
#endif

	//CTcgFeatureSet* _fset = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
	//if (_fset == nullptr) THROW_ERROR(ERR_APP, L"failed on creating feature set");
	//global.m_feature_description.Parse(*_fset, _data, data->Length);
	data->Unlock();
	// convert to .net object
	TcgFeatureSet^ feature_set = gcnew TcgFeatureSet(sec_obj);
	WriteObject(feature_set);
}

void SecureLet::GetProtocol::InternalProcessRecord()
{
	jcvos::auto_interface<IStorageDevice> dd;
	if (dev) dev->GetStorageDevice(dd);
	//else		global.GetDevice(dd);
	if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

	jcvos::auto_interface<tcg::ITcgSession> tcg;
	CreateTcgSession(tcg, dd);
	if (!tcg) throw gcnew System::ApplicationException(L"failed on creating tcg session");

	//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
	//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");

	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(1));
	BYTE* _buf = buf->Lock();
	bool br = tcg->GetProtocol(_buf, SECTOR_TO_BYTE(1) );
	buf->Unlock(_buf);
	if (!br) wprintf_s(L"[err] failed on calling L0Discovery");


	_buf = buf->Lock();
	jcvos::auto_interface<tcg::ISecurityParser> parser;
	tcg::GetSecurityParser(parser);
	jcvos::auto_interface<tcg::ISecurityObject> sec_obj;
	parser->ParseSecurityCommand(sec_obj, _buf, buf->GetSize(), tcg::PROTOCOL_INFO, 0, true);
	buf->Unlock(_buf);

	sec_obj->ToString(std::wcout, -1, 0);

	JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
	WriteObject(data);
}

#pragma once

using namespace System;
using namespace System::Management::Automation;

#include <jccmdlet-comm.h>
#include "../include/disk_info.h"
#include "../include/storage_device.h"
#include "global_init.h"

using tcg::ITcgSession;

//extern StaticInit global;

// 配置：如果USING_DIRECTORY_FOR_FEATURE有效，则feature set被作为以feature name为索引的字典。否则feature set是一个由feature组成的数组。
//#define USING_DIRECTORY_FOR_FEATURE

namespace SecureLet
{

	[CmdletAttribute(VerbsCommon::Get, "L0Discovery")]
	public ref class GetL0Discovery : public JcCmdLet::JcCmdletBase
	{
	public:
		GetL0Discovery(void) {};
		~GetL0Discovery(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

	public:
		virtual void InternalProcessRecord() override
		{
			jcvos::auto_interface<IStorageDevice> dd;
			if (dev) dev->GetStorageDevice(dd);
			else		global.GetDevice(dd);
			if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

			jcvos::auto_interface<ITcgSession> tcg;
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
			
			JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
			WriteObject(data);
		}

	protected:
	};

	[CmdletAttribute(VerbsCommon::Get, "Protocol")]
	public ref class GetProtocol : public JcCmdLet::JcCmdletBase
	{
	public:
		GetProtocol(void) {};
		~GetProtocol(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

	public:
		virtual void InternalProcessRecord() override
		{
			//jcvos::auto_interface<IStorageDevice> dd;
			//if (dev) dev->GetStorageDevice(dd);
			//else		global.GetDevice(dd);

			//if (!dd) throw gcnew System::ApplicationException(L"device is not selected");
			//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
			//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");

			//jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			//jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(1));
			//BYTE* _buf = buf->Lock();
			//bool br = tcg->L0Discovery(_buf);
			//buf->Unlock(_buf);
			//if (!br) wprintf_s(L"[err] failed on calling L0Discovery");

			//JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
			//WriteObject(data);
		}

	protected:
	};

	// 导入L0Discovery的配置文件(json)
	[CmdletAttribute(VerbsData::Import, "L0Discovery")]
	public ref class CImportL0Discovery : public JcCmdLet::JcCmdletBase
	{
	public:
		CImportL0Discovery(void) {};
		~CImportL0Discovery(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "json configuraiton file name")]
		property String^ config;

	public:
		virtual void InternalProcessRecord() override
		{
			std::wstring str_fn;
			ToStdString(str_fn, config);
			global.m_feature_description.LoadConfig(str_fn);
		}
	};

	public ref class TcgFeature : public System::Object
	{
	public:
		property System::String^ name;
		WORD code;
		BYTE version;
		BYTE length;
		System::Collections::Generic::Dictionary<String^, int> fields;
	};

	public ref class TcgFeatureSet : public System::Object
	{
	public:
		DWORD length;
		WORD ver_major;
		WORD ver_minor;
#ifdef USING_DIRECTORY_FOR_FEATURE
		System::Collections::Generic::Dictionary<String^, TcgFeature^> features;
#else
		System::Collections::ArrayList features;
#endif
	};

	// 解析L0Discovery
	[CmdletAttribute(VerbsData::ConvertFrom, "L0Discovery")]
	public ref class CParseL0Discovery : public JcCmdLet::JcCmdletBase
	{
	public:
		CParseL0Discovery(void) {};
		~CParseL0Discovery(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "payload data of L0Discovery")]
		property JcCmdLet::BinaryType^ data;

	public:
		virtual void InternalProcessRecord() override
		{
			if (!data) throw gcnew System::ApplicationException(L"data cannot be null");
			BYTE* _data = data->LockData();
			CTcgFeatureSet* _fset = jcvos::CDynamicInstance<CTcgFeatureSet>::Create();
			if (_fset == nullptr) THROW_ERROR(ERR_APP, L"failed on creating feature set");
			global.m_feature_description.Parse(*_fset, _data, data->Length);
			data->Unlock();
			// convert to .net object
			TcgFeatureSet^ feature_set = gcnew TcgFeatureSet;
			feature_set->length = _fset->m_header.length_of_parameter;
			feature_set->ver_major = _fset->m_header.major_version;
			feature_set->ver_minor = _fset->m_header.minor_version;
			//feature_set->vendor

			for (auto it = _fset->m_features.begin(); it != _fset->m_features.end(); ++it)
			{
				TcgFeature^ feature = gcnew TcgFeature;
				feature->name = gcnew String(it->m_name.c_str());
				feature->code = it->m_code;
				feature->version = it->m_version;
				feature->length = it->m_length;
				for (auto ff = (it->m_features.begin()); ff != (it->m_features.end()); ++ff)
				{
					feature->fields.Add(gcnew String(ff->first.c_str()), ff->second.get_value<int>());
				}
#ifdef USING_DIRECTORY_FOR_FEATURE
				feature_set->features.Add(gcnew String(feature->name), feature);
#else
				feature_set->features.Add(feature);
#endif
			}
			WriteObject(feature_set);
		}
	};

//=============================================================================

	public enum class TCG_OBJ
	{
		TCG_SP_THISSP, TCG_SP_ADMINSP, TCG_SP_LOCKINGSP,
		TCG_NONE, TCG_SID, 

		TCG_C_PIN_MSID, TCG_C_PIN_SID,

	};

#define TCG_SP			TCG_OBJ
#define TCG_AUTHORITY	TCG_OBJ

	//public enum class TCG_SP
	//{
	//	TCG_SP_THISSP, TCG_SP_ADMINSP, TCG_SP_LOCKINGSP,
	//};

	//public enum class TCG_AUTHORITY
	//{
	//	TCG_NONE, TCG_SID, 
	//};

	public ref class TcgSession : public Object
	{
	public:
		TcgSession(ITcgSession * session): m_session(nullptr)
		{
			m_session = session; JCASSERT(m_session);
			m_session->AddRef();
		};
		~TcgSession(void) { RELEASE_INTERFACE(m_session); }
		!TcgSession(void) { RELEASE_INTERFACE(m_session); }
		void GetSession(ITcgSession*& session)
		{
			JCASSERT(session == nullptr);
			session = m_session;
			session->AddRef();
		}
	protected:
		ITcgSession* m_session;
	};

	const TCG_UID& SpToUid(TCG_SP sp);
	const TCG_UID& AuthorityToUid(TCG_AUTHORITY au);
	const TCG_UID& ObjToUid(TCG_OBJ obj);
// 
//== TCG Methods
	[CmdletAttribute(VerbsLifecycle::Start, "TcgSession")]
	public ref class CStartTcgSession : public JcCmdLet::JcCmdletBase
	{
	public:
		CStartTcgSession(void) { challenge = nullptr; authority = TCG_AUTHORITY::TCG_NONE; };
		~CStartTcgSession(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "SP of the session to start")]
		property TCG_SP sp;

		[Parameter(Position = 2,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true,
			HelpMessage = "Authority for session")]
		property TCG_AUTHORITY authority;

		[Parameter(Position = 3,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, 
			HelpMessage = "password for authority")]
		property String^ challenge;

		[Parameter(Position = 4,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, 
			HelpMessage = "set to write")]
		property SwitchParameter write;
	public:
		virtual void InternalProcessRecord() override
		{
			jcvos::auto_interface<IStorageDevice> dd;
			if (dev) dev->GetStorageDevice(dd);
			else		global.GetDevice(dd);
			if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

			jcvos::auto_interface<ITcgSession> tcg;
			CreateTcgSession(tcg, dd);
			if (!tcg) throw gcnew System::ApplicationException(L"failed on creating tcg session");

			const char* str_pwd = nullptr;
			std::string _str_pwd;
			if (challenge != nullptr)
			{
				std::wstring wstr_pwd;
				ToStdString(wstr_pwd, challenge);
				jcvos::UnicodeToUtf8(_str_pwd, wstr_pwd);
				str_pwd = _str_pwd.c_str();
			}
			BYTE br = tcg->StartSession(SpToUid(sp), str_pwd, AuthorityToUid(authority), write.ToBool());
			if (br == 0) WriteDebug(L"Start session succeeded.");
			else WriteDebug(String::Format(L"Start session failed, code={1}", br));
			WriteObject(gcnew TcgSession(tcg));
		}
	};

	[CmdletAttribute(VerbsLifecycle::Stop, "TcgSession")]
	public ref class CStopTcgSession : public JcCmdLet::JcCmdletBase
	{
	public:
		CStopTcgSession(void) {};
		~CStopTcgSession(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property TcgSession ^ session;

	public:
		virtual void InternalProcessRecord() override
		{
			jcvos::auto_interface<ITcgSession> ss;
			session->GetSession(ss);
			BYTE br = ss->EndSession();
			if (br == 0) WriteDebug(L"End session succeeded.");
			else WriteDebug(String::Format(L"End session failed, code={0}", br));
		}
	};


	[CmdletAttribute(VerbsCommon::Get, "TcgTable")]
	public ref class GetTcgTable : public JcCmdLet::JcCmdletBase
	{
	public:
		GetTcgTable(void) {};
		~GetTcgTable(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property TcgSession^ session;

		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify the table to get")]
		property TCG_OBJ table;

		[Parameter(Position = 2,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "start column to get")]
		property int start_col;

		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "end column to get")]
		property int end_col;
	public:
		virtual void InternalProcessRecord() override
		{
			//jcvos::auto_interface<ITcgSession> ss;
			//session->GetSession(ss);
			//
			//DtaResponse res;
			//BYTE br = ss->GetTable(res, ObjToUid(table), start_col, end_col);

		}
	};


//=============================================================================
//== 测试TCG功能

	[CmdletAttribute(VerbsCommon::New, "TcgTestDevice")]
	public ref class CNewTestDevice : public JcCmdLet::JcCmdletBase
	{
	public:
		CNewTestDevice(void) {};
		~CNewTestDevice(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "folder path of the data files")]
		property String^ path;

	public:
		virtual void InternalProcessRecord() override
		{
			std::wstring str_path;
			ToStdString(str_path, path);
			jcvos::auto_interface<IStorageDevice> dd;
			tcg::CreateTcgTestDevice(dd, str_path);
			if (!dd) throw gcnew System::ApplicationException(L"failed on creating test device");
			Clone::StorageDevice^ dev = gcnew Clone::StorageDevice(dd);
			WriteObject(dev);
		}
	};


	// 解析L0Discovery
	[CmdletAttribute(VerbsDiagnostic::Test, "SecUtility")]
	public ref class CTestSecurityUtility : public JcCmdLet::JcCmdletBase
	{
	public:
		CTestSecurityUtility(void) {};
		~CTestSecurityUtility(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property String^ password;

	public:
		virtual void InternalProcessRecord() override
		{
			jcvos::auto_interface<IStorageDevice> dd;
			if (dev) dev->GetStorageDevice(dd);
			else		global.GetDevice(dd);
			if (!dd) throw gcnew System::ApplicationException(L"device is not selected");

			jcvos::auto_interface<ITcgSession> tcg;
			CreateTcgSession(tcg, dd);
			if (!tcg) throw gcnew System::ApplicationException(L"failed on creating tcg session");

			std::string cur_pwd;
			tcg->GetDefaultPassword(cur_pwd);
			std::wstring str_pwd;
			jcvos::Utf8ToUnicode(str_pwd, cur_pwd);

			wprintf_s(L"cur password=%S", cur_pwd.c_str());

//			std::wstring str_new_pw;
			ToStdString(str_pwd, password);
			std::string new_pwd;
			jcvos::UnicodeToUtf8(new_pwd, str_pwd);
			tcg->Reset();
			tcg->SetSIDPassword(cur_pwd.c_str(), new_pwd.c_str());

//			WriteObject(gcnew String(str_pw.c_str()));
		}
	};

}
#pragma once

using namespace System;
using namespace System::Management::Automation;

#include <jccmdlet-comm.h>
#include "../include/disk_info.h"
#include "../include/storage_device.h"
#include "global_init.h"

using tcg::ITcgSession;
using tcg::ISecurityObject;

//extern StaticInit global;
//#pragma make_public(tcg::ISecurityObject)
//[assembly:InternalsVisibleTo("TcgSession")];


namespace SecureLet
{
	//Move to Secure Let

	//[CmdletAttribute(VerbsCommon::Get, "Protocol")]
	//public ref class GetProtocol : public JcCmdLet::JcCmdletBase
	//{
	//public:
	//	GetProtocol(void) {};
	//	~GetProtocol(void) {};

	//public:
	//	[Parameter(Position = 0,
	//		ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
	//		HelpMessage = "specify device object")]
	//	property Clone::StorageDevice^ dev;

	//public:
	//	virtual void InternalProcessRecord() override
	//	{
	//		//jcvos::auto_interface<IStorageDevice> dd;
	//		//if (dev) dev->GetStorageDevice(dd);
	//		//else		global.GetDevice(dd);

	//		//if (!dd) throw gcnew System::ApplicationException(L"device is not selected");
	//		//ITcgDevice* tcg = dd.d_cast<ITcgDevice*>();
	//		//if (!tcg) throw gcnew System::ApplicationException(L"device does not support TCG");

	//		//jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	//		//jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(1));
	//		//BYTE* _buf = buf->Lock();
	//		//bool br = tcg->L0Discovery(_buf);
	//		//buf->Unlock(_buf);
	//		//if (!br) wprintf_s(L"[err] failed on calling L0Discovery");

	//		//JcCmdLet::BinaryType^ data = gcnew JcCmdLet::BinaryType(buf);
	//		//WriteObject(data);
	//	}

	//protected:
	//};

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



// 
//== TCG Methods
/*
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
*/


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
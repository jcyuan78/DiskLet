///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <jccmdlet-comm.h>
#include <LibTcg.h>

#include "global.h"

//#define RELEASE_INTERFACE(ptr) {if (ptr) ptr->Release(); ptr=NULL; 	}

#include "secure-types.h"

using namespace System;
using namespace System::Management::Automation;
//using namespace DiskLet;

//#pragma make_public(tcg::ISecurityObject)

namespace SecureLet
{
	public ref class TcgFeature : public System::Object
	{
	public:
		TcgFeature(CTcgFeature* ff)
		{
			name = gcnew String(ff->m_name.c_str());
			code = ff->m_code;
			version = ff->m_version;
			length = ff->m_length;
			for (auto it = (ff->m_features.begin()); it != (ff->m_features.end()); ++it)
			{
				fields.Add(gcnew String(it->first.c_str()), it->second.get_value<int>());
			}
		}
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
		TcgFeatureSet(tcg::ISecurityObject* ff)
		{
			CTcgFeatureSet* _fset = dynamic_cast<CTcgFeatureSet*>(ff);
			if (_fset == NULL) THROW_ERROR(ERR_APP, L"need CTcgFeature object.");
			// convert to .net object
			length = _fset->m_header.length_of_parameter;
			ver_major = _fset->m_header.major_version;
			ver_minor = _fset->m_header.minor_version;
			//feature_set->vendor

			for (auto it = _fset->m_features.begin(); it != _fset->m_features.end(); ++it)
			{
				TcgFeature^ feature = gcnew TcgFeature(&(*it));
#ifdef USING_DIRECTORY_FOR_FEATURE
				features.Add(gcnew String(feature->name), feature);
#else
				features.Add(feature);
#endif
			}
		}

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



//#define TCG_SP			TCG_OBJ
//#define TCG_AUTHORITY	TCG_OBJ

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
		TcgSession(tcg::ITcgSession* session) : m_session(nullptr)
		{
			m_session = session; JCASSERT(m_session);
			m_session->AddRef();
		};
		~TcgSession(void) { RELEASE_INTERFACE(m_session); }
		!TcgSession(void) { RELEASE_INTERFACE(m_session); }
		void GetSession(tcg::ITcgSession*& session)
		{
			JCASSERT(session == nullptr);
			session = m_session;
			session->AddRef();
		}

	public:
		TcgFeatureSet^ GetFeatures(bool force)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> feature;
			bool br = m_session->GetFeatures(feature, force);
			if (!br) return nullptr;
			return gcnew TcgFeatureSet(feature);
		}

		void StartSession(TCG_SP sp, TCG_AUTHORITY auth, String^ challenge, bool write)
		{
			JCASSERT(m_session);
			const char* pw = nullptr;
			std::string str_pw;
			if (challenge != nullptr)
			{
				std::wstring wstr_pwd;
				ToStdString(wstr_pwd, challenge);
				if (!wstr_pwd.empty())
				{
					jcvos::UnicodeToUtf8(str_pw, wstr_pwd);
					pw = str_pw.c_str();
				}
			}

			BYTE br = m_session->StartSession(SpToUid(sp), pw, AuthorityToUid(auth), write);
//			return br;
		}

		void EndSession(void)
		{
			JCASSERT(m_session);
			BYTE br = m_session->EndSession();
//			return br;
		}

		void GetTable(TcgUid^ uid, WORD start_col, WORD end_col)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> res;
			
			BYTE err = m_session->GetTable(res, uid->GetUid(), start_col, end_col);
		}

		void SetTable(TcgUid^ table, int col, String^ val)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> res;
			std::wstring wstr_val;
			ToStdString(wstr_val, val);
			std::string str_val;
			jcvos::UnicodeToUtf8(str_val, wstr_val);

			BYTE err = m_session->SetTable(res, table->GetUid(), col, str_val.c_str());
		}
		void SetTable(TcgUid^ table, int col, int val)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> res;
			BYTE err = m_session->SetTable(res, table->GetUid(), col,val);
		}


		void Activate(TcgUid^ uid)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> res;
			BYTE err = m_session->Activate(res, uid->GetUid());

		}
	protected:
		tcg::ITcgSession* m_session;
	};


	[CmdletAttribute(VerbsData::Initialize, "SecurityParser")]
	public ref class InitSecurityParser : public JcCmdLet::JcCmdletBase
	{
	public:
		InitSecurityParser(void) {};
		~InitSecurityParser(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true, HelpMessage = "uuid config filename.json")]
		property String^ uuid;

		[Parameter(Position = 1, Mandatory = true, HelpMessage = "l0 discovery config filename.json")]
		property String^ l0discovery;

	public:
		virtual void InternalProcessRecord() override;
	};


	[CmdletAttribute(VerbsCommon::Get, "TPerProperties")]
	public ref class GetTPerProperties : public JcCmdLet::JcCmdletBase
	{
	public:
		GetTPerProperties(void) {};
		~GetTPerProperties(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Get, "DefaultPassword")]
	public ref class GetDefaultPassword : public JcCmdLet::JcCmdletBase
	{
	public:
		GetDefaultPassword(void) {};
		~GetDefaultPassword(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

	public:
		virtual void InternalProcessRecord() override;
	};

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
		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, 
			HelpMessage = "output binary data")]
		property SwitchParameter OutBinary;


	public:
		virtual void InternalProcessRecord() override;

	protected:
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
		virtual void InternalProcessRecord() override;

	};


	[CmdletAttribute(VerbsCommunications::Connect, "TcgDevice")]
	public ref class ConnectTcgDevice : public JcCmdLet::JcCmdletBase
	{
	public:
		ConnectTcgDevice(void) { dev = nullptr; dev_num = -1; };
		~ConnectTcgDevice(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			ParameterSetName = "ByDevice",
			HelpMessage = "specify device object")]
		property Clone::StorageDevice^ dev;

		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			ParameterSetName = "ByIndex",
			HelpMessage = "specify device object")]
		property int dev_num;


	public:
		virtual void InternalProcessRecord() override;
	};




};



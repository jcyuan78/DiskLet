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

	public enum class EncodeType{
		

	};

	public ref class TcgSession : public Object
	{
	public:
		enum class EncodeType {		// 密码的编码形式。用户界面上，密码都是以字符串形式表示。字符如何编码二进制串
			ASCII,	// ASCII字符表示
			HEX,	// 以字节为单位，2个字符表示一个字节，
			BASE32, BASE64	// 标准编码
		};

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

		void StartSession(TcgUid ^sp, TcgUid ^ auth, String^ challenge, bool write, EncodeType encode)
		{
			JCASSERT(m_session);
			const char* pw = nullptr;
			std::string str_pwd;
			if (challenge != nullptr)
			{
				ToStdString(str_pwd, challenge);
				switch (encode)
				{
				case EncodeType::ASCII:	pw = str_pwd.c_str();	break;
				case EncodeType::HEX:	pw = HexDecoder(str_pwd); break;
				}
			}
			BYTE err = m_session->StartSession(sp->GetUid(), pw, auth->GetUid(), write);
			if (encode == EncodeType::HEX) delete[] pw;
			if (err) throw gcnew System::ApplicationException(L"failed on starting session");
		}

		void EndSession(void)
		{
			JCASSERT(m_session);
			BYTE br = m_session->EndSession();
		}

		void GetTable(TcgUid^ uid, WORD start_col, WORD end_col)
		{
			JCASSERT(m_session);
//			jcvos::auto_interface<tcg::ISecurityObject> res;
			boost::property_tree::wptree res;
			m_session->GetTable(res, uid->GetUid(), start_col, end_col);
		}

		//void SetTable(TcgUid^ table, int col, String^ val)
		//{
		//	JCASSERT(m_session);
		//	jcvos::auto_interface<tcg::ISecurityObject> res;
		//	std::wstring wstr_val;
		//	ToStdString(wstr_val, val);
		//	std::string str_val;
		//	jcvos::UnicodeToUtf8(str_val, wstr_val);

		//	BYTE err = m_session->SetTable(res, table->GetUid(), col, str_val.c_str());
		//}

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
			if (err) throw gcnew System::ApplicationException(L"failed on Activit");
		}

		void Revert(TcgUid^ uid)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<tcg::ISecurityObject> res;
			BYTE err = m_session->Revert(res, uid->GetUid());
			if (err) throw gcnew System::ApplicationException(L"failed on Reverting");
		}

		void TperReset(void)
		{
			JCASSERT(m_session);
			BYTE err = m_session->TperReset();
			if (err) throw gcnew System::ApplicationException(L"failed on TperReset");
		}

	// == high level funcsions == 
		void SetACE(TcgUid^ ace_obj, Array^ authorities)
		{
			JCASSERT(m_session);
			int len = authorities->Length;
			std::vector<const BYTE *> aa;
			for (int ii = 0; ii < authorities->Length; ++ii)
			{
				System::Object ^ obj = authorities->GetValue(ii);
				TcgUid^ uid = dynamic_cast<TcgUid^>(obj);
				aa.push_back(uid->GetUid());
			}
			m_session->SetACE(ace_obj->GetUid(), aa);
		}

		void PSIDRevert(String^ pwd)
		{
			JCASSERT(m_session);
			std::string str_pw;
			ToStdString(str_pw, pwd);

			BYTE err = m_session->RevertTPer(str_pw.c_str(), OPAL_PSID_UID, tcg::opal_sp::ADMINSP);
			if (err) throw gcnew System::ApplicationException(L"failed on PSID reverting");
		}

		void SetLockingRange(UINT range, UINT64 start_lba, UINT64 sectors)
		{
			JCASSERT(m_session);
			BYTE err = m_session->SetLockingRange(range, start_lba, sectors);
			if (err) throw gcnew System::ApplicationException(L"failed on setting locking range");
		}

		void SetPassword(TcgUid^ auth, String^ pwd)
		{
			JCASSERT(m_session);
			std::string str_pwd;
			ToStdString(str_pwd, pwd);
			m_session->SetPassword(auth->GetUid(), str_pwd.c_str());
		}

		void SetUserPassword(bool admin, UINT user_id, String^ password)
		{
			JCASSERT(m_session);
			const char* pw = nullptr;
			std::string str_pwd;
			ToStdString(str_pwd, password);
			if (str_pwd.empty())	throw gcnew System::ApplicationException(L"password cannot be empty");
			TCG_UID authority_id;
			memcpy_s(authority_id, sizeof(TCG_UID), OPAL_AUTHORITY_TABLE, sizeof(TCG_UID));
			authority_id[5] = admin? 1:3;
			authority_id[3] = 0x0B;
			authority_id[7] = (BYTE)user_id;

			m_session->SetPassword(authority_id, str_pwd.c_str());
		}

		void WriteShadowMBR(JcCmdLet::BinaryType^ data)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			data->GetData(buf);
			BYTE err = m_session->WriteShadowMBR(buf);
			if (err) throw gcnew System::ApplicationException(L"failed on setting MBR");
		}

		JcCmdLet::BinaryType^ ReadDataTable(TcgUid^ obj, size_t offset, size_t len)
		{
			JCASSERT(m_session);
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			jcvos::CreateBinaryBuffer(buf, len);
			BYTE * _buf = buf->Lock();
			size_t out_len = m_session->ReadDataTable(obj->GetUid(), _buf, offset, len);
			buf->Unlock(_buf);
			return gcnew JcCmdLet::BinaryType(buf);
		}

		void AssignRangeToUser(UINT range_id, UINT user_id)
		{
			JCASSERT(m_session);
			m_session->AssignRangeToUser(range_id, user_id, false);
		}

		void BlockSID(int clear_event, int freeze)
		{
			JCASSERT(m_session);
			BYTE err = m_session->BlockSID((BYTE)clear_event, (BYTE)freeze);
			wprintf_s(L"Result: 0x%02X", err);
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
		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true,
			HelpMessage = "output binary data")]
		property SwitchParameter OutBinary;

	public:
		virtual void InternalProcessRecord() override;

	protected:
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
	[CmdletAttribute(VerbsData::ConvertFrom, "SecurityCmd")]
	public ref class CParseSecurityCmd : public JcCmdLet::JcCmdletBase
	{
	public:
		CParseSecurityCmd(void) {};
		~CParseSecurityCmd(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "payload data of security command")]
		property JcCmdLet::BinaryType^ data;
		[Parameter(Position = 1,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "protocol id")]
		property UINT protocol;
		[Parameter(Position = 2,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "comid")]
		property UINT comid;


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



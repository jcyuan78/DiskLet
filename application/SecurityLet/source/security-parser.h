///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

using namespace System;
using namespace System::Management::Automation;

#include <LibTcg.h>
#include <jccmdlet-comm.h>


namespace SecurityParser
{
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == 
	public ref class SecurityObj : public System::IFormattable
	{
	public:
		SecurityObj(tcg::ISecurityObject* obj) : m_obj(obj), nvme_id(0) { if (m_obj) m_obj->AddRef(); };
		~SecurityObj(void) { if (m_obj) m_obj->Release(); }
		//!SecurityObj(void) { if (m_obj) m_obj->Release(); }


	public:
		JcCmdLet::BinaryType^ GetPayload(int index)
		{
			jcvos::auto_interface<jcvos::IBinaryBuffer> data;
			m_obj->GetPayload(data, index);
//			if (!data) throw gcnew System::ApplicationException(L"failed on getting payload");
			if (!data) return nullptr;
			return gcnew JcCmdLet::BinaryType(data);
		}
		virtual String^ ToString(System::String^ format, IFormatProvider^ formaterprovider)
		{
			UINT level = UINT_MAX;
			std::wstring ff;
			if (format)	ToStdString(ff, format);
			if (!ff.empty())
			{
				if (ff.at(0) == 'L')
				{
					swscanf_s(ff.c_str() + 1, L"%d", &level);
				}
			}
			std::wstringstream stream;
			m_obj->ToString(stream, level, 0);
			return gcnew String(stream.str().c_str());
		};
		virtual String^ ToString(System::String^ format) { return ToString(format, nullptr); }

		SecurityObj^ GetSubItem(String^ child)
		{
			if (m_obj == nullptr) return nullptr;
			std::wstring name;
			ToStdString(name, child);
			jcvos::auto_interface<tcg::ISecurityObject> sub_item;
			m_obj->GetSubItem(sub_item, name);
			SecurityObj^ sub_cmd = nullptr;
			if (sub_item)		sub_cmd = gcnew SecurityObj(sub_item);
			return sub_cmd;
		}

		void GetObject(tcg::ISecurityObject*& obj) { obj = m_obj; if (obj) obj->AddRef(); }
		void SetNvmeId(UINT id) { nvme_id = id; }
		UINT GetNvmeId(void) { return nvme_id; }

	protected:
		UINT nvme_id;
		tcg::ISecurityObject* m_obj;
	};

	public ref class SecurityCmd : public SecurityObj
	{
	public:
		SecurityCmd(tcg::ISecurityObject* obj) : SecurityObj(obj) {};

	public:
		UINT cmd_id;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == 
	[CmdletAttribute(VerbsData::Convert, "SecurityCmd")]
	public ref class ParseSecurity : public JcCmdLet::JcCmdletBase
	{
	public:
		ParseSecurity(void) { nvme_id = 0; };
		~ParseSecurity(void) {};

	public:

		[Parameter(Position = 0, /*ValueFromPipelineByPropertyName = true,*/ ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "payload of security command")]
		property JcCmdLet::BinaryType^ payload;

		[Parameter(Position = 1, Mandatory = false, /*ParameterSetName = "ByIndex",*/
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "Clear a disk")]
		property DWORD cdw10;

		[Parameter(Position = 2,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property bool receive;

		[Parameter(Position = 3,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, 
			HelpMessage = "NVMe ID of the command")]
		property UINT nvme_id;


	public:
		virtual void InternalProcessRecord() override;
	};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// == 
	[CmdletAttribute(VerbsData::Export, "SecurityCmd")]
	public ref class ExportSecurity : public JcCmdLet::JcCmdletBase
	{
	public:
		ExportSecurity(void) { level = UINT_MAX; option = 0; };
		~ExportSecurity(void) {};

	public:

		[Parameter(/*Position = 0, */ValueFromPipelineByPropertyName = true,ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "securty boject")]
		property SecurityObj^ payload;

		[Parameter(Position = 0, Mandatory = false, /*ParameterSetName = "ByIndex",*/
			/*ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,*/
			HelpMessage = "filename to export")]
		property String ^ filename;

		[Parameter(Position = 1,
			/*ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,*/
			HelpMessage = "level to show")]
		property UINT level;
		[Parameter(Position = 2,
			/*ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,*/
			HelpMessage = "option")]
		property int option;

	public:
		virtual void BeginProcessing() override;
		virtual void EndProcessing() override;
		virtual void InternalProcessRecord() override;

	protected:
		std::wofstream * m_output;
	};


	
}
#pragma once

//#include "../include/jcbuffer.h"
#include <jccmdlet-comm.h>
#include "tcg-token.h"

#include <boost/endian.hpp>
using namespace boost::endian;
using namespace System::Management::Automation;


bool AnalyzeTcgProtocol(CTcgTokenBase* tt);


namespace tcg_parser
{
	public ref class TcgToken : public System::Object
	{
	public:
		TcgToken(void);
		TcgToken(CTcgTokenBase* tt) { m_token = tt; }
		~TcgToken(void);
		!TcgToken(void);

	public:
		System::String ^ Print(void);
		CTcgTokenBase* GetToken(void) { return m_token; }

	protected:
		CTcgTokenBase * m_token;
	};


	[CmdletAttribute(VerbsData::Convert, "TcgToken")]
	public ref class ParseTcgToken : public JcCmdLet::JcCmdletBase
	{
	public:
		ParseTcgToken(void);
		~ParseTcgToken(void);
	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		property JcCmdLet::BinaryType^ Payload;
		[Parameter(Position = 1,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		//property SwitchParameter receive;
		property bool receive;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsData::Convert, "TcgProtocol")]
	public ref class ParseTcgProtocol : public JcCmdLet::JcCmdletBase
	{
	public:
		ParseTcgProtocol(void) { token = nullptr; };
		~ParseTcgProtocol(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input token")]
		[ValidateNotNullOrEmpty]
		property TcgToken^ token;

	public:
		virtual void InternalProcessRecord() override
		{
			CTcgTokenBase * tt = token->GetToken();
		}
	};


	[CmdletAttribute(VerbsData::Import, "UIDs")]
	public ref class ImportUid : public JcCmdLet::JcCmdletBase
	{
	public:
		ImportUid(void) { };
		~ImportUid(void) {  };

	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input token")]
		[ValidateNotNullOrEmpty]
		property String ^ fn;

	public:
		virtual void InternalProcessRecord() override
		{
			std::wstring str_fn;
			ToStdString(str_fn, fn);

//			CUidMap uid_map;
			g_uid_map.Clear();
			g_uid_map.Load(str_fn);

		}
	};

}
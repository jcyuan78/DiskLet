#pragma once

#include "../include/jcbuffer.h"
#include "tcg_token.h"

#include <boost/endian.hpp>
using namespace boost::endian;


class SUBPACKET
{
public:
	BYTE reserved[6];
	WORD kind;
	DWORD length;
	union 
	{
		BYTE payload[1];
		DWORD credit;
	};
};

class PACKET
{
public:
	UINT64 session;
	DWORD seq_num;
	WORD reserved;
	WORD ack_type;
	DWORD acknowlede;
	DWORD length;
	BYTE payload[1];
};

class COM_PACKET
{
public:
	DWORD reserved;
	WORD com_id;
	WORD com_id_ex;
	DWORD outstanding;
	DWORD min_transfer;
	DWORD length;
	BYTE payload[1];
};





bool AnalyzeTcgProtocol(CTcgTokenBase* tt);


namespace tcg_parser
{
	public ref class SubPacket :public System::Object
	{
	public:
		SubPacket(SUBPACKET* sub_packet, size_t src_len);
		~SubPacket(void);
		!SubPacket(void);

	public:
		WORD kind;
		DWORD length;
		DWORD credit;
		JcCmdLet::BinaryType^ payload;
	};

	public ref class Packet : public System::Object
	{
	public:
		Packet(PACKET* packet, size_t src_len);
		~Packet(void);
		!Packet(void);
	public:
		UINT64 session;
		property UINT32 TSN {UINT32 get() { return HIDWORD(session); }}
		property UINT32 HSN {UINT32 get() { return LODWORD(session); }}
		DWORD seq_num;
		WORD ack_type;
		DWORD acknowlede;
		DWORD length;
		SubPacket^ sub_packet;
		JcCmdLet::BinaryType^ payload;
	};

	public ref class ComPacket : public System::Object
	{
	public:
		ComPacket(COM_PACKET* packet, size_t len);
		~ComPacket(void);
		!ComPacket(void);
	public:
		WORD com_id;
		WORD com_id_ex;
		DWORD outstanding;
		DWORD min_transfer;
		DWORD length;
		JcCmdLet::BinaryType^ payload;
		Packet^ packet;
	};



	[CmdletAttribute(VerbsData::ConvertFrom, "TcgPayload")]
	public ref class ConvertTcgPayload : public JcCmdLet::JcCmdletBase
	{
	public:
		ConvertTcgPayload(void) { Payload = nullptr; CDW10 = 0xFFFFFFFF; };
		~ConvertTcgPayload(void) {/* delete Payload;*/ };

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
		property bool receive;

		[Parameter(Position = 2,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		property DWORD CDW10;

		[Parameter(Position = 3,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		property DWORD CDW11;

	public:
		virtual void InternalProcessRecord() override;
	};


	public ref class TcgToken : public System::Object
	{
	public:
		TcgToken(void);
		TcgToken(CTcgTokenBase* tt) { m_token = tt; }
		~TcgToken(void);
		!TcgToken(void);

	public:
		void Print(void);
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
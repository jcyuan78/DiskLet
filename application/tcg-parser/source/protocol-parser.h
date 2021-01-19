#pragma once

#include "l0discovery.h"

#include <jccmdlet-comm.h>
#include <boost/cast.hpp>
#include <boost/endian.hpp>
using namespace boost::endian;
using namespace System::Management::Automation;


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


JcCmdLet::BinaryType^ CopyPayload(void* data, size_t length)
{
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	jcvos::CreateBinaryBuffer(buf, length);
	BYTE* _buf = buf->Lock();
	memcpy_s(_buf, length, data, length);
	buf->Unlock(_buf);
	return gcnew JcCmdLet::BinaryType(buf);
}

namespace tcg_parser
{
	public ref class SubPacket :public System::Object
	{
	public:
		SubPacket(SUBPACKET* sub_packet, size_t src_len)
			: payload(nullptr)
		{
			kind = endian_reverse(sub_packet->kind);
			length = endian_reverse(sub_packet->length);
			size_t offset = offsetof(SUBPACKET, payload);
			if (length + offset > src_len)
				THROW_ERROR(ERR_APP, L"len feild (%d) is larger than source size (%d)", length, src_len - offset);
			if (kind == 0x8001) credit = sub_packet->credit;
			else credit = 0;
			payload = CopyPayload(sub_packet->payload, length);
		}

		~SubPacket(void)
		{
			delete payload;
			payload = nullptr;
		}

		!SubPacket(void)
		{
			delete payload;
			payload = nullptr;
		}

	public:
		WORD kind;
		DWORD length;
		DWORD credit;
		JcCmdLet::BinaryType^ payload;
	};

	public ref class Packet : public System::Object
	{
	public:
		Packet(PACKET* packet, size_t src_len)
			: payload(nullptr)
		{
			session = endian_reverse(packet->session);
			seq_num = endian_reverse(packet->seq_num);
			ack_type = endian_reverse(packet->ack_type);
			acknowlede = endian_reverse(packet->acknowlede);
			length = endian_reverse(packet->length);
			size_t offset = offsetof(PACKET, payload);
			if (length + offset > src_len)
				THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%d)", length, src_len - offset);

			sub_packet = gcnew SubPacket((SUBPACKET*)packet->payload, src_len - offset);
			payload = CopyPayload(packet->payload, length);
		}

		~Packet(void)
		{
			delete payload;
			payload = nullptr;
			delete sub_packet;
			sub_packet = nullptr;
		}

		!Packet(void)
		{
			delete payload;
			payload = nullptr;
			delete sub_packet;
			sub_packet = nullptr;
		}

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

	public ref class ComPacket : public SecurityPayload
	{
	public:
		ComPacket(COM_PACKET* com_packet, size_t src_len)
			: packet(nullptr), payload(nullptr)
		{
			com_id = endian_reverse(com_packet->com_id);
			com_id_ex = endian_reverse(com_packet->com_id_ex);
			outstanding = endian_reverse(com_packet->outstanding);
			min_transfer = endian_reverse(com_packet->min_transfer);
			length = endian_reverse(com_packet->length);
			size_t offset = offsetof(COM_PACKET, payload);
			if (length + offset > src_len)
				THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%zd)", length, src_len - offset);

			PACKET* pp = (PACKET*)((BYTE*)com_packet + 20);
			packet = gcnew Packet((PACKET*)(com_packet->payload), src_len - offset);
			payload = CopyPayload(com_packet->payload, length);
		}

		~ComPacket(void)
		{
			LOG_STACK_TRACE();
			delete packet;
			packet = nullptr;
			delete payload;
			payload = nullptr;
		}

		!ComPacket(void)
		{
			LOG_STACK_TRACE();
			delete packet;
			packet = nullptr;
			delete payload;
			payload = nullptr;
		}

	public:
		WORD com_id;
		WORD com_id_ex;
		DWORD outstanding;
		DWORD min_transfer;
		DWORD length;
		JcCmdLet::BinaryType^ payload;
		Packet^ packet;
	};

	public ref class SecurityProtocolList : public SecurityPayload
	{
	public:
		SecurityProtocolList(BYTE* buf, size_t buf_len)
		{
			if (buf_len < 8) throw gcnew System::ApplicationException(L"buf length too small (<8)");
			protocol_num = MAKELONG(MAKEWORD(buf[7], buf[6]), MAKEWORD(buf[5], buf[4]));
			if (buf_len < protocol_num + 8)
				throw gcnew System::ApplicationException(L"buf length less than protocol list size");
			for (UINT ii = 0; ii < protocol_num; ++ii)
			{
				SecurityProtocol protocol_type = SecurityProtocol(buf[ii+8]);
				//switch (buf[ii + 8])
				//{
				//case 0: protocol_type = SecurityProtocol::PROTOCOL_INFO; break;
				//}
				protocol_list.Add(protocol_type);
			}
		}

	public:
		UINT protocol_num;
		System::Collections::ArrayList protocol_list;
	};



///////////////////////////////////////////////////////////////////////////////
// -- Command Let
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
			HelpMessage = "command id in NVME trace or SATA trace")]
		[ValidateNotNullOrEmpty]
		property int cmd_id;		

		[Parameter(Position = 2,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "direction of the command, true: device->host, false: host->device")]
		[ValidateNotNullOrEmpty]
		property bool receive;

		[Parameter(Position = 3,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		property DWORD CDW10;

		[Parameter(Position = 4,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "input payload in binary")]
		[ValidateNotNullOrEmpty]
		property DWORD CDW11;

	public:
		virtual void InternalProcessRecord() override
		{
			SecurityPayload^ payload = nullptr;

			BYTE protocol = 1;
			WORD comid = 0;
			if (CDW10 != 0xFFFFFFFF)
			{
				protocol = boost::numeric_cast<BYTE>(CDW10 >> 24);
				comid = boost::numeric_cast<WORD>((CDW10 & 0xFFFFFF) >> 8);
			}

			if (Payload == nullptr)
			{
				payload = gcnew SecurityPayload;
			}
			else
			{
				jcvos::auto_interface<jcvos::IBinaryBuffer> bb;
				Payload->GetData(bb);
				BYTE* buf = bb->Lock();

				if (protocol == 0)
				{
					payload = ParseProtocolInfo(comid, buf, bb->GetSize());
				}
				if (protocol == 1)
				{
					if (comid == 1 && receive)
					{
						L0Discovery^ l0discovery = gcnew L0Discovery(buf, bb->GetSize());
						payload = static_cast<SecurityPayload^>(l0discovery);
						//					WriteObject(l0discovery);
					}
					else
					{
						COM_PACKET* pp = reinterpret_cast<COM_PACKET*>(buf);
						ComPacket^ com_packet = gcnew ComPacket(pp, Payload->Length);
						//					WriteObject(static_cast<System::Object^>(com_packet));
						payload = static_cast<SecurityPayload^>(com_packet);
					}
				}
				else
				{
					WriteWarning(System::String::Format(L"Unknown protocol 0x{0:X02}", protocol));
					payload = gcnew SecurityPayload;
				}
				bb->Unlock(buf);
			}

			if (payload)
			{
				payload->protocol = protocol;
				payload->sub_protocol = comid;
				payload->cmd_id = cmd_id;
				payload->in_out = receive;
				WriteObject(payload);
			}
		}

	protected:
		SecurityPayload^ ParseProtocolInfo(WORD specific, BYTE* buf, size_t buf_len)
		{
			SecurityPayload^ payload = nullptr;
			if (specific == 0)
			{
				payload = gcnew SecurityProtocolList(buf, buf_len);
			}
			else if (specific == 0x01)
			{

			}
			else if (specific == 0x02)
			{

			}
			else
			{

			}
			return payload;
		}
	};


}
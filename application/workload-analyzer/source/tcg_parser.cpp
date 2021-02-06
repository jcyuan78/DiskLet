#include "pch.h"

#include "tcg_parser.h"
#include "l0discovery.h"
#include <boost/cast.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace prop_tree = boost::property_tree;


LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_NOTICE);
//
//void EndianSubPacket(SUBPACKET& ss)
//{
//	endian_reverse_inplace(ss.kind);
//	endian_reverse_inplace(ss.length);
//}



///////////////////////////////////////////////////////////////////////////////
// ==== Parce TCG Package

void tcg_parser::ConvertTcgPayload::InternalProcessRecord()
{
	BYTE protocol = 1;
	WORD comid = 0;
	if (CDW10 != 0xFFFFFFFF)
	{
		protocol = boost::numeric_cast<BYTE>(CDW10 >> 24);
		comid = boost::numeric_cast<WORD>((CDW10 & 0xFFFFFF) >> 8);
	}

	if (protocol == 1)
	{
		jcvos::auto_interface<jcvos::IBinaryBuffer> bb;
		Payload->GetData(bb);
		BYTE* buf = bb->Lock();
		if (comid == 1 && receive)
		{
//			L0DISCOVERY_HEADER* hh = reinterpret_cast<L0DISCOVERY_HEADER*>(buf);
			L0Discovery^ l0discovery = gcnew L0Discovery(buf, bb->GetSize());
			WriteObject(l0discovery);
		}
		else
		{
			COM_PACKET* pp = reinterpret_cast<COM_PACKET*>(buf);
			ComPacket^ com_packet = gcnew ComPacket(pp, Payload->Length);
			WriteObject(static_cast<System::Object^>(com_packet) );
		}
		bb->Unlock(buf);
	}
	else
	{
		WriteWarning( System::String::Format(L"Unknown protocol 0x{0:X02}", protocol) );
	}
}

JcCmdLet::BinaryType^ CopyPayload(void* data, size_t length)
{
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	jcvos::CreateBinaryBuffer(buf, length);
	BYTE* _buf = buf->Lock();
	memcpy_s(_buf, length, data, length);
	buf->Unlock(_buf);
	return gcnew JcCmdLet::BinaryType(buf);
}

tcg_parser::ComPacket::ComPacket(COM_PACKET* com_packet, size_t src_len)
	: packet(nullptr), payload(nullptr)
{
	com_id = endian_reverse(com_packet->com_id);
	com_id_ex = endian_reverse(com_packet->com_id_ex);
	outstanding = endian_reverse(com_packet->outstanding);
	min_transfer = endian_reverse(com_packet->min_transfer);
	length = endian_reverse(com_packet->length);
	size_t offset = offsetof(COM_PACKET, payload);
	if (length + offset > src_len)
		THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%zd)", length, src_len-offset);

	PACKET* pp = (PACKET*)((BYTE*)com_packet + 20);
	packet = gcnew Packet((PACKET*)(com_packet->payload), src_len- offset);
	payload = CopyPayload(com_packet->payload, length);
}

tcg_parser::ComPacket::~ComPacket(void)
{
	LOG_STACK_TRACE();
	delete packet;
	packet = nullptr;
	delete payload;
	payload = nullptr;
}

tcg_parser::ComPacket::!ComPacket(void)
{
	LOG_STACK_TRACE();
	delete packet;
	packet = nullptr;
	delete payload;
	payload = nullptr;
}

tcg_parser::Packet::Packet(PACKET* pp, size_t src_len)
	: payload(nullptr)
{
	session = endian_reverse(pp->session);
	seq_num = endian_reverse(pp->seq_num);
	ack_type = endian_reverse(pp->ack_type);
	acknowlede = endian_reverse(pp->acknowlede);
	length = endian_reverse(pp->length);
	size_t offset = offsetof(PACKET, payload);
	if (length + offset > src_len) 
		THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%d)", length, src_len-offset);

	sub_packet = gcnew SubPacket((SUBPACKET*)pp->payload, src_len - offset);
	payload = CopyPayload(pp->payload, length);
}

tcg_parser::Packet::~Packet(void)
{
	delete payload;
	payload = nullptr;
	delete sub_packet;
	sub_packet = nullptr;
}

tcg_parser::Packet::!Packet(void)
{
	delete payload;
	payload = nullptr;
}

tcg_parser::SubPacket::SubPacket(SUBPACKET* sub_packet, size_t src_len)
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

tcg_parser::SubPacket::~SubPacket(void)
{
	delete payload;
	payload = nullptr;
}

tcg_parser::SubPacket::!SubPacket(void)
{
	delete payload;
	payload = nullptr;
}


///////////////////////////////////////////////////////////////////////////////
// ==== Parce TCG Token

tcg_parser::ParseTcgToken::ParseTcgToken(void)
{
	Payload = nullptr;
}

tcg_parser::ParseTcgToken::~ParseTcgToken(void)
{
	//delete Payload;
}

void tcg_parser::ParseTcgToken::InternalProcessRecord()
{
//	PSObject^ token_list = gcnew PSObject;
	size_t len = Payload->Length;

//	TcgToken^ token = gcnew TcgToken;
//	CTcgTokenBase* top = token->GetToken();

	BYTE* buf = Payload->LockData();
	BYTE* ptr = buf;
	BYTE* end_buf = buf + len;
//	top->ParseToken(ptr, end_buf);
	LOG_DEBUG(L"receive = %d", receive);
	CTcgTokenBase* tt = CTcgTokenBase::SyntaxParse(ptr, end_buf, buf, /*receive.ToBool()*/ receive);
	Payload->Unlock();
	TcgToken^ token = gcnew TcgToken(tt);
	WriteObject(token);
}

tcg_parser::TcgToken::TcgToken(void) : m_token(nullptr)
{
//	m_token = new CTcgTokenBase(CTcgTokenBase::TopToken);
	m_token = static_cast<CTcgTokenBase*>(new ListToken(CTcgTokenBase::TopToken));
}

tcg_parser::TcgToken::~TcgToken(void)
{
	delete m_token;
	m_token = nullptr;
}

tcg_parser::TcgToken::!TcgToken(void)
{
	delete m_token;
	m_token = nullptr;
}

void tcg_parser::TcgToken::Print(void)
{
	m_token->Print(0);
}

//bool AnalyzeTcgProtocol(CTcgTokenBase* tt)
//{
//	tt->Begin();
//	CTcgTokenBase* cur_token = tt->Font();
//	switch (cur_token->GetType())
//	{
//		case CTcgTokenBase::
//	}
//}

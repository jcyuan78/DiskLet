#include "pch.h"

#include "tcg_parser.h"
#include <boost/property_tree/json_parser.hpp>
namespace prop_tree = boost::property_tree;


LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_DEBUGINFO);
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
	if (CDW10 != 0xFFFFFFFF)
	{
		protocol = CDW10 >> 24;
	}

	if (protocol == 1)
	{
		jcvos::auto_interface<jcvos::IBinaryBuffer> bb;
		Payload->GetData(bb);
		COM_PACKET* pp = reinterpret_cast<COM_PACKET*>(bb->Lock());
		ComPacket^ com_packet = gcnew ComPacket(pp, Payload->Length);
		WriteObject(com_packet);
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
	if (length + offsetof(COM_PACKET, payload) > src_len)
		THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%zd)", length, src_len);

	PACKET* pp = (PACKET*)((BYTE*)com_packet + 20);
//	packet = gcnew Packet(&com_packet->packet);
//	packet = gcnew Packet(pp);
	packet = gcnew Packet((PACKET*)(com_packet->payload));

//	payload = CopyPayload(&com_packet->packet, length);
//	payload = CopyPayload(pp, length);
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

tcg_parser::Packet::Packet(PACKET* pp)
	: payload(nullptr)
{
	session = endian_reverse(pp->session);
	seq_num = endian_reverse(pp->seq_num);
	ack_type = endian_reverse(pp->ack_type);
	acknowlede = endian_reverse(pp->acknowlede);
	length = endian_reverse(pp->length);

	sub_packet = gcnew SubPacket((SUBPACKET*)pp->payload);
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

tcg_parser::SubPacket::SubPacket(SUBPACKET* sub_packet)
	: payload(nullptr)
{
	kind = endian_reverse(sub_packet->kind);
	length = endian_reverse(sub_packet->length);
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
	delete Payload;
}

void tcg_parser::ParseTcgToken::InternalProcessRecord()
{
//	PSObject^ token_list = gcnew PSObject;
	size_t len = Payload->Length;

	TcgToken^ token = gcnew TcgToken;
	CTcgTokenBase* top = token->GetToken();

//	ListToken* top = new ListToken(CTcgTokenBase::TopToken);
	BYTE* buf = Payload->LockData();
	BYTE* ptr = buf;
	BYTE* end_buf = buf + len;
	top->ParseToken(ptr, end_buf);
	Payload->Unlock();
//	top->Print(stdout, 0);
//	delete top;
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
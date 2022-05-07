///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "tcg-packet.h"
#include <boost/endian.hpp>

LOCAL_LOGGER_ENABLE(L"tcg-packet", LOGGER_LEVEL_NOTICE);


void CTcgComPacket::ToString(std::wostream& out, UINT layer, int opt)
{
	if (opt & tcg::SHOW_PACKET_INFO)
	{
		out << std::showbase << std::left;
		out << L"<ComPacket>: " << std::endl;
		out << L"    " << std::setw(12) << L"ComId:" << m_com_packet.com_id << std::endl;
		out << L"    " << std::setw(12) << L"ComIdEx:" << m_com_packet.com_id_ex << std::endl;
		out << L"    " << std::setw(12) << L"OutStanding:" << m_com_packet.outstanding << std::endl;
		out << L"    " << std::setw(12) << L"MinTransfer:" << m_com_packet.min_transfer << std::endl;
		out << L"    " << std::setw(12) << L"DataLength:" << m_com_packet.length << std::endl;
		int packet_num = 0;
		for (auto pit = m_com_packet.packets.begin(); (layer > 0) && pit != m_com_packet.packets.end(); ++pit, ++packet_num)
		{
			out << L"    <Packet>[" << packet_num << L"]" << std::endl;
			out << L"        " << std::setw(12) << L"SessionId:" << pit->session << std::endl;
			out << L"        " << std::setw(12) << L"SeqNum:" << pit->seq_num << std::endl;
			out << L"        " << std::setw(12) << L"AckType:" << pit->ack_type << std::endl;
			out << L"        " << std::setw(12) << L"Acknoledge:" << pit->acknowledge << std::endl;
			out << L"        " << std::setw(12) << L"DataLength:" << pit->length << std::endl;

			//if (layer <= 1) continue;
			int subpacket_num = 0;
			for (auto sit = pit->subpackets.begin(); (layer > 1) && sit != pit->subpackets.end(); ++sit, ++subpacket_num)
			{
				out << L"        <SubPacket>[" << subpacket_num << L"]" << std::endl;
				out << L"            " << std::setw(12) << L"Kind:" << sit->kind << std::endl;
				out << L"            " << std::setw(12) << L"DataLength:" << sit->length << std::endl;
				if (sit->kind == 0x8001)
				{
					out << L"            " << std::setw(12) << L"Credit:" << sit->credit << std::endl;
				}
				out << L"        </SubPacket>" << std::endl;
			}
			out << L"    </Packt>" << std::endl;
		}
		out << L"</ComPacket>" << std::endl;
		out << std::right;
	}
	if (m_token) m_token->ToString(out, layer, opt);
}

void CTcgComPacket::GetPayload(jcvos::IBinaryBuffer*& data, int index)
{
	JCASSERT(!data);
	if (index == 0)
	{
		jcvos::CreateBinaryBuffer(data, m_com_packet.length);
		if (!data) THROW_ERROR(ERR_APP, L"failed on creating binary buffer");
		BYTE* buf = data->Lock();
		memcpy_s(buf, m_com_packet.length, m_com_packet.payload, m_com_packet.length);
		data->Unlock(buf);
	}
	else
	{
		int sub = index - 1;
		if (m_com_packet.packets.size() <= sub) return;
		PACKET_HEADER& packet = m_com_packet.packets[sub];
		jcvos::CreateBinaryBuffer(data, packet.length);
		if (!data) THROW_ERROR(ERR_APP, L"failed on creating binary buffer");
		BYTE* buf = data->Lock();
		memcpy_s(buf, packet.length, packet.payload, packet.length);
		data->Unlock(buf);
	}
}

void CTcgComPacket::GetSubItem(ISecurityObject*& sub_item, const std::wstring& name)
{
	if (name == L"token")
	{
		sub_item = m_token;
		if (sub_item) sub_item->AddRef();
	}
}

#define MAX_TOKEN_SIZE	(1024*4)

bool CTcgComPacket::ParseData(const BYTE* buf, size_t buf_len, bool receive)
{
	// copy data
	m_data.reset(new BYTE[buf_len]);
	memcpy_s(m_data.get(), buf_len, buf, buf_len);
	m_com_packet.SetByRawData(m_data.get(), buf_len);

	// parse tcg token
	m_token_data.reset(new BYTE[buf_len]);		// 4KB
	BYTE* data = m_token_data.get();
	m_token_len = 0;

	for (auto pit = m_com_packet.packets.begin(); pit != m_com_packet.packets.end(); ++pit)
	{
		int subpacket_num = 0;
		for (auto sit = pit->subpackets.begin(); sit != pit->subpackets.end(); ++sit)
		{
			if (sit->kind == SUBPACKET_KIND_DATA)
			{
				if (m_token_len + sit->length >= buf_len) THROW_ERROR(ERR_APP, L"token data too long");
				memcpy_s(data + m_token_len, (buf_len - m_token_len), sit->payload, sit->length);
				m_token_len += sit->length;
			}
		}
	}
	m_token = CTcgTokenBase::SyntaxParse(data, data + m_token_len, data, receive);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../include/itcg.h"
#include "../include/tcg_token.h"
//#define BOOST_EDIAN_DEPRECATED_NAMES
#include <boost/endian.hpp>

#define SUBPACKET_KIND_DATA		(0)
#define SUBPACKET_KIND_CREDIT	(0x8001)

class RAW_SUBPACKET
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

class SUBPACKET_HEADER
{
public:
	WORD kind;
	DWORD length;
	union
	{
		BYTE* payload;
		DWORD credit;
	};
	void SetByRawData(BYTE*& buf, size_t buf_len)
	{
		RAW_SUBPACKET* raw = reinterpret_cast<RAW_SUBPACKET*>(buf);

		kind = boost::endian::big_to_native(raw->kind);
		length = boost::endian::big_to_native(raw->length);
		size_t offset = offsetof(RAW_SUBPACKET, payload);
		if (length + offset > buf_len)
			THROW_ERROR(ERR_APP, L"len feild (%d) is larger than source size (%d)", length, buf_len - offset);
		if (kind == 0x8001)
		{
			credit = raw->credit;
		}
		else
		{
			payload = buf + offset;
		}
		buf += offset + round_up<DWORD>(length, 4);
	}
};

class RAW_PACKET
{
public:
	UINT64 session;
	DWORD seq_num;
	WORD reserved;
	WORD ack_type;
	DWORD acknowledge;
	DWORD length;
	BYTE payload[1];
};

class PACKET_HEADER
{
public:
	UINT64 session;
	DWORD seq_num;
	WORD ack_type;
	DWORD acknowledge;
	DWORD length;
	std::vector<SUBPACKET_HEADER> subpackets;
	BYTE* payload;
	BYTE* offset_ptr;

public:
	void SetByRawData(BYTE*& buf, size_t buf_len)
	{
		RAW_PACKET* raw = reinterpret_cast<RAW_PACKET*>(buf);
		offset_ptr = buf;
		BYTE * end_buf = buf + buf_len;

		session = boost::endian::big_to_native(raw->session);
		seq_num = boost::endian::big_to_native(raw->seq_num);
		ack_type = boost::endian::big_to_native(raw->ack_type);
		acknowledge = boost::endian::big_to_native(raw->acknowledge);
		length = boost::endian::big_to_native(raw->length);
		size_t offset = offsetof(RAW_PACKET, payload);
		if (length + offset > buf_len)
			THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%d)", length, buf_len - offset);
		buf += offset;
		payload = buf;
		while (buf < end_buf)
		{
			subpackets.emplace_back();
			SUBPACKET_HEADER& subpacket = subpackets.back();
			subpacket.SetByRawData(buf, length);
		}
		//sub_packet = gcnew SubPacket((SUBPACKET*)raw->payload, src_len - offset);
		//payload = CopyPayload(packet->payload, length);

	}

//	BYTE payload[1];
};

class RAW_COM_PACKET
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

class COM_PACKET_HEADER
{
public:
	WORD com_id;
	WORD com_id_ex;
	DWORD outstanding;
	DWORD min_transfer;
	DWORD length;
	std::vector<PACKET_HEADER> packets;
	BYTE* payload;
	BYTE* offset_ptr;	// ç›command payload bufferíÜìIoffset
//	BYTE payload[1];

public:
	void SetByRawData(BYTE* buf, size_t src_len)
	{
		RAW_COM_PACKET* raw = reinterpret_cast<RAW_COM_PACKET*>(buf);
		offset_ptr = buf;

		com_id = boost::endian::big_to_native(raw->com_id);
		com_id_ex = boost::endian::big_to_native(raw->com_id_ex);
		outstanding = boost::endian::big_to_native(raw->outstanding);
		min_transfer = boost::endian::big_to_native(raw->min_transfer);
		length = boost::endian::big_to_native(raw->length);
		size_t offset = offsetof(RAW_COM_PACKET, payload);

		if (length + offset > src_len)
			THROW_ERROR(ERR_APP, L"len field (%d) is larger than source size (%zd)", length, src_len - offset);
		payload = (BYTE*)(raw) + offset;

		BYTE* ptr = payload;
		BYTE* end_ptr = ptr + length;
		while (ptr < end_ptr)
		{
			packets.emplace_back();
			PACKET_HEADER& packet_header = packets.back();
			packet_header.SetByRawData(ptr, length);
		}
	}
};

class CTcgComPacket : public tcg::ISecurityObject
{
public:
	CTcgComPacket(void) : m_token(NULL) {}
	virtual ~CTcgComPacket(void) { RELEASE(m_token); }
public:
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop) { m_token->ToProperty(prop); };
	virtual void GetPayload(jcvos::IBinaryBuffer*& data, int index);
	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name);

public:
	bool ParseData(const BYTE* buf, size_t buf_len, bool receive);

protected:
	COM_PACKET_HEADER m_com_packet;
	CTcgTokenBase* m_token;
	bool m_receive;
	//jcvos::CStaticInstance<CTcgPacket> m_packet;

	std::shared_ptr<BYTE> m_data;
	BYTE*	m_offset;
	size_t	m_data_len;

	std::shared_ptr<BYTE> m_token_data;
	size_t	m_token_len;
};
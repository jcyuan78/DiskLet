///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "../include/protocol-analyzer.h"
#include "../include/utility.h"
#include <boost/endian.hpp>

#import "../PEAutomation.tlb" no_namespace named_guids

LOCAL_LOGGER_ENABLE(L"vss.app", LOGGER_LEVEL_DEBUGINFO);

static bool g_init = false;




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== CPacket ====

class TLPHeader
{
public:
	BYTE fmt;
	BYTE tcr;
	WORD attr_len;
	union
	{
		struct
		{
			WORD request_id;
			BYTE tag;
			BYTE be;
			DWORD add_hi;
			DWORD add_lo;
		} mem_rw;
		struct
		{
			WORD completer_id;
			WORD status_count;
			WORD request_id;
			BYTE tag;
			BYTE low_addr;
		} cpl;
	};
};



class CPacket : public pa::IPacket
{
public:
	CPacket(void);
	~CPacket(void) { if (m_packet) m_packet->Release(); delete[] m_raw_data; }
	void Initialize(IPEPacket1* packet, long long index)
	{
		m_packet = packet;
		m_packet_index = index;
		if (packet)
		{
			packet->AddRef();
			ParsePacket();
		}

	}
	void ParsePacket(IPEPacket1* packet=NULL);
	void ParseTlpMemoryReadWrite(BYTE fmt, TLPHeader * header);
	void ParseTlpCompletion(BYTE fmt, TLPHeader* header);
	const BYTE* GetPayloadPtr(void) const { return m_payload; }

public:
	virtual pa::PACKET_TYPE GetPacketType(void) const { return m_packet_type; }
	virtual pa::TLP_TYPE GetTLPType(void) const { return m_tlp_type; }
	virtual DWORD GetRequestId(void) const { return m_tlp_req_tag; }
	virtual UINT64 GetAddress(void) const { return m_addr; }
	virtual size_t GetPayloadLen(void) const { return m_data_len; }
	virtual WORD GetRemainByte(void) const { return m_remain_byte; }
	virtual void GetPacketData(jcvos::IBinaryBuffer*& buf);

protected:
	// 返回raw data大小
	size_t GetRawData(void);

protected:
	IPEPacket1* m_packet;
	long long	m_packet_index;
	pa::PACKET_TYPE m_packet_type;
//-- raw data
	TLPHeader*	m_tlp_header;
	BYTE*		m_raw_data;
	size_t		m_raw_len;
	WORD		m_start_word;
	long		m_link_width;

//-- decoded payload
	UINT64 m_addr;
	BYTE* m_payload;
	size_t m_data_len;
//	size_t m_offset;				// 通过completion的remaining byte count，计算得到的offset

//-- decoded TLP info
	WORD m_tlp_seq;
	pa::TLP_TYPE m_tlp_type;
	BYTE m_tlp_tag;
	DWORD m_tlp_req_tag;		// 合并request ID 和 tag
	WORD m_remain_byte;
};

CPacket::CPacket(void)
	: m_packet(NULL), m_raw_data(NULL), m_raw_len(0), m_packet_type(pa::PACKET_TYPE_UNKNOWN)
	, m_addr(0)
{
}

size_t CPacket::GetRawData(void)
{
	if (m_raw_data)
	{
		delete[] m_raw_data;
		m_raw_data = 0;
	}
	m_raw_len = 0;

	HRESULT hres;
	VARIANT packet_data;
	VariantInit(&packet_data);
	m_raw_len = m_packet->GetPacketData(PACKETFORMAT_BYTES, &packet_data);
	if (packet_data.vt == (VT_ARRAY | VT_VARIANT))
	{
		SAFEARRAY* psa = packet_data.parray;
		size_t size = psa->rgsabound[0].cElements;
		if (m_raw_len != size) THROW_ERROR(ERR_APP, L"data length not match, elements=%lld, len=%lld", size, m_raw_len);
		m_raw_data = new BYTE[m_raw_len];

		VARIANT HUGEP* src;
		hres = SafeArrayAccessData(psa, (void HUGEP**) & src);
		for (size_t ii = 0; ii < m_raw_len; ++ii) { m_raw_data[ii] = V_UI1(src + ii); }
		hres = SafeArrayUnaccessData(psa);
	}
	VariantClear(&packet_data);
	return m_raw_len;
}

void CPacket::GetPacketData(jcvos::IBinaryBuffer*& buf)
{
	JCASSERT(m_packet && (buf == NULL));
	size_t len = GetRawData();
	if (len > 0)
	{
		jcvos::CreateBinaryBuffer(buf, len);
		BYTE * dst = buf->Lock();
		memcpy_s(dst, len, m_raw_data, m_raw_len);
		buf->Unlock(dst);
	}
}

void CPacket::ParsePacket(IPEPacket1 * packet)
{
	JCASSERT(m_packet);
	double timestamp = m_packet->GetTimestamp();
	size_t len = GetRawData();
	if (len == 0) THROW_ERROR(ERR_APP, L"failed on getting packet raw data");
	WORD w0 = MAKEWORD(m_raw_data[0], m_raw_data[1]);
	WORD w1 = MAKEWORD(m_raw_data[3], m_raw_data[2]);
	LOG_DEBUG_(1,L"w0=%04X, w1=%04X", w0, w1);
	m_link_width = m_packet->GetLinkWidth();
	if ( (w0 & 0xF) == 0xF)
	{	// TLP
		LOG_DEBUG_(1, L"TLP, STP=%X, len=%d, CRC=%X, seq=%d", w0 & 0xF, (w0 >> 4) & 0x7FF, (w1 >> 12), (w1 & 0xFFF));
		m_packet_type = pa::PACKET_TYPE_TLP;
		m_tlp_seq = w1 & 0xFF;
		m_tlp_header = (TLPHeader*)(m_raw_data + 4);

		BYTE fmt = (m_tlp_header->fmt >> 5) & 0x07;
		BYTE tlp_type = (m_tlp_header->fmt & 0x1F);
		m_tlp_type = pa::TLP_UNKOWN;
		if (tlp_type == 0)			ParseTlpMemoryReadWrite(fmt, m_tlp_header);
		else if (tlp_type == 0xA)	ParseTlpCompletion(fmt, m_tlp_header);
	}
	else if (w0 == 0xACF0)
	{
		LOG_DEBUG_(1, L"DLLP");
	}
}

void CPacket::ParseTlpMemoryReadWrite(BYTE fmt, TLPHeader * header)
{
	m_addr = boost::endian::big_to_native(header->mem_rw.add_hi);
	if (fmt == 3 || fmt == 1)
	{	// 64 bit address
		m_addr <<= 32;
		m_addr |= boost::endian::big_to_native(header->mem_rw.add_lo);
	}
	if (fmt >= 0x2)
	{	// 3DW header with data
		m_tlp_type = pa::TLP_MEMORY_WRITE;
		if (fmt == 0x3)		m_payload = m_raw_data + 20;
		else				m_payload = m_raw_data + 16;
	}
	else
	{
		m_tlp_type = pa::TLP_MEMORY_READ;
		m_payload = NULL;
	}
	WORD ll = boost::endian::big_to_native(header->attr_len) & 0x3FF;
	m_data_len = (ll == 0) ? (4096) : (ll * 4);	// DW to byte
	m_tlp_tag = header->mem_rw.tag;
	m_tlp_req_tag = (((DWORD)header->mem_rw.request_id) << 8) | header->mem_rw.tag;
	m_remain_byte = (WORD)m_data_len;
//	m_offset = 0;
}

void CPacket::ParseTlpCompletion(BYTE fmt, TLPHeader* header)
{
	if (fmt == 2)	m_tlp_type = pa::TLP_COMPLETION_DATA;
	else			m_tlp_type = pa::TLP_COMPLETION;
	
	WORD ll = boost::endian::big_to_native(header->attr_len) & 0x3FF;
	m_data_len = (ll == 0) ? (4096) : (ll * 4);	// DW to byte
	m_tlp_tag = header->cpl.tag;
	m_tlp_req_tag = (((DWORD)header->cpl.request_id) << 8) | header->cpl.tag;
	m_payload = m_raw_data + 16;
	m_remain_byte = boost::endian::big_to_native(header->cpl.status_count) & 0xFFF;
	//if (byte_count > m_data_len)
	//{
	//	THROW_ERROR(ERR_APP, L"remaining bytes (%d) is large than data len (%d)", byte_count, m_data_len);
	//}
	//m_offset = m_data_len - byte_count;

//	if (byte_count != m_data_len) LOG_WARNING(L"byte count not match, data len=%d, byte count=%d", m_data_len, byte_count);
//	{
//		THROW_ERROR(ERR_APP, L"byte cont does not match, data len=%d, byte count=%d", m_data_len, byte_count);
//	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== CTrace ====


class CTrace : public pa::ITrace
{
public:
	CTrace(void) : m_trace(NULL), m_analyzer(NULL), m_packet_count(0) {}
	~CTrace(void);
	void Initialize(IPETraceForScript16* trace, IPEAnalyzer* pe);

public:
	virtual void Close(void);
	virtual void GetPacket(pa::IPacket * & p, UINT64 index);
	virtual void ExportTraceToCsv(const std::wstring& out_fn, int level);
	virtual void GetCommandPayload(jcvos::IBinaryBuffer* & buf, double start_time, UINT64 start_addr, 
		UINT data_len, bool receive);
protected:
	size_t GetReadPayload(BYTE * buf, long long start_index, UINT64 start_addr, UINT data_len);
	size_t GetWritePayload(BYTE * buf, long long start_index, UINT64 start_addr, UINT data_len);
	CPacket* InternalGetPacket(long long index);

protected:
	IPETraceForScript16* m_trace;
	IPEAnalyzer* m_analyzer;
	long long m_packet_count;
};

CTrace::~CTrace(void)
{
	if (m_trace)
	{
		m_trace->Close();
		m_trace->Release();
	}
	if (m_analyzer) m_analyzer->Release();
}


void CTrace::Initialize(IPETraceForScript16* trace, IPEAnalyzer* pe)
{
	if (trace == NULL) THROW_ERROR(ERR_APP, L"trace is NULL");
	m_trace = trace;
	m_trace->AddRef();
	m_packet_count = m_trace->GetPacketsCountEx();

	if (pe)
	{
		m_analyzer = pe;
		m_analyzer->AddRef();
	}
}

size_t CTrace::GetReadPayload(BYTE * out_buf, long long start_index, UINT64 start_addr, UINT data_len)
{
	JCASSERT(out_buf);
	long long index = start_index;

	UINT64 end_addr = start_addr + data_len;
	int max_check_packets = 1000;	//保护
	size_t remain = data_len;
	size_t read = 0;

	struct READ_REQUEST
	{
		UINT64	addr;
		DWORD	req_id;
		WORD	len;
		WORD	filled;	
	};

//	std::map<DWORD, READ_REQUEST> requests;
	// 以tag为索引，req_id=0为空，否则填入
	jcvos::auto_array<READ_REQUEST> requests(256, 0);

	while (max_check_packets > 0 && index <m_packet_count)
	{
		jcvos::auto_interface<CPacket> _packet(InternalGetPacket(index++));
		CPacket* packet = (CPacket*)_packet;
		if (packet == NULL) THROW_ERROR(ERR_APP, L"Failed on getting packet %lld", index - 1);
		max_check_packets--;
		//packet->ParsePacket();
		if (packet->GetPacketType() != pa::PACKET_TYPE_TLP)
		{
			LOG_DEBUG(L"packet %lld is not TLP", index - 1);
			continue;
		}
		UINT64 addr = packet->GetAddress();
		pa::TLP_TYPE type = packet->GetTLPType();
		if (type == pa::TLP_MEMORY_READ && addr>=start_addr && addr<end_addr)
		{	// 处理read request
			DWORD req_id = packet->GetRequestId();
			BYTE tag = (BYTE)(req_id & 0xFF);
			READ_REQUEST& req = requests[tag];
			if (req.req_id != 0)
			{
				THROW_ERROR(ERR_APP, L"req_id 0x%X has exist", req_id);
			}
//			READ_REQUEST req;
			req.addr = addr;
			req.len = (WORD)packet->GetPayloadLen();
			req.filled = req.len;
			req.req_id = req_id;
//			req.req_id = packet->GetRequestId();
//			auto it = requests.insert(std::make_pair(req.req_id, req));
//			BYTE tag = (BYTE)(req.req_id & 0xFF);
			LOG_DEBUG(L"Mrd: id=%X, addr=%llX, len=%d", req.req_id, req.addr, req.len);
//			if (!it.second) THROW_ERROR(ERR_APP, L"req_id 0x%X has exist", req.req_id);
		}
		else if (type == pa::TLP_COMPLETION_DATA)
		{
			DWORD req_id = packet->GetRequestId();
			BYTE tag = (BYTE)(req_id & 0xFF);
			READ_REQUEST& req = requests[tag];
			if (req.req_id != req_id)
			{
				THROW_ERROR(ERR_APP, L"req_id 0x%X does not match, exist req_id=0x%X", req_id, req.req_id);
			}

			size_t payload_len = packet->GetPayloadLen();
			WORD remain_byte = packet->GetRemainByte();
			LOG_DEBUG(L"CplD: id=%X, len=%d, remain=%d", req_id, (DWORD)(payload_len), remain_byte);

			//auto req = requests.find(req_id);
			//if (req == requests.end()) THROW_ERROR(ERR_APP, L"req_id 0x%X does not match", req_id);

			const BYTE* payload_ptr = packet->GetPayloadPtr();
			if (remain_byte > req.len)
			{
				THROW_ERROR(ERR_APP, L"remain (%d) is large than data len (%d)", remain_byte, req.len);
			}
			// 在一个read请求内，不同的cpl的offset
			WORD local_offset = req.len - remain_byte;

			UINT64 offset = (req.addr - start_addr) + local_offset;
			memcpy_s(out_buf + offset, data_len - offset, payload_ptr, payload_len);
			req.filled -= (WORD)payload_len;
			if (req.filled == 0)
			{	// 对于已经完成的request，tag可能被复用，删除request
				req.req_id = 0;
			}

			read += payload_len;
			if (remain <= payload_len) break;
			remain -= payload_len;
		}
	}
	return read;
}

size_t CTrace::GetWritePayload(BYTE *out_buf, long long start_index, UINT64 start_addr, UINT data_len)
{
	JCASSERT(out_buf);
	long long index = start_index;

	UINT64 end_addr = start_addr + data_len;
	int max_check_packets = 1000;	//保护
	size_t remain = data_len;
	size_t written = 0;
	while (max_check_packets > 0 && index < m_packet_count)
	{
		LOG_DEBUG(L"get packeg %lld", index);
		jcvos::auto_interface<CPacket> _packet(InternalGetPacket(index++));
		CPacket* packet = (CPacket*)_packet;
		if (packet == NULL) THROW_ERROR(ERR_APP, L"Failed on getting packet %lld", index-1);
		max_check_packets--;
		//packet->ParsePacket();

		if (packet->GetPacketType() != pa::PACKET_TYPE_TLP || packet->GetTLPType() != pa::TLP_MEMORY_WRITE)
		{
			LOG_DEBUG(L"packet %lld is not Memory Write", index - 1);
			continue;
		}
		UINT64 addr = packet->GetAddress();
		if (addr < start_addr || addr >= end_addr)
		{
			LOG_DEBUG(L"packet address=0x%0llX is out of range", addr);
			continue;
		}
		size_t payload_len = packet->GetPayloadLen();
		LOG_DEBUG(L"got payload %d bytes, from 0x%0llX", payload_len, addr);
		const BYTE* payload_ptr = packet->GetPayloadPtr();
		UINT64 offset = addr - start_addr;
		memcpy_s(out_buf + offset, data_len - offset, payload_ptr, payload_len);
		written += payload_len;

		if (remain <= payload_len) break;
		remain -= payload_len;
	}
	return written;
}

CPacket* CTrace::InternalGetPacket(long long index)
{
	IDispatch* pp = m_trace->GetBusPacketEx(index).Detach();
	if (!pp) THROW_ERROR(ERR_APP, L"failed on getting bus package, index=%lld", index);
	auto_unknown<IPEPacket1> packet1;
	HRESULT hres = pp->QueryInterface<IPEPacket1>(&packet1);
	pp->Release();
	if (FAILED(hres) || packet1 == NULL) THROW_COM_ERROR(hres, L"failed on getting packet interface");

	CPacket* _packet = jcvos::CDynamicInstance<CPacket>::Create();
	_packet->Initialize(packet1, index);
	return _packet;
}

void CTrace::Close(void)
{
	if (m_trace)
	{
		m_trace->Close();
		m_trace->Release();
		m_trace = NULL;
	}
}

void CTrace::GetPacket(pa::IPacket*& p, UINT64 index)
{
	JCASSERT(p == NULL);
	p = static_cast<pa::IPacket*>(InternalGetPacket(index));
}

void CTrace::GetCommandPayload(jcvos::IBinaryBuffer* & buf, double start_time, UINT64 start_addr, UINT data_len, bool receive)
{
	JCASSERT(m_trace && buf == NULL);
	int level = 0;
	long long index = 0;
	HRESULT hres = m_trace->GetPacketByTime(start_time, 1, &level, &index);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting packet by time, ts=%0.01 ns", start_time);

	// 像DWORD对齐
	data_len = round_up(data_len, (UINT)4);
	jcvos::CreateBinaryBuffer(buf, data_len);
	BYTE* out_buf = buf->Lock();

	// security receive命令：device 写入结果到memory, write payload
	if (receive)	GetWritePayload(out_buf, index, start_addr, data_len);
	else			GetReadPayload(out_buf, index, start_addr, data_len);

	buf->Unlock(out_buf);
}

void CTrace::ExportTraceToCsv(const std::wstring& out_fn, int level_in)
{
//	EOutputFileFormat format = OUTPUT_FORMAT_CSV;
	EOutputFileFormat format = OUTPUT_FORMAT_TEXT;

	EPETraceDecodeLevels level = (EPETraceDecodeLevels)((int)(level_in));
	HRESULT hres = m_trace->ExportTrace(format, out_fn.c_str(), -1, -1, level);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on exporting trace");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== CAnalyzer ====


class CAnalyzer : public pa::IAnalyzer
{
public:
	CAnalyzer(void) /*: m_analyzer(NULL) */{}
	~CAnalyzer(void) { /*if (m_analyzer) m_analyzer->Release();*/ }
public:
	virtual void OpenTrace(pa::ITrace*& trace, const std::wstring& fn)
	{
		JCASSERT(trace == NULL)
		IDispatch* tt = NULL;
		tt = m_analyzer->OpenFile(fn.c_str()).Detach();
		if (!tt) THROW_ERROR(ERR_APP, L"failed on open file %s", fn.c_str());
		auto_unknown<IPETraceForScript16> new_trace;
		HRESULT hres = tt->QueryInterface<IPETraceForScript16>(&new_trace);
		tt->Release();
		if (FAILED(hres) || new_trace == NULL) THROW_ERROR(ERR_APP, L"failed on converitng dispatch to trace");

		CTrace* _trace = jcvos::CDynamicInstance<CTrace>::Create();
		_trace->Initialize(new_trace, m_analyzer);
		trace = static_cast<pa::ITrace*>(_trace);
	}
public:
	static IPEAnalyzer * m_analyzer;
	

};

IPEAnalyzer* CAnalyzer::m_analyzer = NULL;

void pa::CreateAnalyzer(IAnalyzer*& aa, bool dll)
{
	JCASSERT(aa == NULL);
	CAnalyzer* an = jcvos::CDynamicInstance<CAnalyzer>::Create();
	if (!an->m_analyzer)
	{
		HRESULT hres;
		if (!dll)
		{
			// init comm 
			hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
			if (hres != S_OK) THROW_COM_ERROR(hres, L"Failed on initialize COM");

			hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL, EOAC_NONE, NULL);
			if (FAILED(hres)) THROW_COM_ERROR(hres, L"Failed on initialize security");

			g_init = true;
		}
		hres = CoCreateInstance(CLSID_PEAnalyzer, NULL, CLSCTX_SERVER, IID_IPEAnalyzer, (void**)(&an->m_analyzer));
		if (FAILED(hres) || an->m_analyzer == NULL) THROW_COM_ERROR(hres, L"failed on creating pe analyzer");



	//auto_unknown<IPEAnalyzer> analyzer;
	//hres = CoCreateInstance(CLSID_PEAnalyzer, NULL, CLSCTX_SERVER, IID_IPEAnalyzer, analyzer);
	}
	aa = static_cast<IAnalyzer*>(an);
}

void pa::DestoryAnalyzer(bool dll)
{
	CAnalyzer* an = jcvos::CDynamicInstance<CAnalyzer>::Create();
	if (an->m_analyzer)
	{
		an->m_analyzer->Release();
		an->m_analyzer = NULL;
		if (!dll)
		{
			CoUninitialize();
		}
	}
	
}

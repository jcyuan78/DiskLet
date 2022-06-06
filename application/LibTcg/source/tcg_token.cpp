///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "../include/tcg_token.h"
//#include "tcg_parser.h"
#include <boost/cast.hpp>

LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_DEBUGINFO);

static const wchar_t* _INDENTATION = L"                              ";
static const wchar_t* INDENTATION = _INDENTATION + wcslen(_INDENTATION);

static CUidMap * g_uid_map = NULL;

#define CONSUME_TOKEN(b, e, s, t) { \
		if (b>=e) THROW_ERROR(ERR_APP, L"unexpected end of streaming");	\
		if (*b != t) THROW_ERROR(ERR_APP, L"expected " #t " token, offset=%zd, val=0x%02X",\
		(b - s), *s);	b++; }

#define NEW_LINE_SUB	(10)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ==== CTcgTokenBase ======
#if 1
void CTcgTokenBase::SetUidMap(CUidMap* map)
{
	g_uid_map = map;
}

CTcgTokenBase* CTcgTokenBase::Parse(BYTE*& begin, BYTE* end, BYTE * start)
{
	CTcgTokenBase* token = nullptr;
	BYTE token_code = *begin;
	if (token_code <= 0xE3)
	{
		token = static_cast<CTcgTokenBase*>(MidAtomToken::ParseAtomToken(begin, end, start) );
		//return token;
	}
	else if (token_code == START_LIST)
	{
		//			token = static_cast<CTcgTokenBase*>(new ListToken(CTcgTokenBase::List));
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<ListToken>::Create(CTcgTokenBase::List));
		token->ParseToken(begin, end, start);
		//CONSUME_TOKEN(begin, end, start, END_LIST);
	}
	else if (token_code == START_NAME)
	{
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<NameToken>::Create());
		token->ParseToken(begin, end, start);
	}
	else if (token_code == TOKEN_CALL)
	{
		CallToken* tt = jcvos::CDynamicInstance<CallToken>::Create();
		tt->ParseToken(begin, end, start);
		token = static_cast<CTcgTokenBase*>(tt);
	}
	else if (token_code == END_OF_SESSION)
	{
		token = jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::EndOfSession);
		begin++;
	}
	else if (token_code == START_TRANSACTION || token_code == END_TRANSACTION)
	{
		THROW_ERROR(ERR_APP, L"not support for transaction, offset=%zd", (begin -start));
	}
	else if (token_code == EMPTY_TOKEN)
	{
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::Empty));
		begin++;
	}
	else
	{
		THROW_ERROR(ERR_APP, L"unknown or reserved token: 0x%02X, offset=%zd", *begin, begin - start);
	}

/*

	switch (*begin)
	{
	case 0xF1:	// END_LIST;
		THROW_ERROR(ERR_APP, L"unexpected end list token, offset=%zd", (begin - start));
		break;
	case 0xF2:	// START_NAME;
//		token = static_cast<CTcgTokenBase*>(new NameToken);
		break;
	case 0xF3:	// END_NAME;
		THROW_ERROR(ERR_APP, L"unexpected end name token, offset=%zd", (begin - start));
		break;
	case 0xF8:	// CALL;
//		token = new CTcgTokenBase(Call);
		begin++;
		break;

	case 0xF9:	// END_OF_DATA;
//		token = new CTcgTokenBase(EndOfData);
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::EndOfData));
		begin++;
		break;
	case 0xFA:	// END_OF_SECTION;
//		token = new CTcgTokenBase(EndOfSession);
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::EndOfSession));
		begin++;
		break;
	case 0xFB:	// START_TRANSACTION;
	case 0xFC:	// END_TRANSACTION;
		THROW_ERROR(ERR_APP, L"not support for transaction, offset=%zd", (begin - start));
		break;
	case 0xFF:	// EMPTY;
//		token = new CTcgTokenBase(CTcgTokenBase::Empty);
		token = static_cast<CTcgTokenBase*>(jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::Empty));
		begin++;
		break;

	default:
		THROW_ERROR(ERR_APP, L"unknown or reserved token: 0x%02X, offset=%zd", *begin, begin - start);
		break;
	}
	}
*/
	return token;
}
#endif

CTcgTokenBase* CTcgTokenBase::SyntaxParse(BYTE*& begin, BYTE* end, BYTE* start, bool receive)
{
	bool br;
	CTcgTokenBase* token = nullptr;
	switch (*begin)
	{
	case TOKEN_CALL: {	// CALL
		CallToken* tt = jcvos::CDynamicInstance<CallToken>::Create();
		br = tt->ParseToken(begin, end, start);
		token = static_cast<CTcgTokenBase*>(tt);
		break; }

	case START_LIST: {
		if (!receive) THROW_ERROR(ERR_APP, L"illeagle return value in send command");
		ListToken* tt = jcvos::CDynamicInstance<ListToken>::Create(CTcgTokenBase::List);
		tt->ParseToken(begin, end, start);
		CONSUME_TOKEN(begin, end, start, END_OF_DATA);
		CStatePhrase* ss = jcvos::CDynamicInstance<CStatePhrase>::Create();
		ss->ParseToken(begin, end, start);
		tt->AddToken(ss);
		token = static_cast<CTcgTokenBase*>(tt);
		break; }

	case END_OF_SESSION:
//		if (receive) THROW_ERROR(ERR_APP, L"illeagle end of session in receive command");
		token = jcvos::CDynamicInstance<CTcgTokenBase>::Create(CTcgTokenBase::EndOfSession);
		begin++;
		break;
	}
	return token;
}

bool CTcgTokenBase::ParseToken(BYTE*& begin, BYTE* end, BYTE* start)
{
	m_payload_begin = begin;
	bool br = InternalParse(begin, end);
	m_payload_len = begin - m_payload_begin;
	return br;
}


void CTcgTokenBase::Print(int indetation)
{
	Print(stdout, indetation);
}

void CTcgTokenBase::Print(FILE* ff, int indentation)
{
	switch (m_type)
	{
	case Call:			fwprintf_s(ff, L"%s<CALL >\n", INDENTATION - indentation);			break;
	case Transaction:	fwprintf_s(ff, L"%s<TRANS>\n", INDENTATION - indentation);			break;
	case EndOfData:		fwprintf_s(ff, L"%s<END OF DATA>\n", INDENTATION - indentation);	break;
	case EndOfSession:	fwprintf_s(ff, L"%s<END OF SESSION>\n", INDENTATION - indentation);	break;
	case Empty:			fwprintf_s(ff, L"%s<EMPTY>\n", INDENTATION - indentation);			break;
	case TopToken:		fwprintf_s(ff, L"%s<START>\n", INDENTATION - indentation);			break;
	case UnknownToken:	fwprintf_s(ff, L"%s<UNKWO>\n", INDENTATION - indentation);			break;
	default:			JCASSERT(0);		break;
	}
}

void CTcgTokenBase::ToString(std::wostream& out, UINT layer, int opt)
{
	int indentation = HIBYTE(HIWORD(opt));
	switch (m_type)
	{
	case Call:			out << (INDENTATION - indentation) << L"<CALL>"				<< std::endl;	break;
	case Transaction:	out << (INDENTATION - indentation) << L"<TRANS>"			<< std::endl;	break;
	case EndOfData:		out << (INDENTATION - indentation) << L"<END_OF_DATA/>"		<< std::endl;	break;
	case EndOfSession:	out << (INDENTATION - indentation) << L"<END_OF_SESSION/>"	<< std::endl;	break;					    
	case Empty:			out << (INDENTATION - indentation) << L"<EMPTY/>"			<< std::endl;	break;
	case TopToken:		out << (INDENTATION - indentation) << L"<START>"			<< std::endl;	break;
	case UnknownToken:	out << (INDENTATION - indentation) << L"<UNKWO>"			<< std::endl;	break;
	default:			JCASSERT(0);		break;
	}
}

void CTcgTokenBase::GetPayload(jcvos::IBinaryBuffer*& data, int index)
{
	if (m_payload_begin && m_payload_len)
	{
		jcvos::CreateBinaryBuffer(data, m_payload_len);
		BYTE* buf = data->Lock();
		memcpy_s(buf, m_payload_len, m_payload_begin, m_payload_len);
		data->Unlock(buf);
	}
}


///////////////////////////////////////////////////////////////////////////////
// ==== Atom Tokens ======

bool MidAtomToken::InternalParse(BYTE*& begin, BYTE* end)
{	// 解析所有长度大于 8 字节的atom token。
	// 如果长度小于等于8， 返回后转换成小token。
	bool binary = false;
	m_signe = false;
	BYTE token = begin[0];
	//m_data = NULL;

	DWORD val_len;	//atom中数值的长度

	if ( (token & 0x80) == 0 )
	{
		val_len = 0;
		m_len = 1;
		m_data_val[0] = token & 0x3F;
//		m_val = token & 0x3F;
		begin++;
	}
	else if ((token & 0xC0) == 0x80)
	{
		LOG_DEBUG(L"parsing short atom token");
		binary = ((token & 0x20) != 0);
		m_signe = ((token & 0x10) != 0);
		val_len = (token) & 0x0F;
		m_len = val_len;
		begin++;
	}
	else if ((token & 0xE0) == 0xC0)
	{
		LOG_DEBUG(L"parsing medium atom token");
		binary = ((token & 0x10) != 0);
		m_signe = ((token & 0x08) != 0);
		size_t hi = token & 0x07;
		val_len = MAKEWORD(begin[1], hi);
		m_len = val_len;
		begin += 2;
	}
	else if ((token & 0xF0) == 0xE0)
	{
		LOG_DEBUG(L"parsing long atom token");
		binary = ((token & 0x02) != 0);
		m_signe = ((token & 0x01) != 0);
		val_len = MAKELONG(MAKEWORD(begin[3], begin[2]), begin[1]);
		m_len = val_len;
		begin += 4;
	}
	else
	{
		THROW_ERROR(ERR_APP, L"unexpected atom token, token=0x%02X, offset=%d", token, begin - m_payload_begin);
	}

	//	if (binary == 0) THROW_ERROR(ERR_APP, L"unsupport integer atom, token=%02X", token);
	if (!binary)	m_type = CTcgTokenBase::IntegerAtom;
	else			m_type = CTcgTokenBase::BinaryAtom;

//	if (signe_int == 1) THROW_ERROR(ERR_APP, L"unspport continue = 1");

	if (val_len == 0) {}
	//else if (val_len <= 8)
	//{
	//	BYTE* dd = (BYTE*)(&m_val);
	//	for (size_t ii = 0; ii < val_len; ++ii, ++begin)
	//	{
	//		if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of data, offset = %zd", (begin-m_payload_begin) );
	//		dd[val_len-ii-1] = *begin;
	//	}
	//}
	else 
	{
		BYTE* buf = NULL;
		if (m_len > 8)
		{
			m_data = new BYTE[m_len];
			buf = m_data;
		}
		else buf = m_data_val;

		for (size_t ii = 0; ii < m_len; ++ii, ++begin)
		{
			if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of data, offset = %zd", (begin-m_payload_begin) );
			buf[ii] = *begin;
		}
	}

	// 如果时int, 并且len < 8，复制到value
	// UID是以Binary 8的形式出现的
	//if (m_data && m_len <=8 )
	//{
	//	m_val = 0;
	//	BYTE* dd = (BYTE*)(&m_val);
	//	for (size_t ii = 0; ii < val_len; ++ii)		{			dd[val_len-ii-1] = m_data[ii];		}
	//}

	return true;
}

void MidAtomToken::Print(FILE* ff, int indentation)
{

	//if (m_len == 0)
	//{
	//	fwprintf_s(ff, L"0x%llX", m_val);
	//}
	//else
	//{
	//	fwprintf_s(ff, L"%s<ATOM %s>: ", INDENTATION - indentation,
	//		m_type == CTcgTokenBase::BinaryAtom ? L"BIN" : L"INT");
	//	for (size_t ii = 0; ii < m_len; ++ii)
	//	{
	//		fwprintf_s(ff, L"%02X ", m_data[ii]);
	//	}
	//	fwprintf_s(ff, L"</ATOM>\n");
	//}
}

void MidAtomToken::ToString(std::wostream& out, UINT layer, int opt)
{
	// 输出三种形式：
	// (1) Integer (Hex): 连续的16进制整数，
	// (2) Integer (Dec): 
	// (2) Binary: 连续输出字节
	// (4) Binary: 输出连续字符
	int indentation = HIBYTE(HIWORD(opt));
//	out << (INDENTATION - indentation);
	if (m_type == CTcgTokenBase::IntegerAtom /*&& m_len <= 8*/)
	{	// 情况(1)
		BYTE* buf= (m_len > 8)? m_data : m_data_val;
		out << std::hex << std::noshowbase << std::setfill(L'0');// << std::setfill('0');
		out << L"0x";
		for (size_t ii = 0; ii < m_len; ++ii) out /*<< std::setfill(L'0')*/ << std::setw(2) << buf[ii];
		out << std::dec << std::showbase << std::setfill(L' ');
//		 out << L"<ATOM val=" << m_val << "/>";
	}
	else
	{
		BYTE* buf = (m_len > 8) ? m_data : m_data_val;
		out << L"<A type=" << (m_type == CTcgTokenBase::BinaryAtom ? L"BIN" : L"INT") << L" signe=" << m_signe 
			<< L" len=" << m_len << L"> ";
		if (layer > 0)
		{
			out << std::hex << std::noshowbase; // std::setw(3);
			for (size_t ii = 0; ii < m_len; ++ii) out << std::setfill(L'0') << std::setw(2) << buf[ii] << L" ";
			out << std::dec << std::showbase << std::setfill(L' ');
		}
		out << L" </A>" << std::endl;
	}
}

void MidAtomToken::ToProperty(boost::property_tree::wptree& prop)
{
	BYTE* buf = (m_len > 8) ? m_data : m_data_val;

	if (m_type == CTcgTokenBase::IntegerAtom)
	{	// 情况(1)
		if (m_signe)
		{
			INT64 val;
			GetValue(val);
			prop.put_value(val);
		}
		else
		{
			UINT64 val;
			GetValue(val);
			prop.put_value(val);
		}
	}
	else
	{
		std::wstringstream out;
		out << std::hex << std::noshowbase; // std::setw(3);
		for (size_t ii = 0; ii < m_len; ++ii) out << std::setfill(L'0') << std::setw(2) << buf[ii] << L" ";
//			out << std::dec << std::showbase << std::setfill(L' ');
		prop.put_value(out.str());
	}
}

size_t MidAtomToken::Encode(BYTE* buf, size_t buf_len)
{
	if (m_len == 0)
	{	// 自动化长度
		// for Tiny atom
		if (buf_len < 1)	THROW_ERROR(ERR_APP, L"buffer is too small, len=%zd, request=1", buf_len);
		buf[0] = (BYTE)(m_data_val[0] & 0x3F);
		if (m_signe) buf[0] |= 0x40;
		return 1;
	}

	BYTE* data = nullptr;
	size_t data_len = 0;
	if (m_len < 16)
	{	// for Short atom
		data_len = m_len + 1;
		if (buf_len < data_len ) THROW_ERROR(ERR_APP, L"buffer is too small, len=%zd, request=%zd", buf_len, data_len);
		buf[0] = 0x80 + (BYTE)(m_len & 0xF);
		if (m_type == CTcgTokenBase::BinaryAtom) buf[0] |= 0x20;
		if (m_signe) buf[0] |= 0x10;
		data = (m_len <= 8) ? m_data_val : m_data;
		memcpy_s(buf + 1, buf_len - 1, data, m_len);
	}
	else if (m_len < 2048)
	{
		data_len = m_len + 2;
		if (buf_len <data_len) THROW_ERROR(ERR_APP, L"buffer is too small, len=%zd, request=%zd", buf_len, data_len);
		buf[0] = 0xC0 + (BYTE)((m_len >> 8) & 0x7);
		if (m_type == CTcgTokenBase::BinaryAtom) buf[0] |= 0x10;
		if (m_signe) buf[0] |= 0x8;
		buf[1] = (BYTE)(m_len & 0xFF);
		data = m_data;
		memcpy_s(buf + 2, buf_len - 1, data, m_len);
	}
	else if (m_len < 0x01000000)
	{
		data_len = m_len + 4;
		if (buf_len <data_len) THROW_ERROR(ERR_APP, L"buffer is too small, len=%zd, request=%zd", buf_len, data_len);
		buf[0] = 0xE0;
		if (m_type == CTcgTokenBase::BinaryAtom) buf[0] |= 0x02;
		if (m_signe) buf[0] |= 0x01;
		buf[1] = (BYTE)((m_len >> 16) & 0xFF);
		buf[2] = (BYTE)((m_len >> 8) & 0xFF);
		buf[3] = (BYTE)(m_len & 0xFF);
		data = m_data;
		memcpy_s(buf + 4, buf_len - 1, data, m_len);
	}
	else
	{
		THROW_ERROR(ERR_APP, L"data size should be less than 0x1000000, size=%zd", m_len);
	}
	//memcpy_s(buf + 1, buf_len - 1, data, m_len);
	return data_len;
}

UINT64 MidAtomToken::FormatToInt(void) const
{
	if (m_len > 8)	THROW_ERROR(ERR_APP, L"data length=%d is over UINT64, type=%d", m_len, m_type);
	const BYTE* buf = m_data_val;
	UINT64 val = 0;
	BYTE* dd = (BYTE*)(&val);
	for (size_t ii = 0; ii < m_len; ++ii)		{ dd[m_len-ii-1] = buf[ii];		}
	return val;
}

bool MidAtomToken::FormatToString(std::wstring& str) const
{
	if (m_type != CTcgTokenBase::BinaryAtom) return false;

	jcvos::auto_array<char> str_buf(m_len + 1);
	const BYTE* buf = (m_len > 8) ? m_data : m_data_val;
	memcpy_s(str_buf, m_len + 1, buf, m_len);
	str_buf[m_len] = 0;
	jcvos::Utf8ToUnicode(str, str_buf.get_ptr());
	return true;
}

bool MidAtomToken::FormatToByteString(std::wstring& str) const
{
	if (m_type != CTcgTokenBase::BinaryAtom) return false;
	int str_len = (int)(m_len) * 3 + 4;
	int out_len = 0;
	str.resize(str_len, ' ');
	wchar_t* ch = const_cast<wchar_t*>(str.data());
	const BYTE* buf = (m_len > 8) ? m_data : m_data_val;
	for (size_t ii = 0; ii < m_len; ++ii)
	{
		int out = swprintf_s(ch, str_len, L"%02X ", buf[ii]);
		str_len -= out;
		out_len += out;
		ch += out;
	}
	*ch = 0;
	str.resize(out_len);
	return true;
}

MidAtomToken* MidAtomToken::CreateToken(const std::wstring& wstr)
{
	std::string str;
	jcvos::UnicodeToUtf8(str, wstr);

	MidAtomToken* token = jcvos::CDynamicInstance<MidAtomToken>::Create();
	if (token == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating MidAtomToken");
	token->m_type = CTcgTokenBase::BinaryAtom;
	token->m_signe = false;
	size_t len = str.size();
	token->m_len = len;
	if (len > 8)
	{
		token->m_data = new BYTE[token->m_len];
		memcpy_s(token->m_data, len, str.c_str(), len);
	}
	else memcpy_s(token->m_data_val, 8, str.c_str(), len);
	return token;
}


MidAtomToken* MidAtomToken::CreateToken(const BYTE* data, size_t data_len)
{
	MidAtomToken* token = jcvos::CDynamicInstance<MidAtomToken>::Create();
	if (token == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating MidAtomToken");
	token->m_type = CTcgTokenBase::BinaryAtom;
	token->m_signe = false;
	size_t len = data_len;
	token->m_len = len;
	if (len > 8)
	{
		token->m_data = new BYTE[token->m_len];
		memcpy_s(token->m_data, len, data, len);
	}
	else memcpy_s(token->m_data_val, 8, data, len);
	return token;
}

MidAtomToken* MidAtomToken::CreateToken(const HUID& data)
{
	MidAtomToken* token = jcvos::CDynamicInstance<MidAtomToken>::Create();
	if (token == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating MidAtomToken");
	token->m_type = CTcgTokenBase::BinaryAtom;
	token->m_signe = false;
	token->m_len = 4;
	memcpy_s(token->m_data_val, 8, data, 8);
	return token;
}

MidAtomToken* MidAtomToken::CreateToken(const TCG_UID& data)
{
	MidAtomToken* token = jcvos::CDynamicInstance<MidAtomToken>::Create();
	if (token == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating MidAtomToken");
	token->m_type = CTcgTokenBase::BinaryAtom;
	token->m_signe = false;
	token->m_len = 8;
	memcpy_s(token->m_data_val, 8, data, 8);
	return token;
}

MidAtomToken* MidAtomToken::CreateToken(UINT64 val)
{
	MidAtomToken* token = jcvos::CDynamicInstance<MidAtomToken>::Create();
	if (token == nullptr) THROW_ERROR(ERR_MEM, L"failed on creating MidAtomToken");
	token->m_type = CTcgTokenBase::IntegerAtom;
	token->m_signe = false;
	if (val < 0x40)
	{
		token->m_len = 0;
		token->m_data_val[0] = (BYTE)(val);
	}
	else
	{
		BYTE* data = (BYTE*)(&val);
		int ii = 3;
		while (ii >= 0 && data[ii] == 0) ii--;
		size_t len = 0;
		for (; ii >= 0; ii--) token->m_data_val[len++] = data[ii];
		token->m_len = len;
	}
	return token;
}





///////////////////////////////////////////////////////////////////////////////
// ==== List Tokens ======

ListToken::~ListToken(void)
{
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
//		delete (*it);
		(*it)->Release();
	}
}

bool ListToken::InternalParse(BYTE*& begin, BYTE* end)
{
	CONSUME_TOKEN(begin, end, m_payload_begin, START_LIST);
	while ((begin < end) && (*begin != END_LIST))
	{
		CTcgTokenBase* token = CTcgTokenBase::Parse(begin, end, m_payload_begin);
		if (token == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data (%02X, %02X, ..)", begin[0], begin[1]);
		m_tokens.push_back(token);
	}
	CONSUME_TOKEN(begin, end, m_payload_begin, END_LIST);
	return true;
}

void ListToken::Print(FILE* ff, int indentation)
{
	if (m_type == CTcgTokenBase::TopToken) 	fwprintf_s(ff, L"%s<TOKENS>\n", INDENTATION - indentation);
	else fwprintf_s(ff, L"%s<LIST BEGIN>\n", INDENTATION - indentation);
	indentation += 2;
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
		(*it)->Print(ff, indentation);
	}
	indentation -= 2;
	if (m_type == CTcgTokenBase::TopToken) 	fwprintf_s(ff, L"%s</TOKENS>\n", INDENTATION - indentation);
	else fwprintf_s(ff, L"%s<LIST END>\n", INDENTATION - indentation);
}

void ListToken::ToString(std::wostream& out, UINT layer, int opt)
{
	int indentation = HIBYTE(HIWORD(opt));

	out << (INDENTATION - indentation);
	if (m_type == CTcgTokenBase::TopToken)	out << L"<TOKENS>";
	else									out << L"<LIST>";
	if (layer > 0)
	{
		//opt += 0x02000000;
		out << std::endl;
		auto end_it = m_tokens.end();
		for (auto it = m_tokens.begin(); it != end_it; ++it)
		{
			JCASSERT(*it);
			(*it)->ToString(out, layer - 1, opt + 0x02000000);
		}
		out << (INDENTATION - indentation);
	}
	if (m_type == CTcgTokenBase::TopToken) 	out << L"</TOKENS>";
	else									out << L"</LIST>";
	out << std::endl;
}

void ListToken::ToProperty(boost::property_tree::wptree& prop)
{
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
		(*it)->ToProperty(prop);
	}
}

void ListToken::GetSubItem(ISecurityObject*& sub_item, const std::wstring& name)
{
	for (auto it = m_tokens.begin(); it != m_tokens.end(); ++it)
	{
		if ((*it) && (*it)->m_name == name)
		{
			sub_item = (*it);
			sub_item->AddRef();
		}
	}
}

size_t ListToken::Encode(BYTE* buf, size_t buf_len)
{
	size_t data_len=0;
	buf[0] = START_LIST;
	data_len++;
	buf++;
	buf_len--;
	for (auto it = m_tokens.begin(); it != m_tokens.end(); ++it)
	{
		JCASSERT(*it);
		size_t len = (*it)->Encode(buf, buf_len);
		data_len += len;
		buf += len;
		buf_len -= len;
	}
	buf[0] = END_LIST;
	data_len++;
	buf_len--;
	buf++;
	return data_len;
}


///////////////////////////////////////////////////////////////////////////////
// ==== Name Tokens ======

NameToken::~NameToken(void)
{
//	delete m_value;
	RELEASE(m_name_id);
	RELEASE(m_value);
}

bool NameToken::InternalParse(BYTE*& begin, BYTE* end)
{
	CONSUME_TOKEN(begin, end, m_payload_begin, START_NAME);
	m_name_id = MidAtomToken::ParseAtomToken(begin, end, m_payload_begin);
	//if (nn == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing name, offset=%zd", begin - m_payload_begin);
	//bool br = nn->GetValue(m_name_id);
	//if (!br)
	//{
	//	LOG_ERROR(L"failed on getting name from atom");
	//	CErrorToken * err = jcvos::CDynamicInstance<CErrorToken>::Create();
	//	err->ParseToken(begin, end, m_payload_begin);
	//	err->m_name = L"err";
	//	err->m_error_msg = L"name atom is too longer than 8";
	//	m_value = err;
	//	delete nn;
	//	return false;
	//}
	//m_name_id = boost::numeric_cast<DWORD>( nn->GetValue());
	////wchar_t str[10];
	////swprintf_s(str, L"%X", )
	//m_name = std::to_wstring(m_name_id);
	//delete nn;
	m_value = Parse(begin, end, m_payload_begin);
	if (m_value == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data, offset=%zd", begin - m_payload_begin);
	CONSUME_TOKEN(begin, end, m_payload_begin, END_NAME);
	return true;
}

void NameToken::Print(FILE* ff, int indentation)
{
	//fwprintf_s(ff, L"%s<NAME=%d>, val=", INDENTATION - indentation, m_name_id);
	//if (m_value) m_value->Print(ff, 0);
	//fwprintf_s(ff, L"</NAME>\n");
}

void NameToken::ToString(std::wostream& out, UINT layer, int opt)
{
	int indentation = HIBYTE(HIWORD(opt));
//	out << (INDENTATION - indentation);
	size_t sub_len = m_value ? m_value->GetPayloadLen() : 0;
//	MidAtomToken* nn = dynamic_cast<MidAtomToken*>(m_name_id);
//	DWORD val;
	out << (INDENTATION - indentation) << L"<NAME_TOKEN>" << std::endl;
	
	//if (m_name_id && m_name_id->m_type == CTcgTokenBase::BinaryAtom)
	//{
	//	std::wstring str;
	//	m_name_id->FormatToString(str);
	//	out << L"<NAME name=" << str << L">";
	//}
	//else if (m_name_id && m_name_id->GetValue(val))	
	//{
	//	out << L"<NAME name=" << val << L">";	
	//}
	//else
	//{
	//	out << L"<NAME name=";
	//	m_name_id->ToString(out, layer, opt);
	//	out << L">";
	//}
	indentation += 2;
	if (layer > 0)
	{
		out << (INDENTATION - indentation) << L"<name>";
//		if (sub_len > NEW_LINE_SUB)		out << std::endl;
//		opt += 0x02000000;
		m_name_id->ToString(out, layer - 1, opt);
//		if (sub_len > NEW_LINE_SUB)		out << std::endl;
//		out << (INDENTATION - indentation);
		out << L"</name>" << std::endl;
	}

	if (layer > 0 && m_value)
	{
		if (sub_len > NEW_LINE_SUB)		out << std::endl;
		out << L"<value>";
		opt += 0x02000000;
		m_value->ToString(out, layer - 1, opt);
		if (sub_len > NEW_LINE_SUB)		out << std::endl;
//		out << (INDENTATION - indentation);
		out << L"</value>";
	}
	indentation -= 2;
	out << (INDENTATION - indentation)<< L"</NAME_TOKEN>" << std::endl;
}

void NameToken::ToProperty(boost::property_tree::wptree& prop)
{
	JCASSERT(m_name_id);
	std::wstring str_name;
	DWORD name_val;
	if (m_name_id->m_type == CTcgTokenBase::BinaryAtom)
	{
		m_name_id->FormatToString(str_name);
	}
	else if (m_name_id->GetValue(name_val))
	{
		wchar_t str[24];
		swprintf_s(str, L"name_%X", name_val);
		str_name = str;
	}
	else { str_name = L"not_show"; }
	boost::property_tree::wptree val_pt;
	m_value->ToProperty(val_pt);
	prop.add_child(str_name, val_pt);
}

size_t NameToken::Encode(BYTE* buf, size_t buf_len)
{
	size_t data_len=0;
	buf[0] = START_NAME;
	data_len++;
	buf++;
	buf_len--;

	size_t len = m_name_id->Encode(buf, buf_len);
	data_len += len;
	buf += len;
	buf_len -= len;

	len = m_value->Encode(buf, buf_len);
	data_len += len;
	buf += len;
	buf_len -= len;

	buf[0] = END_NAME;
	data_len++;
	buf_len--;
	buf++;
	return data_len;
}

//NameToken * NameToken::CreateToken(const std::wstring& name, UINT val)
//{
//	NameToken* token = jcvos::CDynamicInstance<NameToken>::Create();
//	if (!token) THROW_ERROR(ERR_MEM, L"failed on creating NameToken");
//	std::string str;
//	jcvos::UnicodeToUtf8(str, name);
//	token->m_name_id = MidAtomToken::CreateToken(str);
//	token->m_value = MidAtomToken::CreateToken(val);
//	return token;
//}

NameToken* NameToken::CreateToken(UINT id, CTcgTokenBase* val)
{
	JCASSERT(val);
	NameToken* token = jcvos::CDynamicInstance<NameToken>::Create();
	if (!token) THROW_ERROR(ERR_MEM, L"failed on creating NameToken");
	token->m_name_id = MidAtomToken::CreateToken(id);
	token->m_value = val;
	token->m_value->AddRef();
	return token;
}

NameToken* NameToken::CreateToken(UINT id, const BYTE* data, size_t data_len)
{
//	JCASSERT(data && data_len>0);
	NameToken* token = jcvos::CDynamicInstance<NameToken>::Create();
	if (!token) THROW_ERROR(ERR_MEM, L"failed on creating NameToken");
	token->m_name_id = MidAtomToken::CreateToken(id);
	token->m_value = MidAtomToken::CreateToken(data, data_len);
//	token->m_value->AddRef();
	return token;
}

//NameToken* NameToken::CreateToken(UINT id, UINT64 val)
//{
//	NameToken* token = jcvos::CDynamicInstance<NameToken>::Create();
//	if (!token) THROW_ERROR(ERR_MEM, L"failed on creating NameToken");
//	token->m_name_id = MidAtomToken::CreateToken(id);
//	token->m_value = MidAtomToken::CreateToken(val);
//	return token;
//}


///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
bool CStatePhrase::InternalParse(BYTE*& begin, BYTE* end)
{
	CONSUME_TOKEN(begin, end, m_payload_begin, CTcgTokenBase::START_LIST);

	for (int ii = 0; ii < 3; ++ii)
	{
		if (*begin == EMPTY_TOKEN)
		{
			CONSUME_TOKEN(begin, end, m_payload_begin, CTcgTokenBase::EMPTY_TOKEN);
			m_empty[ii] = 1;
		}
		else
		{
			MidAtomToken* t1 = MidAtomToken::ParseAtomToken(begin, end, m_payload_begin);
			if (t1 == nullptr) THROW_ERROR(ERR_APP, L"expected atome. offset=%zd", m_payload_begin);
			t1->GetValue(m_state[ii]);
//			m_state[ii] = boost::numeric_cast<UINT>(t1->GetValue());
			delete t1;
		}

		//t1 = MidAtomToken::ParseAtomToken(begin, end, start);
		//if (t1 == nullptr) THROW_ERROR(ERR_APP, L"expected atome. offset=%zd", start);
		//m_s1 = boost::numeric_cast<UINT>(t1->GetValue());
		//delete t1;

		//t1 = MidAtomToken::ParseAtomToken(begin, end, start);
		//if (t1 == nullptr) THROW_ERROR(ERR_APP, L"expected atome. offset=%zd", start);
		//m_s1 = boost::numeric_cast<UINT>(t1->GetValue());
		//delete t1;
	}
	CONSUME_TOKEN(begin, end, m_payload_begin, CTcgTokenBase::END_LIST);
	return true;
}

const wchar_t* GetStateString(UINT state, UINT empty)
{
	const wchar_t* str_state;
	if (empty) str_state = L"<EMPTY>";
	else
	{
		switch (state)
		{
		case 0x00: str_state = L"SUCCESS"; break;
		case 0x01: str_state = L"NOT_AUTHORIZED"; break;
		case 0x03: str_state = L"SP_BUSY"; break;
		case 0x04: str_state = L"SP_FAILED"; break;
		case 0x05: str_state = L"SP_DISABLED"; break;
		case 0x06: str_state = L"SP_FROZEN"; break;
		case 0x07: str_state = L"NO_SESSIONS_AVAILABLE"; break;
		case 0x08: str_state = L"UNIQUENESS_CONFLICT"; break;
		case 0x09: str_state = L"INSUFFICIENT SPACE"; break;
		case 0x0A: str_state = L"INSUFFICIENT ROWS"; break;
		case 0x0C: str_state = L"INVALID_PARAMETER"; break;
		case 0x0F: str_state = L"TPER_MALFUNCTION"; break;
		case 0x10: str_state = L"TRANSACTION_FAILURE"; break;
		case 0x11: str_state = L"PESPONSE_OVERFLOW"; break;
		case 0x12: str_state = L"AUTHORITY_LOCKED_OUT"; break;
		case 0x3F: str_state = L"FAIL"; break;

		case 0x0D:
		case 0x0E:
		case 0x02: str_state = L"OBSOLETE"; break;
		default:   str_state = L"Unkonw State Code"; break;
		}
	}
	return str_state;
}

void CStatePhrase::Print(FILE* ff, int indentation)
{
	const wchar_t* str_state = GetStateString(m_state[0], m_empty[0]);

	fwprintf_s(ff, L"STATE=%s(0x%02X), 0x%02X, 0x%02X\n", str_state, 
		m_empty[0]?0xFFF:m_state[0], 
		m_empty[1]?0xFFF:m_state[1], 
		m_empty[2]?0xFFF:m_state[2]);
}

void CStatePhrase::ToString(std::wostream& out, UINT layer, int opt)
{
	const wchar_t* str_state = GetStateString(m_state[0], m_empty[0]);
	int indentation = HIBYTE(HIWORD(opt));
	out << (INDENTATION - indentation);
	out << L"<STATE state=" << str_state << L"(" << (m_empty[0] ? 0xFFF : m_state[0]) << L"), ";
	for (int ii = 1; ii < 3; ++ii) out << (m_empty[ii] ? 0xFFF : m_state[ii]) << L",";
	out << L">" << std::endl;
}

void CStatePhrase::ToProperty(boost::property_tree::wptree& prop)
{
	const wchar_t* str_state = GetStateString(m_state[0], m_empty[0]);
	prop.add(L"state", std::wstring(str_state));
}

///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====

CallToken::~CallToken(void)
{
	auto end_it = m_parameters.end();
	for (auto it = m_parameters.begin(); it != end_it; ++it)
	{
//		delete it->m_token_val;
		(*it)->Release();
	}
	m_parameters.clear();
	RELEASE(m_state);
}

bool CallToken::InternalParse(BYTE*& begin, BYTE* end)
{	// 解析函数调用，未去掉了Call Token
	if (g_uid_map == nullptr) THROW_ERROR(ERR_APP, L"missing UID map");
	begin++; // consume the call token
	// 解析 invoking
	MidAtomToken* t1 = MidAtomToken::ParseAtomToken(begin, end, m_payload_begin);
	if (t1 == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing atom offset=%zd", begin - m_payload_begin);
//	m_invoking_id = t1->GetValue();
	t1->GetValue(m_invoking_id);
	delete t1;
	t1 = nullptr;
	const UID_INFO * uid_info = g_uid_map->GetUidInfo(m_invoking_id);

	// ON Invoking
	if (uid_info)
	{
		if ((uid_info->m_class & UID_INFO::Involing) == 0) THROW_ERROR(ERR_APP,
			L"UID:%016llX (%s) is not a invoking, offset=%zd", m_invoking_id, uid_info->m_name.c_str(),
			begin - m_payload_begin);
		m_invoking_name = uid_info->m_name;
	}
	else
	{
		wchar_t str[100];
//		swprintf_s(str, L"Unknown(%016llX)", m_invoking_id);
		swprintf_s(str, L"(%016llX)", m_invoking_id);
		m_invoking_name = str;
	}

	// 解析 method
	t1 = MidAtomToken::ParseAtomToken(begin, end, m_payload_begin);
	if (t1 == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing atom offset=%zd", begin - m_payload_begin);

	//t2.Parse(begin, end, start);
//	m_method_id = t1->GetValue();
	t1->GetValue(m_method_id);
	delete t1;
	t1 = nullptr;
	uid_info = g_uid_map->GetUidInfo(m_method_id);

	// OnMethod
	bool br;
	if (uid_info)
	{
		if ((uid_info->m_class & UID_INFO::Method) == 0)
		{
			THROW_ERROR(ERR_APP, L"UID:%016llX (%s) is not a method, offset=%zd", m_method_id, uid_info->m_name.c_str(),
				begin - m_payload_begin);
		}
		m_method_name = uid_info->m_name;
		METHOD_INFO* method_info = uid_info->m_method;
		CONSUME_TOKEN(begin, end, m_payload_begin, START_LIST);
	// 解析 parameter
		br = ParseRequestedParameter(method_info->m_required_param, begin, end, m_payload_begin);
		if (!br) return false;
	// 解析 optional parameter
		br = ParseOptionalParameter(method_info->m_option_param, begin, end, m_payload_begin);
		if (!br) return false;
	}
	else
	{
		wchar_t str[100];
		//swprintf_s(str, L"Unknown(%016llX)", m_method_id);
		swprintf_s(str, L"(%016llX)", m_method_id);
		m_method_name = str;
		if (*begin != START_LIST)
		{
			THROW_ERROR(ERR_APP, L"expected start list token, offset=%zd, val=0x%02X", (begin - m_payload_begin), *begin);
		}
		begin++;
		ParseParameter(begin, end, m_payload_begin);
	}
	m_name = m_invoking_name + L"." + m_method_name;

	CONSUME_TOKEN(begin, end, m_payload_begin, END_LIST);
	CONSUME_TOKEN(begin, end, m_payload_begin, END_OF_DATA);
	// <TODO> 解析 state
	m_state = jcvos::CDynamicInstance<CStatePhrase>::Create();
	m_state->ParseToken(begin, end, m_payload_begin);
	return true;
}

bool CallToken::ParseRequestedParameter(PARAM_INFO::PARAM_LIST & param_list, BYTE* &begin, BYTE* end, BYTE* start)
{
	// Check "START LIST"
	size_t param_size = param_list.size();
	for (size_t ii = 0; ii < param_size; ++ii)
	{
		PARAM_INFO& param_info = param_list[ii];
		CParameterToken *param = jcvos::CDynamicInstance<CParameterToken>::Create();
		param->m_num = param_info.m_num;
		param->m_name = param_info.m_name;
		param->m_type = param_info.m_type_id;
		// <TODO> 根据不同的type解析参数
		if (*begin == END_LIST)
		{
			param->m_type = PARAM_INFO::Err_MissingRequest;
			param->m_token_val = nullptr;
		}
		else
		{
//			param->m_token_val = CTcgTokenBase::Parse(begin, end, start);
			param->ParseToken(begin, end, start);
			//if (param->m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - m_payload_begin);
		}
		m_parameters.push_back(param);
	}
	return true;
}

bool CallToken::ParseParameter(BYTE* &begin, BYTE* end, BYTE* start)
{
	DWORD requested = 0;
	while (1)
	{
		if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of streaming");
		if (*begin == END_LIST) break;
		CParameterToken * param=jcvos::CDynamicInstance<CParameterToken>::Create();
		param->m_type = PARAM_INFO::OtherType;
		wchar_t str[100];
		if (*begin == START_NAME)
		{	// optianl param
			CONSUME_TOKEN(begin, end, start, START_NAME);
			// parse name
			MidAtomToken* name = MidAtomToken::ParseAtomToken(begin, end, start);
			if (name == nullptr) THROW_ERROR(ERR_APP, L"failed on parse AtomToken, offset=%zd", (begin - m_payload_begin));
			DWORD param_num;	//= boost::numeric_cast<DWORD>(name->GetValue());
			name->GetValue(param_num);
			delete name;
			param->m_num = param_num;
			swprintf_s(str, L"OPTION_PARAM[%d]", param_num);
			// parse value
			param->ParseToken(begin, end, start);
			//param->m_token_val = Parse(begin, end, start);
			//if (param->m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - m_payload_begin);
			CONSUME_TOKEN(begin, end, start, END_NAME);
		}
		else
		{	// requested param
			swprintf_s(str, L"REQUESTED_PARAM[%d]", requested);
			param->m_num = requested;
			requested++;
			// parse value
			param->ParseToken(begin, end, start);
			//param->m_token_val = Parse(begin, end, start);
			//if (param->m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - m_payload_begin);
		}
		param->m_name = str;
		m_parameters.push_back(param);
	}
	return true;
}

bool CallToken::ParseOptionalParameter(PARAM_INFO::PARAM_LIST& param_list, BYTE* &begin, BYTE* end, BYTE* start)
{
	while (1)
	{
		if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of streaming");
		if (*begin == END_LIST) break;
//		CONSUME_TOKEN(begin, end, start, START_NAME);
		if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of streaming");
		if (*begin != START_NAME)
		{
			CErrorToken* err = jcvos::CDynamicInstance<CErrorToken>::Create();
			err->ParseToken(begin, end, start);
			err->m_name = L"err";
			err->m_error_msg = L"expected START_NAME";
			m_parameters.push_back(err);
			return false;
		}
		begin++;
	//<END OF CONSUME>

		// parse name
		MidAtomToken* name = MidAtomToken::ParseAtomToken(begin, end, start);
		if (name == nullptr) THROW_ERROR(ERR_APP, L"failed on parse AtomToken, offset=%zd", (begin - m_payload_begin));
		DWORD param_num;// = boost::numeric_cast<DWORD>(name->GetValue());
		name->GetValue(param_num);
		delete name;
		CParameterToken *param=jcvos::CDynamicInstance<CParameterToken>::Create();
		param->m_num = param_num;

		if (param_num >= param_list.size())
		{	
//			THROW_ERROR(ERR_APP, L"illeagle param name (%lld), offset=%zd", param_num, (begin - m_payload_begin));
			LOG_WARNING(L"param (%d) id is large than optional param number (%d)", param_num, param_list.size());
			// parse name
			wchar_t str[32];
			swprintf_s(str, L"OPTION_PARAM[%d]", param_num);
			param->m_name = str;
		}
		else
		{	// parse value
			PARAM_INFO& param_info = param_list[param_num];
			param->m_name = param_info.m_name;
			param->m_type = param_info.m_type_id;
		}
		param->ParseToken(begin, end, start);
		m_parameters.push_back(param);
		CONSUME_TOKEN(begin, end, start, END_NAME);
	}
	return true;
}

void CallToken::Print(FILE* ff, int indentation)
{
	fwprintf_s(ff, L"CALL: %s.%s [\n", m_invoking_name.c_str(), m_method_name.c_str());
	auto end_it = m_parameters.end();
	for (auto it = m_parameters.begin(); it != end_it; ++it)
	{
		if (*it) 	(*it)->Print(ff, indentation);
	}
	fwprintf_s(ff, L"]\n");
	if (m_state) m_state->Print(ff, indentation);
}

void CallToken::ToString(std::wostream& out, UINT layer, int opt)
{
	int indentation = HIBYTE(HIWORD(opt));
	out << (INDENTATION - indentation);
	out << L"<CALL, " << m_invoking_name.c_str() << L"." << m_method_name.c_str() << L">";
	if (layer > 0)
	{
		//opt += 0x02000000;
		out << std::endl;
		auto end_it = m_parameters.end();
		for (auto it = m_parameters.begin(); it != end_it; ++it)
		{
			if (*it)	(*it)->ToString(out, layer - 1, opt + 0x02000000);
		}
		out << (INDENTATION - indentation-2);
		if (m_state) m_state->ToString(out, layer, opt);
	}
	out << L"</CALL>" << std::endl;
}

void CallToken::ToProperty(boost::property_tree::wptree& prop)
{
	boost::property_tree::wptree call_pt;
	call_pt.add(L"invoking", m_invoking_name);
	call_pt.add(L"method", m_method_name);
	boost::property_tree::wptree param_set;
	
	auto end_it = m_parameters.end();
	for (auto it = m_parameters.begin(); it != end_it; ++it)
	{
//		boost::property_tree::wptree param_pt;
		if (*it)	(*it)->ToProperty(param_set);
//		param_set.push_back(std::make_pair( L"param", param_pt) );
	}
	call_pt.add_child(L"parameters", param_set);
//	boost::property_tree::wptree state_pt;
	if (m_state) m_state->ToProperty(call_pt);
//	call_pt.add_child(L"state", state_pt);
	prop.add_child(L"call", call_pt);
}

void CallToken::GetSubItem(ISecurityObject*& sub_item, const std::wstring& name)
{
	if (name == L"state")
	{
		sub_item = m_state;
		if (sub_item) sub_item->AddRef();
		return;
	}

	for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it)
	{
		if ((*it) && (*it)->m_name == name)
		{
			sub_item = *it;
			sub_item->AddRef();
			return;
		}
	}

}

void CParameterToken::Print(FILE* ff, int indentation)
{
	if (g_uid_map == nullptr) THROW_ERROR(ERR_APP, L"missing UID map");
	fwprintf_s(ff, L"\t %s=", m_name.c_str());
	switch (m_type)
	{
	case PARAM_INFO::UidRef: {
		MidAtomToken* mt = dynamic_cast<MidAtomToken*>(m_token_val);
		if (!mt)
		{
			fwprintf_s(ff, L"<!expected atom value>");
			break;
		}
		UINT64 uid;	// = mt->GetValue();
		mt->GetValue(uid);
		const UID_INFO* uid_info = g_uid_map->GetUidInfo(uid);
		if (uid_info) fwprintf_s(ff, L"%s ", uid_info->m_name.c_str());
		fwprintf_s(ff, L"(UID=%016llX)", uid);
		break; }

	case PARAM_INFO::Err_MissingRequest: {
		fwprintf_s(ff, L"<!missing requested parameter>");
		break;
	}

	default:
		if (m_token_val) m_token_val->Print(ff, 0);

	}
	fwprintf_s(ff, L"\n");
}

void CParameterToken::ToString(std::wostream& out, UINT layer, int opt)
{
	if (g_uid_map == nullptr) THROW_ERROR(ERR_APP, L"missing UID map");
	int indentation = HIBYTE(HIWORD(opt));
	out << (INDENTATION - indentation) << L"<Param, name=" << m_name.c_str() << L">";
	size_t sub_len = m_token_val? m_token_val->GetPayloadLen() : 0;
	if (layer > 0)
	{
		//opt += 0x02000000;
		switch (m_type)
		{
		case PARAM_INFO::UidRef: {
			MidAtomToken* mt = dynamic_cast<MidAtomToken*>(m_token_val);
			if (!mt)	
			{
				out << L"[err] expected atom value";
				break;
			}
			UINT64 uid; //= mt->GetValue();
			mt->GetValue(uid);
			const UID_INFO* uid_info = g_uid_map->GetUidInfo(uid);
			if (uid_info) out << uid_info->m_name.c_str();
			out << L"(UID=" << std::hex << uid << L")" << std::dec;
			break; }

		case PARAM_INFO::Err_MissingRequest: {
			out << L"[err] missing requested parameter";
			break;
		}

		default:
			if (m_token_val)
			{
				if (sub_len > NEW_LINE_SUB)		out << std::endl;
				m_token_val->ToString(out, layer - 1, opt + 0x02000000);
				if (sub_len > NEW_LINE_SUB)		out << std::endl;
				out << (INDENTATION - indentation);
			}
		}
	}
	out << L"</Param>" << std::endl;

}

void CParameterToken::ToProperty(boost::property_tree::wptree& prop)
{

	switch (m_type)
	{
	case PARAM_INFO::UidRef: {
		MidAtomToken* mt = dynamic_cast<MidAtomToken*>(m_token_val);
		if (!mt)
		{
			prop.add(m_name, L"[err] expected atom of uid");
			break;
		}
		UINT64 uid; //= mt->GetValue();
		mt->GetValue(uid);
		const UID_INFO* uid_info = g_uid_map->GetUidInfo(uid);
		boost::property_tree::wptree pp;
		
		if (uid_info)	pp.add(L"obj", uid_info->m_name.c_str());
		pp.add(L"uid", uid);
		prop.add_child(m_name, pp);
		break; }

	case PARAM_INFO::Err_MissingRequest: {
		prop.add(m_name, L"[err] missing reuqested parameter");
		break;
	}

	default:
		if (m_token_val)
		{
			boost::property_tree::wptree pp;
			m_token_val->ToProperty(pp);
			prop.add_child(m_name, pp);
		}
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// ==== Uid Map ====

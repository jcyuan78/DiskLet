#include "pch.h"

#include "tcg-token.h"
#include <boost/cast.hpp>

LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_DEBUGINFO);

static const wchar_t* _INDENTATION = L"                              ";
static const wchar_t* INDENTATION = _INDENTATION + wcslen(_INDENTATION) - 1;

CUidMap g_uid_map;

#define CONSUME_TOKEN(b, e, s, t) { \
		if (b>=e) THROW_ERROR(ERR_APP, L"unexpected end of streaming");	\
		if (*b != t) THROW_ERROR(ERR_APP, L"expected " #t " token, offset=%zd, val=0x%02X",\
		(b - s), *s);	b++; }


///////////////////////////////////////////////////////////////////////////////
// ==== CTcgTokenBase ======

CTcgTokenBase* CTcgTokenBase::Parse(BYTE*& begin, BYTE* end, BYTE * start)
{
	CTcgTokenBase* token = nullptr;
	if (*begin <= 0xE3)
	{
		token = static_cast<CTcgTokenBase*>(MidAtomToken::ParseAtomToken(begin, end, start) );
	}
	else
	{
		switch (*begin)
		{
		case 0xF0:	// START_LIST;
			token = static_cast<CTcgTokenBase*>(new ListToken(CTcgTokenBase::List));
//			begin++;
			token->ParseToken(begin, end, start);
			break;

		case 0xF1:	// END_LIST;
			//begin++;
			THROW_ERROR(ERR_APP, L"unexpected end list token, offset=%zd", (begin - start));
			break;

		case 0xF2:	// START_NAME;
			token = static_cast<CTcgTokenBase*>(new NameToken);
			//begin++;
			token->ParseToken(begin, end, start);
			break;
		case 0xF3:	// END_NAME;
			//begin++;
			THROW_ERROR(ERR_APP, L"unexpected end name token, offset=%zd", (begin - start));
			break;

		case 0xF8:	// CALL;
			token = new CTcgTokenBase(Call);
			begin++;
			break;

		case 0xF9:	// END_OF_DATA;
			token = new CTcgTokenBase(EndOfData);
			begin++;
			break;

		case 0xFA:	// END_OF_SECTION;
			token = new CTcgTokenBase(EndOfSession);
			begin++;
			break;

		case 0xFB:	// START_TRANSACTION;
		case 0xFC:	// END_TRANSACTION;
			THROW_ERROR(ERR_APP, L"not support for transaction, offset=%zd", (begin - start));
			break;
		case 0xFF:	// EMPTY;
			token = new CTcgTokenBase(CTcgTokenBase::Empty);
			begin++;
			break;

		default:
			THROW_ERROR(ERR_APP, L"unknown or reserved token: 0x%02X, offset=%zd", *begin, begin - start);
			break;
		}
	}
	return token;
}

CTcgTokenBase* CTcgTokenBase::SyntaxParse(BYTE*& begin, BYTE* end, BYTE* start, bool receive)
{
	CTcgTokenBase* token = nullptr;
	switch (*begin)
	{
	case TOKEN_CALL: {	// CALL
		CallToken* tt = new CallToken;
		tt->ParseToken(begin, end, start);
		token = static_cast<CTcgTokenBase*>(tt);
		break; }

	case START_LIST: {
		if (!receive) THROW_ERROR(ERR_APP, L"illeagle return value in send command");
		ListToken* tt = new ListToken(CTcgTokenBase::List);
		tt->ParseToken(begin, end, start);
		CONSUME_TOKEN(begin, end, start, END_OF_DATA);
		CStatePhrase* ss = new CStatePhrase;
		ss->ParseToken(begin, end, start);
		tt->AddToken(ss);
		token = static_cast<CTcgTokenBase*>(tt);
		break; }

	case END_OF_SESSION:
//		if (receive) THROW_ERROR(ERR_APP, L"illeagle end of session in receive command");
		token = new CTcgTokenBase(EndOfSession);
		begin++;
		break;
	}
	return token;
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


///////////////////////////////////////////////////////////////////////////////
// ==== Atom Tokens ======

bool MidAtomToken::ParseToken(BYTE*& begin, BYTE* end, BYTE * start)
{	// 解析所有长度大于 8 字节的atom token。
	// 如果长度小于等于8， 返回后转换成小token。
	//bool bytes = false;
	//bool signe_int = false;
	BYTE token = begin[0];
#ifdef _DEBUG
	m_token = token;
#endif

	if ( (token & 0x80) == 0 )
	{	// tiny atom
		m_len = 0;
		m_bytes = false;
		m_sign = ((token & 0x40) !=0);
		m_val = token & 0x3F;
		begin++;
	}
	else if ((token & 0xC0) == 0x80)
	{	// Short Atom
		LOG_DEBUG(L"parsing short atom token");
		m_bytes = ((token & 0x20) != 0);
		m_sign = ((token & 0x10) != 0);
		m_len = (token) & 0x0F;
		begin++;
	}
	else if ((token & 0xE0) == 0xC0)
	{	// Mit Atom
		LOG_DEBUG(L"parsing medium atom token");
		m_bytes = ((token & 0x10) != 0);
		m_sign = ((token & 0x08) != 0);
		size_t hi = token & 0x07;
		m_len = MAKEWORD(begin[1], hi);
		begin += 2;
	}
	else if ((token & 0xF0) == 0xE0)
	{	// Long Atom
		LOG_DEBUG(L"parsing long atom token");
		m_bytes = ((token & 0x02) != 0);
		m_sign = ((token & 0x01) != 0);
		m_len = MAKELONG(MAKEWORD(begin[3], begin[2]), begin[1]);
		begin += 4;
	}

	else
	{
		THROW_ERROR(ERR_APP, L"unexpected atom token, token=0x%02X, offset=%d", token, begin - start);
	}

	//	if (m_bytes == 0) THROW_ERROR(ERR_APP, L"unsupport integer atom, token=%02X", token);
	if (!m_bytes)	m_type = CTcgTokenBase::IntegerAtom;
	else	m_type = CTcgTokenBase::BinaryAtom;

	if (m_len == 0) { /*m_len = 1;*/ }
	else if (!m_bytes && m_len <= 8)
	{
		BYTE* dd = (BYTE*)(&m_val);
		for (size_t ii = 0; ii < m_len; ++ii, ++begin)
		{
			if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of data, offset = %zd", (begin - start));
			dd[m_len - ii - 1] = *begin;
		}
//		m_len = 0;
	}
	else
	{
		m_data = new BYTE[m_len];
		BYTE* dd = (BYTE*)(&m_val);
		for (size_t ii = 0; ii < m_len; ++ii, ++begin)
		{
			if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of data, offset = %zd", (begin - start));
			m_data[ii] = *begin;
			if (m_len <= 8) dd[m_len - ii - 1] = *begin;
		}
	}
	if (m_sign) THROW_ERROR(ERR_APP, L"unspport continue = 1");
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
	//		fwprintf_s(ff, L"%02X ", d.m_data[ii]);
	//	}
	//	fwprintf_s(ff, L"</ATOM>\n");
	//}
	std::wstring str;
	ToString(str);
	fwprintf_s(ff, L"%s<A>%s</A>: ", INDENTATION - indentation, str.c_str());

}

//UINT64 MidAtomToken::GetValue(void) const
//{
//	if (m_len == 0) return d.m_val;
//	else THROW_ERROR(ERR_APP, L"data length=%d is over UINT64", m_len);
//}

wchar_t V2Hex(BYTE b)
{
	if (b < 10) return '0' + b;
	else return 'A' + (b - 10);
}

void MidAtomToken::ToString(std::wstring& str)
{
//	str.clear();
	wchar_t* buf0 = NULL;
	if (m_bytes)
	{	// 字符串
		if (m_len == 0)
		{
			str = L"";
			return;
		}
		JCASSERT(m_data);
		//str.resize(m_len * 4 + 8, 0);
		buf0 = new wchar_t[m_len * 4 + 8];
//		memset(buf0, 0, (m_len * 4 + 8) * sizeof(wchar_t));
//		wchar_t * buf = const_cast<wchar_t*>(str.data());
		wchar_t* buf = buf0;
		wchar_t* buf2 = buf + m_len * 3 + 2;
		buf[0] = '['; buf++;
		for (size_t ii = 0; ii < m_len; ii++)
		{
			BYTE dd = m_data[ii];
			buf[0] = V2Hex(dd>> 4);
			buf[1] = V2Hex(dd& 0xF);
			buf[2] = ' ';
			buf += 3;
			buf2[0] = (dd>= '0' && dd< 127) ? dd: '.';
			buf2++;
		}
		buf[0] = ':';
		buf2[0] = ']'; buf2++;
		buf2[0] = 0;
		//size_t ss = buf2 - str.data();
		//str.resize(ss);
	}
	else
	{
		if (m_data)
		{
			size_t len = m_len * 2 + 10;
			buf0 = new wchar_t[len];
			wchar_t *buf = buf0;
			memset(buf0, 0, len);
			for (size_t ii = 0; ii < m_len; ++ii)
			{
				BYTE dd = m_data[ii];
				buf[0] = V2Hex(dd >> 4);
				buf[1] = V2Hex(dd & 0xF);
				buf += 2;
			}
		}
		else
		{
			buf0 = new wchar_t[32];
			swprintf_s(buf0, 32, L"0x%llX", m_val);
		}
	}
	str = buf0;
	delete[] buf0;
}
///////////////////////////////////////////////////////////////////////////////
// ==== List Tokens ======

ListToken::~ListToken(void)
{
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
		delete (*it);
	}
}

bool ListToken::ParseToken(BYTE*& begin, BYTE* end, BYTE * start)
{
	CONSUME_TOKEN(begin, end, start, START_LIST);
	while ((begin < end) && (*begin != END_LIST))
	{
		CTcgTokenBase* token = CTcgTokenBase::Parse(begin, end, start);
		if (token == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data (%02X, %02X, ..)", begin[0], begin[1]);
		m_tokens.push_back(token);
	}
	CONSUME_TOKEN(begin, end, start, END_LIST);
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


///////////////////////////////////////////////////////////////////////////////
// ==== Name Tokens ======

NameToken::~NameToken(void)
{
	delete m_name;
	delete m_value;
}

bool NameToken::ParseToken(BYTE*& begin, BYTE* end, BYTE * start)
{
	CONSUME_TOKEN(begin, end, start, START_NAME);
	m_name = MidAtomToken::ParseAtomToken(begin, end, start);
	m_value = Parse(begin, end, start);
	if (m_value == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data, offset=%zd", begin - start);
	CONSUME_TOKEN(begin, end, start, END_NAME);
	return true;
}

void NameToken::Print(FILE* ff, int indentation)
{
	// name to string
	std::wstring str;
	m_name->ToString(str);
	fwprintf_s(ff, L"%s<NAME=%s>, val=", INDENTATION - indentation, str.c_str());
	if (m_value) m_value->Print(ff, 0);
	fwprintf_s(ff, L"</NAME>\n");
}


///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
bool CStatePhrase::ParseToken(BYTE*& begin, BYTE* end, BYTE* start)
{
	CONSUME_TOKEN(begin, end, start, CTcgTokenBase::START_LIST);

	for (int ii = 0; ii < 3; ++ii)
	{
		if (*begin == EMPTY_TOKEN)
		{
			CONSUME_TOKEN(begin, end, start, CTcgTokenBase::EMPTY_TOKEN);
			m_empty[ii] = 1;
		}
		else
		{
			MidAtomToken* t1 = MidAtomToken::ParseAtomToken(begin, end, start);
			if (t1 == nullptr) THROW_ERROR(ERR_APP, L"expected atome. offset=%zd", start);
//			m_state[ii] = boost::numeric_cast<UINT>(t1->GetValue());
			if (t1->m_len > sizeof(UINT))
				THROW_ERROR(ERR_APP, L"length of state[%d] (%zd) is larger than UINT", ii, t1->m_len);
			m_state[ii] = t1->GetValue<UINT>();
			delete t1;
		}
	}
	CONSUME_TOKEN(begin, end, start, CTcgTokenBase::END_LIST);
	return true;
}

void CStatePhrase::Print(FILE* ff, int indentation)
{
	const wchar_t* str_state;
	if (m_empty[0]) str_state = L"<EMPTY>";
	else
	{
		switch (m_state[0])
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
	fwprintf_s(ff, L"STATE=%s(0x%02X), 0x%02X, 0x%02X\n", str_state, 
		m_empty[0]?0xFFF:m_state[0], 
		m_empty[1]?0xFFF:m_state[1], 
		m_empty[2]?0xFFF:m_state[2]);
}

///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====

CallToken::~CallToken(void)
{
	auto end_it = m_parameters.end();
	for (auto it = m_parameters.begin(); it != end_it; ++it)
	{
		delete it->m_token_val;
	}
}

bool CallToken::ParseToken(BYTE*& begin, BYTE* end, BYTE* start)
{	// 解析函数调用，未去掉了Call Token
	begin++; // consume the call token
	// 解析 invoking
	MidAtomToken* t1 = MidAtomToken::ParseAtomToken(begin, end, start);
	if (t1 == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing atom offset=%zd", begin - start);
	if (t1->m_len > sizeof(UINT64))
		THROW_ERROR(ERR_APP, L"length of invoking id (%zd) is larger than UNIT64", t1->m_len);
	m_invoking_id = t1->GetValue<UINT64>();
	delete t1;
	t1 = nullptr;
	const UID_INFO * uid_info = g_uid_map.GetUidInfo(m_invoking_id);

	// ON Invoking
	if (uid_info)
	{
		if ((uid_info->m_class & UID_INFO::Involing) == 0) THROW_ERROR(ERR_APP,
			L"UID:%016llX (%s) is not a invoking, offset=%zd", m_invoking_id, uid_info->m_name.c_str(),
			begin - start);
		m_invoking_name = uid_info->m_name;
	}
	else
	{
		wchar_t str[100];
		swprintf_s(str, L"Unknown(%016llX)", m_invoking_id);
		m_invoking_name = str;
	}

	// 解析 method
	t1 = MidAtomToken::ParseAtomToken(begin, end, start);
	if (t1 == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing atom offset=%zd", begin - start);

	//t2.Parse(begin, end, start);
	if (t1->m_len > sizeof(UINT64))
		THROW_ERROR(ERR_APP, L"length of method id (%zd) is larger than UNIT64", t1->m_len);
	m_method_id = t1->GetValue<UINT64>();
	delete t1;
	t1 = nullptr;
	uid_info = g_uid_map.GetUidInfo(m_method_id);

	// OnMethod
	if (uid_info)
	{
		if ((uid_info->m_class & UID_INFO::Method)==0) THROW_ERROR(ERR_APP,
			L"UID:%016llX (%s) is not a method, offset=%zd", m_method_id, uid_info->m_name.c_str(),
			begin - start);
		m_method_name = uid_info->m_name;
		METHOD_INFO* method_info = uid_info->m_method;
		CONSUME_TOKEN(begin, end, start, START_LIST);
	// 解析 parameter
		ParseRequestedParameter(method_info->m_required_param, begin, end, start);
	// 解析 optional parameter
		ParseOptionalParameter(method_info->m_option_param, begin, end, start);
	}
	else
	{
		wchar_t str[100];
		swprintf_s(str, L"Unknown(%016llX)", m_method_id);
		m_method_name = str;
		if (*begin != START_LIST) THROW_ERROR(ERR_APP, L"expected start list token, offset=%zd, val=0x%02X", (begin - start), *begin);
		begin++;

		ParseParameter(begin, end, start);
	}
	CONSUME_TOKEN(begin, end, start, END_LIST);
	CONSUME_TOKEN(begin, end, start, END_OF_DATA);
	// <TODO> 解析 state
	m_state.ParseToken(begin, end, start);
	return true;
}

bool CallToken::ParseRequestedParameter(PARAM_INFO::PARAM_LIST & param_list, BYTE* &begin, BYTE* end, BYTE* start)
{
	// Check "START LIST"
	size_t param_size = param_list.size();
	for (size_t ii = 0; ii < param_size; ++ii)
	{
		//if (*begin == END_LIST)
		//{
		//	LOG_ERROR(L"[err] missing requested parameter");
		//	return false;
		//}
		PARAM_INFO& param_info = param_list[ii];
		CParameter param;
		param.m_num = param_info.m_num;
		param.m_name = param_info.m_name;
		param.m_type = param_info.m_type_id;
		// <TODO> 根据不同的type解析参数
		if (*begin == END_LIST)
		{
			param.m_type = PARAM_INFO::Err_MissingRequest;
			param.m_token_val = nullptr;
		}
		else
		{
			param.m_token_val = CTcgTokenBase::Parse(begin, end, start);
			if (param.m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - start);
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
		CParameter param;
		param.m_type = PARAM_INFO::OtherType;
		wchar_t str[100];
		if (*begin == START_NAME)
		{	// optianl param
			CONSUME_TOKEN(begin, end, start, START_NAME);
			// parse name
			MidAtomToken* name = MidAtomToken::ParseAtomToken(begin, end, start);
			if (name == nullptr) THROW_ERROR(ERR_APP, L"failed on parse AtomToken, offset=%zd", (begin - start));
//			DWORD param_num = boost::numeric_cast<DWORD>(name->GetValue());
			if (name->m_len > sizeof(DWORD)) 
				THROW_ERROR(ERR_APP, L"length of param number (%zd) is larger than DWORD", name->m_len);
			DWORD param_num = name->GetValue<DWORD>();
			delete name;
			param.m_num = param_num;
			swprintf_s(str, L"OPTION_PARAM[%d]", param_num);
			// parse value
			param.m_token_val = Parse(begin, end, start);
			if (param.m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - start);
			CONSUME_TOKEN(begin, end, start, END_NAME);
		}
		else
		{	// requested param
			swprintf_s(str, L"REQUESTED_PARAM[%d]", requested);
			param.m_num = requested;
			requested++;
			// parse value
			param.m_token_val = Parse(begin, end, start);
			if (param.m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - start);
		}
		param.m_name = str;
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
		CONSUME_TOKEN(begin, end, start, START_NAME);
		// parse name
		MidAtomToken* name = MidAtomToken::ParseAtomToken(begin, end, start);
		if (name == nullptr) THROW_ERROR(ERR_APP, L"failed on parse AtomToken, offset=%zd", (begin - start));
//		DWORD param_num = boost::numeric_cast<DWORD>(name->GetValue());
		if (name->m_len > sizeof(DWORD))
			THROW_ERROR(ERR_APP, L"length of param num (%zd) is larger than DWORD", name->m_len);
		DWORD param_num = name->GetValue<DWORD>();
		delete name;
		CParameter param;
		param.m_num = param_num;

		if (param_num >= param_list.size())
		{	
			THROW_ERROR(ERR_APP, L"illeagle param name (%lld), offset=%zd", param_num, (begin - start));
		}
		else
		{	// parse value
			PARAM_INFO& param_info = param_list[param_num];
			param.m_name = param_info.m_name;
			param.m_type = param_info.m_type_id;
		}
		param.m_token_val = Parse(begin, end, start);
		if (param.m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - start);
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
		//fwprintf_s(ff, L"\t %s=", it->m_name.c_str());
		//if (it->m_token_val) it->m_token_val->Print(ff, 0);
		//fwprintf_s(ff, L"\n");
		it->Print(ff, indentation);
	}
	fwprintf_s(ff, L"]\n");
	m_state.Print(ff, indentation);
}

void CallToken::CParameter::Print(FILE* ff, int indentation)
{
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
		if (mt->m_len > sizeof(UINT64))
			THROW_ERROR(ERR_APP, L"length of uid (%zd) is larger than UINT64", mt->m_len);
		UINT64 uid = mt->GetValue<UINT64>();
		const UID_INFO* uid_info = g_uid_map.GetUidInfo(uid);
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

///////////////////////////////////////////////////////////////////////////////
//// ==== Uid Map ====

void CUidMap::LoadParameter(boost::property_tree::wptree& param_pt, std::vector<PARAM_INFO>& param_list)
{
	auto end_it = param_pt.end();
	for (auto it = param_pt.begin(); it != end_it; ++it)
	{
		PARAM_INFO param;
		boost::property_tree::wptree& pt = (*it).second;
		param.m_num = pt.get<DWORD>(L"Number");
		param.m_name = pt.get<std::wstring>(L"Name");
		param.m_type = pt.get<std::wstring>(L"Type");
		if (param.m_type == L"uidref") param.m_type_id = PARAM_INFO::UidRef;
		else param.m_type_id = PARAM_INFO::OtherType;
		param_list.push_back(param);
	}
}

bool CUidMap::Load(const std::wstring& fn)
{
	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, fn);

	boost::property_tree::wptree root_pt;
	boost::property_tree::read_json(str_fn, root_pt);
	// Load UIDs
	boost::property_tree::wptree& uids_pt = root_pt.get_child(L"UIDs");
	auto end_it = uids_pt.end();
	for (auto it = uids_pt.begin(); it != end_it; ++it)
	{
		UID_INFO uinfo;
		std::wstring& str_uid = (*it).second.get<std::wstring>(L"UID");
		uinfo.m_uid = StringToUid(str_uid);
		uinfo.m_name = (*it).second.get<std::wstring>(L"Name");
		std::wstring& str_class = (*it).second.get<std::wstring>(L"class");
		uinfo.m_class = StringToClass(str_class);
		uinfo.m_method = nullptr;
		m_map.insert(std::make_pair(uinfo.m_uid, uinfo));
	}

	// Load method defines
	boost::property_tree::wptree& methods_pt = root_pt.get_child(L"Methods");
	end_it = methods_pt.end();
	for (auto it = methods_pt.begin(); it != end_it; ++it)
	{
		boost::property_tree::wptree& pt = (*it).second;
		UINT64 uid = StringToUid(pt.get<std::wstring>(L"UID"));
		METHOD_INFO* method = new METHOD_INFO;
		UID_INFO* uidinfo = GetUidInfo(uid);
		if (uidinfo)
		{
			uidinfo->m_method = method;
		}
		//		if (uidinfo == nullptr)
		else
		{
			UID_INFO _uidinfo;
			LOG_NOTICE(L"add new uid=%016llX, ", uid);
			_uidinfo.m_uid = uid;
			_uidinfo.m_name = pt.get<std::wstring>(L"Name");
			_uidinfo.m_class = UID_INFO::Method;
			_uidinfo.m_method = method;
			m_map.insert(std::make_pair(uid, _uidinfo));
		}

		method->m_id = uid;
		auto req_param = pt.get_child_optional(L"Requested");
		if (req_param) LoadParameter((*req_param), method->m_required_param);
		auto opt_param = pt.get_child_optional(L"Optional");
		if (opt_param)	LoadParameter((*opt_param), method->m_option_param);
	}

	return true;
}

UINT64 CUidMap::StringToUid(const std::wstring& str)
{
	UINT64 val = 0;
	const wchar_t* ch = str.c_str();
	for (size_t ii = 0; ii < 8; ++ii)
	{
		val <<= 8;
		int dd;
		swscanf_s(ch, L"%X", &dd);
		val |= dd;
		ch += 3;
	}
	return val;
}

UID_INFO::UIDCLASS CUidMap::StringToClass(const std::wstring& str)
{
	UINT cc = 0;
	wchar_t* context = 0;
	wchar_t* src = const_cast<wchar_t*>(str.c_str());
	
	wchar_t* token = wcstok_s(src, L";", &context);
	while (token)
	{
		//if (token == nullptr) break;
		if (0) {}
		else if (wcscmp(token, L"invoking") == 0) cc |= UID_INFO::Involing;
		else if (wcscmp(token, L"method") == 0) cc |= UID_INFO::Method;
		else if (wcscmp(token, L"object") == 0) cc |= UID_INFO::Object;
		token = wcstok_s(NULL, L";", &context);
	}
	return (UID_INFO::UIDCLASS)(cc);
}

const UID_INFO* CUidMap::GetUidInfo(UINT64 uid) const
{
	auto it = m_map.find(uid);
	if (it == m_map.end()) return nullptr;
	else return &(it->second);
}

UID_INFO* CUidMap::GetUidInfo(UINT64 uid)
{
	auto it = m_map.find(uid);
	if (it == m_map.end()) return nullptr;
	else return &(it->second);
	//if (it->first) return &(it->second);
	//else return nullptr;
}

CUidMap::~CUidMap(void)
{
	m_map.clear();
}

void CUidMap::Clear(void)
{
	auto end_it = m_map.end();
	for (auto it = m_map.begin(); it != end_it; ++it)
	{
		delete it->second.m_method;
	}
}

#include "pch.h"

#include "tcg_token.h"

LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_DEBUGINFO);

static const wchar_t* _INDENTATION = L"                              ";
static const wchar_t* INDENTATION = _INDENTATION + wcslen(_INDENTATION) - 1;

///////////////////////////////////////////////////////////////////////////////
// ==== CTcgTokenBase ======

CTcgTokenBase* CTcgTokenBase::Parse(BYTE*& begin, BYTE* end)
{
	CTcgTokenBase* token = nullptr;
	//enum {
	//	TokenHeader, TokenData,
	//} state;
	//state = TokenHeader;

	//if (state == TokenHeader)
	//{
		if (*begin <= 0x7F)
		{
			ShortAtomToken* tt = new ShortAtomToken;
			LOG_DEBUG(L"parsing tiny atom token");
			tt->SetTinyValue(*begin & 0x3F);
			begin++;
			token = static_cast<CTcgTokenBase*>(tt);
		}
		else if (*begin <= 0xE3)
		{
			MidAtomToken* tt = new MidAtomToken;
			tt->ParseToken(begin, end);
			token = static_cast<CTcgTokenBase*>(tt);
		}
		else
		{
			switch (*begin)
			{
			case 0xF0:	// START_LIST;
				token = static_cast<CTcgTokenBase*>(new ListToken(CTcgTokenBase::List));
				begin++;
				token->ParseToken(begin, end);
				break;

			case 0xF1:	// END_LIST;
				begin++;
				THROW_ERROR(ERR_APP, L"unexpected end list token");
				break;

			case 0xF2:	// START_NAME;
				token = static_cast<CTcgTokenBase*>(new NameToken);
				begin++;
				token->ParseToken(begin, end);
				break;
			case 0xF3:	// END_NAME;
				begin++;
				THROW_ERROR(ERR_APP, L"unexpected end name token");
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
				THROW_ERROR(ERR_APP, L"not support for transaction");
				break;
			case 0xFF:	// EMPTY;
				token = new CTcgTokenBase(CTcgTokenBase::Empty);
				begin++;
				break;

			default:
				THROW_ERROR(ERR_APP, L"unknown or reserved token: 0x%02X", *begin);
				break;
			}
		}
		return token;
	//}
	//return nullptr;
}


void CTcgTokenBase::Print(int indetation)
{
	Print(stdout, indetation);
}

void CTcgTokenBase::Print(FILE* ff, int indentation)
{
	switch (m_type)
	{
	//case BinaryAtom:
	//case IntegerAtom:
	//	fwprintf_s(ff, L"Atom:");
	//	break;

	//case List:	JCASSERT(0); break;
	//case Name:	JCASSERT(0); break;
		//fwprintf_s(ff, L"%s<NAME >\n", INDENTATION - indentation);
		//break;
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

void ShortAtomToken::SetTinyValue(BYTE val)
{
	m_val = val;
}

void ShortAtomToken::Print(FILE* ff, int indentation)
{
	fwprintf_s(ff, L"%s<ATOM >: 0x%llX\n", INDENTATION - indentation, m_val);
}

bool MidAtomToken::ParseToken(BYTE*& begin, BYTE* end)
{	// 解析所有长度大于 8 字节的atom token。
	// 如果长度小于等于8， 返回后转换成小token。
	bool binary = false;
	bool signe_int = false;
	BYTE token = begin[0];

	if ((token & 0xC0) == 0x80)
	{
		LOG_DEBUG(L"parsing short atom token");
		binary = ((token & 0x20) != 0);
		signe_int = ((token & 0x10) != 0);
		m_len = (token) & 0x0F;
		begin++;
	}
	else if ((token & 0xE0) == 0xC0)
	{
		LOG_DEBUG(L"parsing medium atom token");
		binary = ((token & 0x10) != 0);
		signe_int = ((token & 0x08) != 0);
		size_t hi = token & 0x07;
		m_len = MAKEWORD(begin[1], hi);
		begin += 2;
	}
	else if ((token & 0xF0) == 0xE0)
	{
		LOG_DEBUG(L"parsing long atom token");
		binary = ((token & 0x02) != 0);
		signe_int = ((token & 0x01) != 0);
		m_len = MAKELONG(MAKEWORD(begin[3], begin[2]), begin[1]);
		begin += 4;
	}

	//	if (binary == 0) THROW_ERROR(ERR_APP, L"unsupport integer atom, token=%02X", token);
	if (!binary)	m_type = CTcgTokenBase::IntegerAtom;
	else			m_type = CTcgTokenBase::BinaryAtom;

	if (signe_int == 1) THROW_ERROR(ERR_APP, L"unspport continue = 1");

	m_data = new BYTE[m_len];
	for (size_t ii = 0; ii < m_len; ++ii, ++begin)
	{
		if (begin >= end) THROW_ERROR(ERR_APP, L"unexpected end of data");
		//		m_data[m_len - ii - 1] = *begin;
		m_data[ii] = *begin;
	}

	return true;
}

void MidAtomToken::Print(FILE* ff, int indentation)
{
	fwprintf_s(ff, L"%s<ATOM %s>: ", INDENTATION - indentation,
		m_type == CTcgTokenBase::BinaryAtom ? L"BIN" : L"INT");
	for (size_t ii = 0; ii < m_len; ++ii)
	{
		fwprintf_s(ff, L"%02X ", m_data[ii]);
	}
	fwprintf_s(ff, L"\n");
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

bool ListToken::ParseToken(BYTE*& begin, BYTE* end)
{
	while (begin < end)
	{
		if (*begin == 0xF1)
		{	// end of list
			begin++;
			break;
		}
		CTcgTokenBase* token = CTcgTokenBase::Parse(begin, end);
		if (token == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data (%02X, %02X, ..)", begin[0], begin[1]);
		m_tokens.push_back(token);
	}
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

CTcgTokenBase* ListToken::Begin()
{
	m_cur_it = m_tokens.begin();
	return (*m_cur_it);
}

///////////////////////////////////////////////////////////////////////////////
// ==== Name Tokens ======

NameToken::~NameToken(void)
{
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
		delete (*it);
	}
}

bool NameToken::ParseToken(BYTE*& begin, BYTE* end)
{
	while (begin < end)
	{
		if (*begin == 0xF3)
		{	// end of list
			begin++;
			break;
		}
		CTcgTokenBase* token = CTcgTokenBase::Parse(begin, end);
		if (token == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing data (%02X, %02X, ..)", begin[0], begin[1]);
		m_tokens.push_back(token);
	}
	return true;
}

void NameToken::Print(FILE* ff, int indentation)
{
	fwprintf_s(ff, L"%s<BEGIN NAME>\n", INDENTATION - indentation);
	indentation += 2;
	auto end_it = m_tokens.end();
	for (auto it = m_tokens.begin(); it != end_it; ++it)
	{
		JCASSERT(*it);
		(*it)->Print(ff, indentation);
	}
	indentation -= 2;
	fwprintf_s(ff, L"%s<END NAME>\n", INDENTATION - indentation);
}


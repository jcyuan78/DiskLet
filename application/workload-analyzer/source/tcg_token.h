#pragma once

#include <vector>

class CTcgTokenBase
{
public:
	enum TokenType
	{
		BinaryAtom, IntegerAtom, List, Name, Call, Transaction, EndOfData, EndOfSession, Empty, TopToken, UnknownToken,
	};
	CTcgTokenBase(void) : m_type(UnknownToken) {}
	CTcgTokenBase(TokenType type) : m_type(type) {};
	virtual ~CTcgTokenBase(void) {};

public:
	static CTcgTokenBase* Parse(BYTE*& begin, BYTE* end);
	virtual bool ParseToken(BYTE*& begin, BYTE* end) { return false; }
	virtual void Print(int indetation);
	virtual void Print(FILE* ff, int indentation);
	TokenType GetType(void) { return m_type; }

	// 遍历
	virtual CTcgTokenBase* Begin() {return nullptr; }
	CTcgTokenBase* Font();
	CTcgTokenBase* Next();

public:
	TokenType m_type;

	// 用于遍历
	std::vector<CTcgTokenBase*>::iterator m_cur_it;
};

class ShortAtomToken : public CTcgTokenBase
{
public:
	ShortAtomToken(void) : CTcgTokenBase(CTcgTokenBase::BinaryAtom) {};

public:
	void SetTinyValue(BYTE val);
	//virtual bool ParseToken(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);

protected:
	union {
		BYTE m_data[15];
		UINT64 m_val;
	};
};

class MidAtomToken : public CTcgTokenBase
{
public:
	MidAtomToken(void) : CTcgTokenBase(CTcgTokenBase::BinaryAtom), m_len(0), m_data(nullptr) { };
	~MidAtomToken(void) { delete[] m_data; };

public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);

public:
	size_t m_len;
	BYTE* m_data;
};

class ListToken : public CTcgTokenBase
{
public:
	ListToken(CTcgTokenBase::TokenType type) : CTcgTokenBase(type) {};
	virtual ~ListToken(void);
public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);

	virtual CTcgTokenBase* Begin();

public:
	std::vector<CTcgTokenBase*> m_tokens;
};

class NameToken : public CTcgTokenBase
{
public:
	NameToken(void) : CTcgTokenBase(CTcgTokenBase::Name) {};
	~NameToken(void);

public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);

	virtual CTcgTokenBase* Begin() { return nullptr; };

public:
	std::vector<CTcgTokenBase*> m_tokens;
};



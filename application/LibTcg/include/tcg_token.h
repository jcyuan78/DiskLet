///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include <boost/cast.hpp>

#include "itcg.h"
#include "table_info.h"
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// ==== data structus ====

class CUidMap;

class CTcgTokenBase : public tcg::ISecurityObject
{
public:
	enum TokenType
	{
		BinaryAtom, IntegerAtom, List, Name, Call, Transaction, EndOfData, EndOfSession, Empty, TopToken, UnknownToken,
	};
	CTcgTokenBase(void) : m_type(UnknownToken) {}
	CTcgTokenBase(const TokenType &type) : m_type(type) {};
	virtual ~CTcgTokenBase(void) {};

public:
	enum TOKEN_ID {
		START_LIST = 0xF0, END_LIST = 0xF1,
		START_NAME = 0xF2, END_NAME = 0xF3,
		END_OF_DATA = 0xF9, END_OF_SESSION = 0xFA,
		TOKEN_CALL = 0xF8,
		START_TRANSACTION = 0xFB, END_TRANSACTION = 0xFC,
		EMPTY_TOKEN = 0xFF,
	};

public:
	static void SetUidMap(CUidMap* map);
	static CTcgTokenBase* Parse(BYTE*& begin, BYTE* end, BYTE * start);
	static CTcgTokenBase* SyntaxParse(BYTE*& begin, BYTE* end, BYTE* start, bool receive);
	/// <summary> 解析Token </summary>
	/// <param name="begin"> [IN/OUT] 开始解析位置 </param>
	/// <param name="end"> stream的结束位置 </param>
	/// <param name="end"> stream的原始位置，用于显示错误信息 </param>
	/// <returns></returns>
	bool ParseToken(BYTE*& begin, BYTE* end, BYTE* start);
	virtual bool InternalParse(BYTE*& begin, BYTE* end) { JCASSERT(0); return false; }
	virtual void Print(int indetation);
	virtual void Print(FILE* ff, int indentation);
	size_t GetPayloadLen(void) { return m_payload_len; }
	TokenType GetType(void) { return m_type; }

	// interface for ISecurityObject
	///<param name="opt"> 最高字节为indenttation</param>
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop)	{	}
	virtual void GetPayload(jcvos::IBinaryBuffer*& data, int index);
	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name) {};

	virtual size_t Encode(BYTE* buf, size_t buf_len) { return 0; }

public:
	TokenType m_type;
	BYTE* m_payload_begin;
	size_t m_payload_len;
	std::wstring m_name;

	// 用于遍历
	//std::vector<CTcgTokenBase*>::iterator m_cur_it;
};

class MidAtomToken : public CTcgTokenBase
{
public:
	MidAtomToken(void) : CTcgTokenBase(CTcgTokenBase::BinaryAtom), m_len(0), m_data(NULL) {};
	~MidAtomToken(void) { if (m_len>8 && m_data) delete[] m_data; };

public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);
	// interface for ISecurityObject
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual size_t Encode(BYTE* buf, size_t buf_len);

	UINT64 FormatToInt(void) const;
	bool FormatToString(std::wstring& str)const ;
	bool FormatToByteString(std::wstring& str) const;
	template <typename T>
	bool GetValue(T& out) const
	{
		if (m_len > 8) return false;
		out = boost::numeric_cast<T>(FormatToInt());
		return true;
	}

	static MidAtomToken* ParseAtomToken(BYTE*& begin, BYTE* end, BYTE* start)
	{
//		MidAtomToken* tt = new MidAtomToken;
		MidAtomToken* tt = jcvos::CDynamicInstance<MidAtomToken>::Create();
		tt->ParseToken(begin, end, start);
		return tt;
	}

	size_t ToBuffer(BYTE* buf, size_t buf_size);

public:
	static MidAtomToken * CreateToken(const std::wstring& str);
	static MidAtomToken* CreateToken(UINT64 val);
	static MidAtomToken* CreateToken(const BYTE* data, size_t data_len);
	static MidAtomToken* CreateToken(const HUID& data);
	static MidAtomToken* CreateToken(const TCG_UID & data);

public:
	size_t m_len;
	union
	{
		BYTE* m_data;
		BYTE  m_data_val[8];
//		UINT64 m_val;
	};
	bool m_signe;
};

class ListToken : public CTcgTokenBase
{
public:
	ListToken(const CTcgTokenBase::TokenType &type) : CTcgTokenBase(type) {};
	ListToken(void) { }
	virtual ~ListToken(void);
public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);
	// interface for ISecurityObject
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name);
	virtual size_t Encode(BYTE* buf, size_t buf_len);


// == local usage ==
	// TcgToken虽然使用IJCInterface，但是不适用引用计数。
	static ListToken* CreateToken(void) 
	{
		ListToken * list = jcvos::CDynamicInstance<ListToken>::Create(); 
		list->m_type = CTcgTokenBase::List;
		return list;
	}
	void AddToken(CTcgTokenBase* tt) { m_tokens.push_back(tt); tt->AddRef(); }
	friend class CSecurityParser;
	friend class CTcgSession;
protected:
	CTcgTokenBase* GetSubToken(int index)
	{
		CTcgTokenBase* sub = nullptr;
		if (index < m_tokens.size())
		{
			sub = m_tokens[index];
			//if (sub) sub->AddRef();
		}
		return sub;
	}
public:
	std::vector<CTcgTokenBase*> m_tokens;
};

//// ==== Name Token ==================================================================================================
/// <summary>
/// 
/// </summary>
class NameToken : public CTcgTokenBase
{
public:
	NameToken(void) : CTcgTokenBase(CTcgTokenBase::Name), m_name_id(nullptr), m_value(nullptr) {};
	~NameToken(void);
public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);
	// interface for ISecurityObject
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name) 
	{
		if (m_value) m_value->GetSubItem(sub_item, name);
	};
	virtual size_t Encode(BYTE* buf, size_t buf_len);

	template <typename T> T GetName(void)
	{
		JCASSERT(m_name_id);
		T name;
		m_name_id->GetValue<T>(name);
		return name;
	}

public:
//	static NameToken * CreateToken(const std::wstring& name, UINT val);
	static NameToken * CreateToken(UINT id, CTcgTokenBase * val);
	static NameToken* CreateToken(UINT id, const BYTE* data, size_t data_len);
//	static NameToken* CreateToken(UINT id, UINT64 val);
	template <typename T1, typename T2>
	static NameToken* CreateToken(const T1& name, const T2& val)
	{
		NameToken* token = jcvos::CDynamicInstance<NameToken>::Create();
		if (!token) THROW_ERROR(ERR_MEM, L"failed on creating NameToken");
		token->m_name_id = MidAtomToken::CreateToken(name);
		token->m_value = MidAtomToken::CreateToken(val);
		return token;
	}

public:
	MidAtomToken* m_name_id = nullptr;
	CTcgTokenBase* m_value = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
class CStatePhrase : public CTcgTokenBase
{
public:
	CStatePhrase(void) : CTcgTokenBase(CTcgTokenBase::List) 
	{
		memset(m_empty, 0, sizeof(UINT) * 3); 
		memset(m_state, 0, sizeof(UINT) * 3); 
		m_name = L"state";
	};
	~CStatePhrase(void) {};

public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);
	// interface for ISecurityObject
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

public:
	WORD getState(void) const { return (WORD)(m_empty[0]?0xFFF:m_state[0]); }

protected:
	UINT m_empty[3];
	UINT m_state[3];
};

class CErrorToken : public CTcgTokenBase
{

	virtual bool InternalParse(BYTE*& begin, BYTE* end) { begin = end; return true; }

	virtual void Print(FILE* ff, int indentation) {}
	virtual void ToString(std::wostream& out, UINT layer, int opt) 
	{
		out << L"[err] " << m_error_msg << std::endl;
	}
	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name)
	{
	}

public:
	std::wstring m_error_msg;
};

class CParameterToken : public CTcgTokenBase
{
public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end)
	{
		m_token_val = CTcgTokenBase::Parse(begin, end, m_payload_begin);
		if (m_token_val == nullptr) THROW_ERROR(ERR_APP, L"failed on parsing token, offset=%zd", begin - m_payload_begin);
		return true;
	}
	virtual void Print(FILE* ff, int indentation);
	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name)
	{
		if (name == L"")
		{
			sub_item = m_token_val;
			if (sub_item) sub_item->AddRef();
		}
		else if (m_token_val) m_token_val->GetSubItem(sub_item, name);
	}

public:
	PARAM_INFO::PARAM_TYPE m_type;
	DWORD m_num;
	UINT64 m_atom_val;
	CTcgTokenBase* m_token_val;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
class CallToken : public CTcgTokenBase
{
public:
	CallToken(void) : CTcgTokenBase(CTcgTokenBase::Call), m_state(NULL){};
	~CallToken(void);

public:
	virtual bool InternalParse(BYTE*& begin, BYTE* end);
	virtual void Print(FILE* ff, int indentation);

	virtual void ToString(std::wostream& out, UINT layer, int opt);
	virtual void ToProperty(boost::property_tree::wptree& prop);

	virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name);


//	virtual CTcgTokenBase* Begin() { return nullptr; };
protected:
	bool ParseRequestedParameter(PARAM_INFO::PARAM_LIST& param_list, BYTE* &begin, BYTE* end, BYTE* start);
	bool ParseOptionalParameter(PARAM_INFO::PARAM_LIST& param_list , BYTE* &begin, BYTE* end, BYTE* start);
	/// <summary>	解析为定义method的参数，包括requested和optional </summary>
	/// <param name="begin"></param>
	/// <param name="end"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	bool ParseParameter(BYTE* &begin, BYTE* end, BYTE* start);

protected:
	UINT64 m_invoking_id;
	std::wstring m_invoking_name;
	UINT64 m_method_id;
	std::wstring m_method_name;
	std::vector<CTcgTokenBase*> m_parameters;
	CStatePhrase *m_state;
};




//extern CUidMap * g_uid_map;

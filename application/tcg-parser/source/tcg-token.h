#pragma once

#include <vector>
#include <boost/property_tree/json_parser.hpp>

///////////////////////////////////////////////////////////////////////////////
//// ==== data structus ====
class PARAM_INFO
{
public:
	typedef std::vector<PARAM_INFO> PARAM_LIST;
	enum PARAM_TYPE {
		OtherType, 
		Err_MissingRequest,	// dummy type: 表示错误，遗漏了相应的requested parameter
		UidRef, 
	};
public:
	DWORD m_num;
	PARAM_TYPE m_type_id;
	std::wstring m_name;
	std::wstring m_type;
};

class METHOD_INFO
{
public:
	UINT64 m_id;
	std::vector<PARAM_INFO> m_required_param;
	std::vector<PARAM_INFO> m_option_param;
};

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
	enum TOKEN_ID {
		START_LIST = 0xF0, END_LIST = 0xF1,
		START_NAME = 0xF2, END_NAME = 0xF3,
		END_OF_DATA = 0xF9, END_OF_SESSION = 0xFA,
		TOKEN_CALL = 0xF8,
		EMPTY_TOKEN = 0xFF,
	};

public:
	static CTcgTokenBase* Parse(BYTE*& begin, BYTE* end, BYTE * start);
	static CTcgTokenBase* SyntaxParse(BYTE*& begin, BYTE* end, BYTE* start, bool receive);
	/// <summary> 解析Token </summary>
	/// <param name="begin"> [IN/OUT] 开始解析位置 </param>
	/// <param name="end"> stream的结束位置 </param>
	/// <param name="end"> stream的原始位置，用于显示错误信息 </param>
	/// <returns></returns>
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE * start) { return false; }
	virtual void Print(int indetation);
	virtual void Print(FILE* ff, int indentation);
	TokenType GetType(void) { return m_type; }

public:
	TokenType m_type;

	// 用于遍历
	//std::vector<CTcgTokenBase*>::iterator m_cur_it;
};

class MidAtomToken : public CTcgTokenBase
{
public:
	MidAtomToken(void) : CTcgTokenBase(CTcgTokenBase::BinaryAtom), m_len(0), m_data(NULL) { m_val = 0; };
	~MidAtomToken(void) { delete[] m_data; };

public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE * start);
	virtual void Print(FILE* ff, int indentation);

	//UINT64 GetValue(void) const;
	template <typename T> T GetValue(void) const
	{
		//if (m_bytes)	
		//	THROW_ERROR(ERR_APP, L"Atom is not a integer");
		if (m_len > sizeof(T))
			THROW_ERROR(ERR_APP, L"data len (%zd) is longer than type (%d)", m_len, sizeof(T));
		return boost::numeric_cast<T>(m_val);
	}

	static MidAtomToken* ParseAtomToken(BYTE*& begin, BYTE* end, BYTE* start)
	{
		MidAtomToken* tt = new MidAtomToken;
		tt->ParseToken(begin, end, start);
		return tt;
	}

	void ToString(std::wstring& str);

public:
	bool m_bytes;
	bool m_sign;
	size_t m_len;
	BYTE* m_data;
	UINT64 m_val;
#ifdef _DEBUG
	BYTE m_token;
#endif
};

class ListToken : public CTcgTokenBase
{
public:
	ListToken(CTcgTokenBase::TokenType type) : CTcgTokenBase(type) {};
	virtual ~ListToken(void);
public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE* start);
	virtual void Print(FILE* ff, int indentation);
	void AddToken(CTcgTokenBase* tt) { m_tokens.push_back(tt); }
public:
	std::vector<CTcgTokenBase*> m_tokens;
};

class NameToken : public CTcgTokenBase
{
public:
	NameToken(void) : CTcgTokenBase(CTcgTokenBase::Name), m_value(nullptr), m_name(NULL) {};
	~NameToken(void);
public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE* start);
	virtual void Print(FILE* ff, int indentation);
//	virtual CTcgTokenBase* Begin() { return nullptr; };
public:
//	DWORD m_name;
	MidAtomToken* m_name;	// 必须是整数或者字符串，不能是复合型
	CTcgTokenBase* m_value;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
class CStatePhrase : public CTcgTokenBase
{
public:
	CStatePhrase(void) : CTcgTokenBase(CTcgTokenBase::List) 
	{
		memset(m_empty, 0, sizeof(UINT) * 3); 
		memset(m_state, 0, sizeof(UINT) * 3); 
	};
	~CStatePhrase(void) {};

public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE* start);
	virtual void Print(FILE* ff, int indentation);

protected:
	UINT m_empty[3];
	UINT m_state[3];
};


///////////////////////////////////////////////////////////////////////////////
//// ==== Call Token ====
class CallToken : public CTcgTokenBase
{
	class CParameter
	{
	public:
		PARAM_INFO::PARAM_TYPE m_type;
		DWORD m_num;
		std::wstring m_name;
		UINT64 m_atom_val;
		CTcgTokenBase* m_token_val;
	public:
		void Print(FILE* ff, int indentation);
	};

public:
	CallToken(void) : CTcgTokenBase(CTcgTokenBase::Call) {};
	~CallToken(void);

public:
	virtual bool ParseToken(BYTE*& begin, BYTE* end, BYTE * start);
	virtual void Print(FILE* ff, int indentation);

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
	std::vector<CParameter> m_parameters;
	CStatePhrase m_state;
};


class UID_INFO
{
public:
//	UID_INFO(void) {};
	//~UID_INFO(void);

public:
	enum UIDCLASS {Unkonw = 0, Involing=0x01, Method=0x02, Object=0x04, Table=0x08};
public:
	UINT64 m_uid;
	std::wstring m_name;
	UIDCLASS m_class;

	METHOD_INFO* m_method;
};

class CUidMap
{
public:
	typedef std::map<UINT64, UID_INFO> UID_MAP;
	~CUidMap(void);

public:
	/// <summary> Load Uid Infors from json file </summary>
	/// <param name="fn"></param>
	/// <returns></returns>
	bool Load(const std::wstring& fn);
	void Clear(void);
	const UID_INFO* GetUidInfo(UINT64 uid) const;
	UID_INFO* GetUidInfo(UINT64 uid);

protected:
	UINT64 StringToUid(const std::wstring& str);
	UID_INFO::UIDCLASS StringToClass(const std::wstring& str);

	void LoadParameter(boost::property_tree::wptree& pt, std::vector<PARAM_INFO>& param);

protected:
	UID_MAP m_map;
	//std::map<UINT64, METHOD_INFO> m_method_map;
};

extern CUidMap g_uid_map;

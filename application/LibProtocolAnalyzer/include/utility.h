///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

inline void GetComError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);

	IErrorInfo* com_err = NULL;
	GetErrorInfo(res, &com_err);
	int pp = 0;
	BSTR disp;
	if (com_err)
	{
		com_err->GetDescription(&disp);
		pp = swprintf_s(out_msg, buf_size, L"[com err] %s; ", disp);
		com_err->Release();
		SysFreeString(disp);
	}
	else
	{
		pp = swprintf_s(out_msg, buf_size, L"[com err] unkonw error=0x%08X;", res);
	}
	vswprintf_s(out_msg + pp, buf_size - pp, msg, argptr);
}

inline void GetWmiError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...)
{
	static HMODULE module_wbem = NULL;
	if (module_wbem == nullptr) module_wbem = LoadLibrary(L"C:\\Windows\\System32\\wbem\\wmiutils.dll");

	va_list argptr;
	va_start(argptr, msg);

	IErrorInfo* com_err = NULL;
	GetErrorInfo(res, &com_err);
	int pp = 0;
	BSTR disp;
	if (com_err)
	{
		com_err->GetDescription(&disp);
		pp = swprintf_s(out_msg, buf_size, L"[com err] (0x%08X) %s; ", res, disp);
		com_err->Release();
		SysFreeString(disp);
	}
	else if (module_wbem)
	{
		LPTSTR strSysMsg;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
			module_wbem, res, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
			(LPTSTR)&strSysMsg, 0, NULL);
		pp = swprintf_s(out_msg, buf_size, L"[wmi err] (0x%8X) %s", res, strSysMsg);
		LocalFree(strSysMsg);

		//		CloseHandle(mm);
	}
	else
	{
		pp = swprintf_s(out_msg, buf_size, L"[com err] unkonw error=0x%08X;", res);
	}
	vswprintf_s(out_msg + pp, buf_size - pp, msg, argptr);
}

#define THROW_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	jcvos::CJCException err(buf, jcvos::CJCException::ERR_APP);	\
    LogException(__STR2WSTR__(__FUNCTION__), __LINE__, err);	\
    throw err;	} while(0);

#define LOG_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	LOG_ERROR(buf);	} while(0);

template <typename TYPE>
class auto_unknown
{
public:
	typedef TYPE* PTYPE;
	explicit auto_unknown(void) : m_ptr(NULL) {};
	explicit auto_unknown(TYPE* ptr) : m_ptr(ptr) {};
	~auto_unknown(void) { if (m_ptr) m_ptr->Release(); };

	operator TYPE*& () { return m_ptr; };
	operator IUnknown* () { return static_cast<IUnknown*>(m_ptr); }
	auto_unknown<TYPE>& operator = (TYPE* ptr)
	{
		JCASSERT(NULL == m_ptr);
		m_ptr = ptr;
		return (*this);
	}
	operator LPVOID* () { return (LPVOID*)(&m_ptr); }

	TYPE* operator ->() { return m_ptr; };

	PTYPE* operator &() { return &m_ptr; };
	TYPE& operator *() { return *m_ptr; };
	bool operator !()	const { return NULL == m_ptr; };
	bool operator == (const TYPE* ptr)	const { return /*const*/ ptr == m_ptr; };
	bool valid() const { return NULL != m_ptr; }
	//operator bool() const	{ return const NULL != m_ptr;};

	template <typename PTR_TYPE>
	PTR_TYPE d_cast() { return dynamic_cast<PTR_TYPE>(m_ptr); };

	void reset(void) { if (m_ptr) m_ptr->Release(); m_ptr = NULL; };

	template <typename TRG_TYPE>
	void detach(TRG_TYPE*& type)
	{
		JCASSERT(NULL == type);
		type = dynamic_cast<TRG_TYPE*>(m_ptr);
		JCASSERT(type);
		m_ptr = NULL;
	};

protected:
	TYPE* m_ptr;
};

#if 0  // move to stdex::utility.h
//#define __round_mask(x, y)	((__typeof__(x))((y)-1))
template <typename T1, typename T2>
inline T1 __round_mask(T1 x, T2 y) { return (T1)((y)-1); }

// 以y为单位，向上对齐。但是不缩小x。y必须是2的整幂
template <typename T>
inline T round_up(T x, T y) { return (((x-1) | __round_mask(x, y)) + 1); }
//#define round_up(x, y)		((((x)-1) | __round_mask(x, y))+1)
/** round_down - round down to next specified power of 2
 * @x: the value to round
 * @y: multiple to round down to (must be a power of 2)
 *
 * Rounds @x down to next multiple of @y (which must be a power of 2). To perform arbitrary rounding down, use rounddown() below. */
#define round_down(x, y)	((x) & ~__round_mask(x, y))
#endif

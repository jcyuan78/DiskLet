#pragma once

#include <WbemIdl.h>
#include <vector>

#include <initguid.h>
#include <vds.h>
#include <boost/property_tree/json_parser.hpp>

///////////////////////////////////////////////////////////////////////////////
// -- COM support functions ---------------------------------------------------

class CCloneException : public jcvos::CJCException
{
public: 
	CCloneException(UINT code, const wchar_t* msg) : jcvos::CJCException(msg, ERR_APP, code)
	{}
};
#define WSTR_GUID_FMT L"{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"

#define GUID_PRINTF_ARG( X )                                \
    (X).Data1,                                              \
    (X).Data2,                                              \
    (X).Data3,                                              \
    (X).Data4[0], (X).Data4[1], (X).Data4[2], (X).Data4[3], \
    (X).Data4[4], (X).Data4[5], (X).Data4[6], (X).Data4[7]

void GetComError(wchar_t * out_msg, size_t buf_size, HRESULT res, const wchar_t * msg, ...);
void GetWmiError(wchar_t * out_msg, size_t buf_size, HRESULT res, const wchar_t * msg, ...);

#ifndef THROW_COM_ERROR
#define THROW_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	jcvos::CJCException err(buf, jcvos::CJCException::ERR_APP);	\
    LogException(__STR2WSTR__(__FUNCTION__), __LINE__, err);	\
    throw err;	} while(0);
#endif

#ifndef LOG_COM_ERROR
#define LOG_COM_ERROR(res, msg, ...)	do {\
	jcvos::auto_array<wchar_t> buf(256);	\
	GetWmiError(buf, 256, res, msg, __VA_ARGS__);	\
	LOG_ERROR(buf);	} while(0);

#endif // !LOG_COM_ERROR


#define _1MB (1024*1024)

#define CLONE_ERROR(code, msg, ...) do {	\
	jcvos::auto_array<wchar_t> buf(256);		\
	swprintf_s(buf.get_ptr(), buf.len(), L"[err %04d] line=%d " msg, code, __LINE__, __VA_ARGS__); \
	LOG_ERROR(buf.get_ptr()); /*return (code);*/ \
	CCloneException err(code, buf); throw err; } while (0)


template <typename TYPE>
class auto_unknown
{
public:
	typedef TYPE * PTYPE;
	explicit auto_unknown(void) : m_ptr(NULL) {};
	explicit auto_unknown(TYPE * ptr) : m_ptr(ptr) {};
	~auto_unknown(void) { if (m_ptr) m_ptr->Release(); };

	operator TYPE* &() { return m_ptr; };
	operator IUnknown * () { return static_cast<IUnknown*>(m_ptr); }
	auto_unknown<TYPE> & operator = (TYPE * ptr)
	{
		JCASSERT(NULL == m_ptr);
		m_ptr = ptr;
		return (*this);
	}
	operator LPVOID *() { return (LPVOID*)(&m_ptr); }

	TYPE * operator ->() { return m_ptr; };

	PTYPE * operator &() { return &m_ptr; };
	TYPE & operator *() { return *m_ptr; };
	bool operator !()	const { return NULL == m_ptr; };
	bool operator == (const TYPE * ptr)	const { return /*const*/ ptr == m_ptr; };
	bool valid() const { return NULL != m_ptr; }
	//operator bool() const	{ return const NULL != m_ptr;};

	template <typename PTR_TYPE>
	PTR_TYPE d_cast() { return dynamic_cast<PTR_TYPE>(m_ptr); };

	void reset(void) { if (m_ptr) m_ptr->Release(); m_ptr = NULL; };

	template <typename TRG_TYPE>
	void detach(TRG_TYPE * & type)
	{
		JCASSERT(NULL == type);
		type = dynamic_cast<TRG_TYPE*>(m_ptr);
		JCASSERT(type);
		m_ptr = NULL;
	};

protected:
	TYPE * m_ptr;
};

template <class T>
class ObjectList : public std::vector<T*>
{
public:
	~ObjectList<T>(void) { ClearObj(); }
	void ClearObj(void)
	{
		for (auto ii = begin(); ii != end(); ++ii)	if (*ii) (*ii)->Release();
		clear();
	}
};


template <class OBJ>
bool EnumObjects(IEnumVdsObject * penum, std::vector<OBJ*> & objs)
{
	while (1)
	{
		auto_unknown<IUnknown> punknown;
		ULONG ulFetched = 0;
		HRESULT hres = penum->Next(1, &punknown, &ulFetched);
		if (hres == S_FALSE)	break;
		if (FAILED(hres) || (!punknown))	THROW_COM_ERROR(hres, L"unexpected enum value");
		OBJ * obj;
		hres = punknown->QueryInterface<OBJ>(&obj);
		if (FAILED(hres) || (!obj)) THROW_COM_ERROR(hres, L"unexpected provider");
		objs.push_back(obj);
	}
	return true;
}

void ReadWmiObject(boost::property_tree::wptree & obj_out, IWbemClassObject * obj_in);


bool GetStringValueFromQuery(std::wstring & res, IWbemServices* pIWbemServices, const wchar_t* query, const wchar_t* valuename);


///////////////////////////////////////////////////////////////////////////////
// -- General support functions -----------------------------------------------

template <class T>
void InitMem(T * ptr, int val=0)
{
	memset(ptr, val, sizeof(T));
}

size_t AllocDriveInfo(size_t partition_num, DRIVE_LAYOUT_INFORMATION_EX * & info);
size_t LayoutInfoSize(size_t partition_num);

bool WaitAsync(IVdsAsync * async, VDS_ASYNC_OUTPUT * res);
//void FreeDriveInfo(DRIVE_LAYOUT_INFORMATION_EX * info);

DWORD CalculateCRC32(DWORD * buf, size_t len);

float ByteToGb(UINT64 size);

inline float SectorToGb(UINT64 size) {
	double ss = (double)size;
	ss /= (2.0*1024.0*1024.0);
	return (float)ss;
}

inline float SectorToTb(UINT64 size) {
	double ss = (double)size;
	ss /= (2.0*1024.0*1024.0*1024.0);
	return (float)ss;
}

inline UINT64 SectorToMb(UINT64 size) {return (size >> 11);}
inline UINT64 MbToSector(UINT64 size) { return (size << 11); }
inline UINT64 MbToByte(UINT64 mb) { return (mb << 20); }

bool EnabledDebugPrivilege();

void PrintProvider(IVdsProvider * provider);

void PrintSubSystem(IVdsSubSystem * subsystem);

void PrintDrive(IVdsDrive * drive);

inline UINT64 ByteToMB(UINT64 size)
{
	// 确保能够被1MB整除。
	JCASSERT((size &(_1MB - 1)) == 0);
	return size / _1MB;
}

// 向上取整对苼E
inline UINT64 ByteToMB_Up(UINT64 size)
{
	return ((size-1) >>20) +1;
}

// 向下取整对苼E
inline UINT64 ByteToMB_Down(UINT64 size)
{
	return (size >>20);
}

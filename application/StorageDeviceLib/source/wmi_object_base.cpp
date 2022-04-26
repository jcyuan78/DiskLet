#include "pch.h"
#include "../include/utility.h"
#include "storage_manager.h"
#include "wmi_object_base.h"

#include <wbemidl.h>

LOCAL_LOGGER_ENABLE(L"wmi_obj_base", LOGGER_LEVEL_NOTICE);
namespace prop_tree = boost::property_tree;

CWmiObjectBase::~CWmiObjectBase(void)
{
	if (m_obj) m_obj->Release();
}

void CWmiObjectBase::Initialize(IWbemClassObject * obj, CStorageManager * manager)
{
	JCASSERT(obj);

	LOG_STACK_TRACE();
	m_manager = manager;

	VARIANT val;
	CIMTYPE type;
	long flavor;
	HRESULT hres = obj->Get(L"__PATH", 0, &val, &type, &flavor);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting __PATH");
	m_obj_path = val.bstrVal;
	VariantClear(&val);


	hres = obj->Get(L"__RELPATH", 0, &val, &type, &flavor);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting __PATH");
	size_t str_len = SysStringLen(val.bstrVal);
	jcvos::auto_array<wchar_t> tmp(str_len + 1, 0);
	wcsncpy_s(tmp, tmp.len(), val.bstrVal, str_len);
	_wcsupr_s(tmp, tmp.len());
	m_rel_path = (wchar_t*)tmp;
	VariantClear(&val);

	SetWmiProperty(obj);

	m_obj = obj;
	m_obj->AddRef();
}

bool CWmiObjectBase::Refresh(void)
{
	auto_unknown<IWbemClassObject> obj;
	GetObjectFromPath(obj, m_obj_path, m_manager);
	SetWmiProperty(obj);
	return true;
}

bool CWmiObjectBase::IsObject(const std::wstring & path)
{
	jcvos::auto_array<wchar_t> tmp(path.size() + 1);
	wcscpy_s(tmp, tmp.len(), path.c_str());
	_wcsupr_s(tmp, tmp.len());
	return (wcsstr(tmp, m_rel_path.c_str()) != NULL);
	//	std::wstring _path = path;
	//	return _path.find(m_rel_path, 0) > 0;
}

bool CWmiObjectBase::GetObjectFromPath(IWbemClassObject *& obj, const std::wstring & path, CStorageManager * manager)
{
	JCASSERT(manager && obj == nullptr);
	HRESULT hres = manager->m_services->GetObject(
		BSTR(path.c_str()), WBEM_FLAG_RETURN_WBEM_COMPLETE,
		NULL, &obj, NULL);
	if (FAILED(hres) || !obj) THROW_COM_ERROR(hres, L"failed on getting object");
	return true;
}

HRESULT CWmiObjectBase::InternalInvokeMethodAsync(IJCProgress *& progress, const wchar_t * name, IWbemClassObject * class_instance)
{
	JCASSERT(progress == nullptr);
	HRESULT hres;
	//auto_unknown<IWbemUnsecuredApartment> unsec_app;
	//CoCreateInstance(CLSID_WbemUnsecuredApartment, NULL, CLSCTX_LOCAL_SERVER,
	//	IID_IWbemUnsecuredApartment, unsec_app);
	//CWmiAsyncProgress * _progress = jcvos::CDynamicInstance<CWmiAsyncProgress>::Create();
	//auto_unknown<IWbemObjectSink> sink;
	//_progress->GetSink(sink);

	//LPCWSTR wszReserved = NULL;
	//auto_unknown<IWbemObjectSink> stub_sink;
	//unsec_app->CreateSinkStub(sink,
	//	WBEM_FLAG_UNSECAPP_CHECK_ACCESS,  //Authenticate callbacks regardless of registry key
	//	wszReserved, &stub_sink);
	//sink.reset();

	//hres = m_manager->m_services->ExecMethodAsync(BSTR(m_obj_path.c_str()), BSTR(name), 0, NULL, class_instance, stub_sink);
//	auto_unknown<IWbemClassObject> out_param;
	auto_unknown<IWbemCallResult> call_result;
	hres = m_manager->m_services->ExecMethod(BSTR(m_obj_path.c_str()), BSTR(name), WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL, class_instance, NULL, &call_result);
//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on invoking method (%s)", name);

	if (SUCCEEDED(hres) && !(call_result == nullptr))
	{
		CWmiSemisyncProgress * pp = jcvos::CDynamicInstance<CWmiSemisyncProgress>::Create();
		pp->SetResult(call_result);
		progress = static_cast<IJCProgress*>(pp);
	}
	return hres;
}

template <> void SetVarType<bool>(VARIANT & var, const bool & val, CIMTYPE &type)
{
	var.vt = VT_BOOL;
	var.boolVal = val;
	type = CIM_BOOLEAN;
}

typedef const wchar_t * _pwstr;

template <> void SetVarType<wchar_t const *>(VARIANT & var, wchar_t const * const & val, CIMTYPE &type)
{
	var.vt = VT_BSTR;
	var.bstrVal = SysAllocString(val);
	type = CIM_STRING;
}

template <> void SetVarType<UINT64>(VARIANT & var, const UINT64 & val, CIMTYPE &type)
{
	//var.vt = VT_UI8;
	//var.ullVal = val;
	wchar_t str_val[24];
	swprintf_s(str_val, L"%lld", val);;
	var.vt = VT_BSTR;
	var.bstrVal = SysAllocString(str_val);
	type = CIM_UINT64;
}

CJCVariant::CJCVariant(void)
{
	VariantInit(&m_var);
}

CJCVariant::~CJCVariant(void)
{
	VariantClear(&m_var);
}

template <> CJCVariant::CJCVariant(const bool & val)
{
	VariantInit(&m_var);
	m_var.vt = VT_BOOL;
	m_var.boolVal = val;
}

template <> CJCVariant::CJCVariant(const UINT16 & val)
{
	VariantInit(&m_var);
	wchar_t str_val[24];
	swprintf_s(str_val, L"%d", val);;
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(str_val);
}

template <> CJCVariant::CJCVariant(wchar_t const * const & val)
{
	VariantInit(&m_var);
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(val);
}

template <> CJCVariant::CJCVariant(const std::wstring const & val)
{
	VariantInit(&m_var);
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(val.c_str());
}

template <> CJCVariant::CJCVariant(const UINT64 & val)
{
	VariantInit(&m_var);
	wchar_t str_val[24];
	swprintf_s(str_val, L"%lld", val);;
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(str_val);
}

template<> CJCVariant::CJCVariant(const GUID & val)
{
	VariantInit(&m_var);
	wchar_t str[64];
	StringFromGUID2(val, str, 64);
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(str);
}



template <> CJCVariant & CJCVariant::operator = (const bool & val)
{
	m_var.vt = VT_BOOL;
	m_var.boolVal = val;
	return *this;
}

typedef const wchar_t * _pwstr;

template <> CJCVariant & CJCVariant::operator = (wchar_t const * const & val)
{
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(val);
	return *this;
//	type = CIM_STRING;
}

template <> CJCVariant & CJCVariant::operator = (const UINT64 & val)
{
	wchar_t str_val[24];
	swprintf_s(str_val, L"%lld", val);;
	m_var.vt = VT_BSTR;
	m_var.bstrVal = SysAllocString(str_val);
	return *this;
}


template<> UINT32 CJCVariant::val(void) const
{
	return m_var.uintVal;
}

template<> UINT64 CJCVariant::val(void) const
{
	switch (m_var.vt)
	{
	case VT_BSTR: return _wtoi64(m_var.bstrVal);
	case VT_UI8: return m_var.ullVal;

	}
	return (UINT64)(-1);
}


template<> const wchar_t * CJCVariant::val(void) const
{
	if (m_var.vt == VT_BSTR)	return m_var.bstrVal;
	else return NULL;
}


const wchar_t * CWmiObjectBase::GetMSFTErrorMsg(UINT32 err_code)
{
	switch (err_code)
	{
	case 0:		return L"Success";
	case 1:		return L"Not Supported";
	case 2:		return L"Unspecified Error";
	case 3:		return L"Timeout";
	case 4:		return L"Failed";
	case 5:		return L"Invalid Parameter";
	case 6:		return L"Disk is in use";
	case 7:		return L"This command is not supported on x86 running in x64 environment.";
	case 4097:	return L"Size Not Supported";
	case 40000:	return L"Not enough free space";
	case 40001:	return L"Access Denied";
	case 40002:	return L"There are not enough resources to complete the operation.";
	case 40004:	return L"An unexpected I / O error has occured";
	case 40005: return L"You must specify a size by using either the Size or the UseMaximumSize parameter.You can specify only one of these parameters at a time.";
	case 40018:	return L"The specified object is managed by the Microsoft Failover Clustering component.The disk must be in cluster maintenance mode and the cluster resource status must be online to perform this operation.";

	case 41000: return L"The disk has not been initialized.";
	case 41002: return L"The disk is read only";
	case 41003: return L"The disk is offline.";
	case 41006: return L"A parameter is not valid for this type of partition.";
	case 41012: return L"The specified offset is not valid";
	case 42008:	return L"Cannot shrink a partition containing a volume with errors.";
	case 42009:	return L"Cannot resize a partition containing an unknown file system.";
	case 42010:	return L"The operation is not allowed on a system or critical partition.";






			//The disk's partition limit has been reached. (41004)

			//The specified partition alignment is not valid.It must be a multiple of the disk's sector size. (41005)


			//The specified partition type is not valid. (41010)

			//Only the first 2 TB are usable on MBR disks. (41011)

			//The specified offset is not valid. (41012)

			//There is no media in the device. (41015)

			//The specified offset is not valid. (41016)

			//The specified partition layout is invalid. (41017)

			//The specified object is managed by the Microsoft Failover Clustering component.The disk must be in cluster maintenance mode and the cluster resource status must be online to perform this operation. (41018)

			//The requested access path is already in use. (42002)

			//Cannot assign access paths to hidden partitions. (42004)

			//The access path is not valid. (42007)






	//	The specified cluster size is invalid(43000)

	//	The specified file system is not supported(43001)

	//	The volume cannot be quick formatted(43002)

	//	The number of clusters exceeds 32 bits(43003)

	//	The specified UDF version is not supported(43004)

	//	The cluster size must be a multiple of the disk's physical sector size (43005)

	//	Cannot perform the requested operation when the drive is read only(43006)

	default:	return L"unkonw error";
	}



}

HRESULT __stdcall CQuerySink::Indicate(LONG lObjCount, IWbemClassObject ** pArray)
{
	for (long i = 0; i < lObjCount; i++)
	{
		IWbemClassObject *pObj = pArray[i];

		// ... use the object.

		// AddRef() is only required if the object will be held after
		// the return to the caller.
	}

	return WBEM_S_NO_ERROR;
}

HRESULT __stdcall CQuerySink::SetStatus(LONG lFlags, HRESULT hResult, BSTR strParam, IWbemClassObject * pObjParam)
{
	printf("QuerySink::SetStatus hResult = 0x%X\n", hResult);
	return WBEM_S_NO_ERROR;
}

CWmiAsyncProgress::CWmiAsyncProgress(void)
{
	m_query_sink = new CQuerySink;
	m_query_sink->AddRef();
}

CWmiAsyncProgress::~CWmiAsyncProgress(void)
{
	m_query_sink->Release();
}

CWmiSemisyncProgress::CWmiSemisyncProgress(void)
	:m_call_result(NULL)
{
	m_status = -1;	// running
	m_semi_progress = 0;
}

CWmiSemisyncProgress::~CWmiSemisyncProgress(void)
{
	if (m_call_result) m_call_result->Release();
}

int CWmiSemisyncProgress::GetProgress(void)
{
	LOG_STACK_TRACE();
	JCASSERT(m_call_result);
	long ss;
	HRESULT res = m_call_result->GetCallStatus(10, &ss);
	LOG_DEBUG(L"call status return: %08X, status = %d", res, ss);
	if (res == WBEM_S_NO_ERROR)
	{
		m_status = ss;
		auto_unknown<IWbemClassObject> out_param;
		m_call_result->GetResultObject(WBEM_INFINITE, &out_param);
		LOG_DEBUG(L"get results");
		prop_tree::wptree prop;
		ReadWmiObject(prop, out_param);

		CJCVariant return_val;
		HRESULT hres = out_param->Get(BSTR(L"ReturnValue"), 0, &return_val, NULL, 0);
		UINT32 res = return_val.val<UINT32>();
		LOG_DEBUG(L"result = %d", res);

	}
	m_semi_progress += (INT_MAX / 100);
	return m_semi_progress;
}

int CWmiSemisyncProgress::GetResult(void)
{
	LOG_STACK_TRACE();
	return m_status;
}

void CWmiSemisyncProgress::GetStatus(std::wstring & status)
{
	status = L"running";
}

int CWmiSemisyncProgress::WaitForStatusUpdate(DWORD timeout, std::wstring & status, int & progress)
{
	DWORD tt = min(timeout, 500);
	Sleep(tt);
	progress = GetProgress();
	status = L"running";

	return m_status;
}

bool CWmiSemisyncProgress::SetResult(IWbemCallResult * call_result)
{
	JCASSERT(call_result);
	m_call_result = call_result;
	m_call_result->AddRef();
	return true;
}

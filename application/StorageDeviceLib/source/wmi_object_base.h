#pragma once

#include "../include/idiskinfo.h"
#include <shlwapi.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <VsProv.h>

#include <initguid.h>
//#include <vds.h>
#include <boost/property_tree/json_parser.hpp>
#include "../include/utility.h"


class CStorageManager;

//template <class T> VARTYPE TypeToVt(void);
template <class T> void SetVarType(VARIANT & var, const T & val, CIMTYPE &type);

class CJCVariant/* : public VARIANT*/
{
public:
	CJCVariant(void);
	~CJCVariant(void);

	template <class T> CJCVariant(const T & val);
	template <class T> CJCVariant & operator = (const T & val);
	operator VARIANT&(void) { return m_var; }
	VARIANT * operator &(void) { return &m_var; }
//	template <class T> T operator = (void);
	template <class T> T val(void) const;
protected:
	VARIANT m_var;
};


///////////////////////////////////////////////////////////////////////////////
// -- progress class ---------------------------------------------------------
class CQuerySink : public IWbemObjectSink
{
	LONG m_lRef;
	bool bDone;

public:
	CQuerySink() { m_lRef = 0; }
	~CQuerySink() { bDone = TRUE; }

	virtual ULONG STDMETHODCALLTYPE AddRef() {return InterlockedIncrement(&m_lRef); }
	virtual ULONG STDMETHODCALLTYPE Release() 
	{ 
		LONG lRef = InterlockedDecrement(&m_lRef);
		if (lRef == 0) delete this;
		return lRef;
	}
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv)
	{
		if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
		{
			*ppv = (IWbemObjectSink *)this;
			AddRef();
			return WBEM_S_NO_ERROR;
		}
		else return E_NOINTERFACE;
	}

	virtual HRESULT STDMETHODCALLTYPE Indicate(
		/* [in] */
		LONG lObjectCount,
		/* [size_is][in] */
		IWbemClassObject __RPC_FAR *__RPC_FAR *apObjArray
	);

	virtual HRESULT STDMETHODCALLTYPE SetStatus(
		/* [in] */ LONG lFlags,
		/* [in] */ HRESULT hResult,
		/* [in] */ BSTR strParam,
		/* [in] */ IWbemClassObject __RPC_FAR *pObjParam
	);
};

class CWmiAsyncProgress : public IJCProgress
{
public:
	CWmiAsyncProgress(void);
	~CWmiAsyncProgress(void);

public:
	virtual int GetProgress(void) { return 0; };
	virtual int GetResult(void) { return 1; };
	virtual void GetStatus(std::wstring & status) {};

public:
	bool GetSink(IWbemObjectSink * & sink) { return false; };

protected:
	CQuerySink * m_query_sink;
};

class CWmiSemisyncProgress : public IJCProgress
{
public:
	CWmiSemisyncProgress(void);
	~CWmiSemisyncProgress(void);

public:
	virtual int GetProgress(void);
	virtual int GetResult(void);
	virtual void GetStatus(std::wstring & status);
	virtual int WaitForStatusUpdate(DWORD timeout, std::wstring & status, int & progress);

public:
	bool SetResult(IWbemCallResult * call_result);

protected:
	int m_status;
	int m_semi_progress;
	IWbemCallResult * m_call_result;
};


///////////////////////////////////////////////////////////////////////////////
// -- wmi object base ---------------------------------------------------------

class CWmiObjectBase
{
public:
	CWmiObjectBase(void) : m_obj(NULL) {}
	~CWmiObjectBase(void);
public:
	virtual void SetWmiProperty(IWbemClassObject * obj) = 0;
	void Initialize(IWbemClassObject * obj, CStorageManager * manager);
	friend class CStorageManager;
public:
	static const wchar_t * GetMSFTErrorMsg(UINT32 err_code);
	bool Refresh(void);
	//bool InvokeMethod(const wchar_t * name, boost::property_tree::wptree & out_param, const boost::property_tree::wptree & in_param);

	template <class T> void PutParameter(IWbemClassObject *in_param, int param_index, 
		const wchar_t* param_name, const T& pval)
	{
		HRESULT hres;
		JCASSERT(param_name);
		//if (param_name)
		//{
			if (in_param == nullptr) THROW_ERROR(ERR_APP, L"function has no input parameter");
			CJCVariant var(pval);
			hres = in_param->Put(param_name, 0, &var, 0);
			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[%d] %s", param_index, param_name);
		//}
	}

	template <class T1=bool, class T2 = bool, class T3 = bool, class T4=bool, class T5=bool>
	bool MakeCallParameter( IWbemClassObject * & in_param, IWbemClassObject *& out_param, 
		const wchar_t * name,
		const wchar_t * pname_1 = NULL, const T1 & pval_1 = false,
		const wchar_t * pname_2 = NULL, const T2 & pval_2 = false,
		const wchar_t * pname_3 = NULL, const T3 & pval_3 = false,
		const wchar_t * pname_4 = NULL, const T4 & pval_4 = false,
		const wchar_t * pname_5 = NULL, const T5 & pval_5 = false)
	{
		HRESULT hres;
		auto_unknown<IWbemClassObject> class_obj;
		hres = m_manager->m_services->GetObject(BSTR(m_class_name.c_str()), 0, NULL, &class_obj, NULL);
		if (FAILED(hres) || !class_obj) THROW_COM_ERROR(hres,L"failed on getting class (%s)", m_class_name.c_str());

		hres = class_obj->GetMethod(BSTR(name), 0, &in_param, &out_param);
		if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting method (%s)", name);
		//if (pname_1)
		//{
		//	if (in_param == nullptr) THROW_ERROR(ERR_APP, L"function [%s] has no input parameter", name);
		//	PutParameter(in_param, 1, pname_1, pval_1);
		//	if (pname_2) { PutParameter(in_param, 2, pname_2, pval_2);
		//	if (pname_3) { PutParameter(in_param, 3, pname_3, pval_3);
		//	if (pname_4) { PutParameter(in_param, 4, pname_4, pval_4); 
		//	if (pname_5) { PutParameter(in_param, 5, pname_5, pval_5);
		//	} } } }
		//}
		if (pname_1) PutParameter(in_param, 1, pname_1, pval_1);
		if (pname_2) PutParameter(in_param, 2, pname_2, pval_2);
		if (pname_3) PutParameter(in_param, 3, pname_3, pval_3);
		if (pname_4) PutParameter(in_param, 4, pname_4, pval_4); 
		if (pname_5) PutParameter(in_param, 5, pname_5, pval_5);
		return true;
	}


	template <class T1=bool, class T2 = bool, class T3 = bool, class T4=bool, class T5=bool>
	UINT32 InvokeMethod(const wchar_t * name, IWbemClassObject * & out_param, 
		const wchar_t * pname_1 = NULL, const T1 & pval_1 = false,
		const wchar_t * pname_2 = NULL, const T2 & pval_2 = false,
		const wchar_t * pname_3 = NULL, const T3 & pval_3 = false,
		const wchar_t * pname_4 = NULL, const T4 & pval_4 = false,
		const wchar_t * pname_5 = NULL, const T5 & pval_5 = false
	)
	{
		HRESULT hres;
//		auto_unknown<IWbemClassObject> class_obj;
//		hres = m_manager->m_services->GetObject(BSTR(m_class_name.c_str()), 0, NULL, &class_obj, NULL);
//		if (FAILED(hres) || !class_obj) THROW_COM_ERROR(hres,
//			L"failed on getting class (%s)", m_class_name.c_str());
//
		auto_unknown<IWbemClassObject> in_signaure;
		auto_unknown<IWbemClassObject> out_signature;
//		hres = class_obj->GetMethod(BSTR(name), 0, &class_instance, &out_signature);
////		if (FAILED(hres) || !class_instance) THROW_COM_ERROR(hres, L"failed on getting method (%s)", name);
//		if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting method (%s)", name);
//
//
//		if (pname_1)
//		{
//			if (class_instance == nullptr) THROW_ERROR(ERR_APP, L"function [%s] has no input parameter", name);
//			CJCVariant var(pval_1);
//			//CIMTYPE type;
////			hres = class_instance->Get(pname_1, 0, &var, &type, NULL);
////			var_1 = pval_1;
//			hres = class_instance->Put(pname_1, 0, &var, 0);
//			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[1] %s", pname_1);
//		}
//		if (pname_2)
//		{
//			CJCVariant var(pval_2);
//			hres = class_instance->Put(pname_2, 0, &var, 0);
//			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[2] %s", pname_2);
//		}
//		if (pname_3)
//		{
//			CJCVariant var(pval_3);
//			hres = class_instance->Put(pname_3, 0, &var, 0);
//			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_3);
//		}
//		if (pname_4)
//		{
//			CJCVariant var(pval_4);
//			hres = class_instance->Put(pname_4, 0, &var, 0);
//			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_4);
//		}
//		if (pname_5)
//		{
//			CJCVariant var(pval_5);
//			hres = class_instance->Put(pname_5, 0, &var, 0);
//			if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_5);
//		}
		MakeCallParameter(in_signaure, out_signature, name, pname_1, pval_1,
			pname_2, pval_2, pname_3, pval_3, pname_4, pval_4, pname_5, pval_5);
		hres = m_manager->m_services->ExecMethod(BSTR(m_obj_path.c_str()), BSTR(name), 0,
			NULL, in_signaure, &out_param, NULL);
		if (FAILED(hres) || !out_param) THROW_COM_ERROR(hres, L"failed on invoking method (%s)", name);
		return 0;
	}

	template <class T1=bool, class T2 = bool, class T3 = bool>
	UINT32 InvokeMethod(const wchar_t * name, 
		const wchar_t * pname_1 = NULL, const T1 & pval_1 = false,
		const wchar_t * pname_2 = NULL, const T2 & pval_2 = false,
		const wchar_t * pname_3 = NULL, const T3 & pval_3 = false
	)
	{
		auto_unknown<IWbemClassObject> out_param;
		InvokeMethod<T1, T2, T3>(name, out_param, pname_1, pval_1, pname_2, pval_2, pname_3, pval_3);

		CJCVariant return_val;
		HRESULT hres = out_param->Get(BSTR(L"ReturnValue"), 0, &return_val, NULL, 0);
		UINT32 res = return_val.val<UINT32>();
		return res;
	}

	bool IsObject(const std::wstring & path);
	static bool GetObjectFromPath(IWbemClassObject * & obj, const std::wstring & path, CStorageManager * mamager);

	HRESULT InternalInvokeMethodAsync(IJCProgress * & progress, const wchar_t * name, IWbemClassObject * class_instance);

	template <class T1 = bool, class T2 = bool, class T3 = bool, class T4 = bool, class T5 = bool>
	UINT32 InvokeMethodAsync(IJCProgress * & progress, const wchar_t * name,
		const wchar_t * pname_1 = NULL, const T1 & pval_1 = false,
		const wchar_t * pname_2 = NULL, const T2 & pval_2 = false,
		const wchar_t * pname_3 = NULL, const T3 & pval_3 = false,
		const wchar_t * pname_4 = NULL, const T4 & pval_4 = false,
		const wchar_t * pname_5 = NULL, const T5 & pval_5 = false
	)
	{
		HRESULT hres;
		//auto_unknown<IWbemClassObject> class_obj;
		//hres = m_manager->m_services->GetObject(BSTR(m_class_name.c_str()), 0, NULL, &class_obj, NULL);
		//if (FAILED(hres) || !class_obj) THROW_COM_ERROR(hres,
		//	L"failed on getting class (%s)", m_class_name.c_str());

		//auto_unknown<IWbemClassObject> class_instance;
		//hres = class_obj->GetMethod(BSTR(name), 0, &class_instance, NULL);
		//if (FAILED(hres) || !class_instance) THROW_COM_ERROR(hres, L"failed on getting method (%s)", name);

		//if (pname_1)
		//{
		//	CJCVariant var(pval_1);
		//	hres = class_instance->Put(pname_1, 0, &var, 0);
		//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[1] %s", pname_1);
		//}
		//if (pname_2)
		//{
		//	CJCVariant var(pval_2);
		//	hres = class_instance->Put(pname_2, 0, &var, 0);
		//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[2] %s", pname_2);
		//}
		//if (pname_3)
		//{
		//	CJCVariant var(pval_3);
		//	hres = class_instance->Put(pname_3, 0, &var, 0);
		//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_3);
		//}
		//if (pname_4)
		//{
		//	CJCVariant var(pval_4);
		//	hres = class_instance->Put(pname_4, 0, &var, 0);
		//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_4);
		//}
		//if (pname_5)
		//{
		//	CJCVariant var(pval_5);
		//	hres = class_instance->Put(pname_5, 0, &var, 0);
		//	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on setting param[3] %s", pname_5);
		//}
		auto_unknown<IWbemClassObject> in_signaure;
		auto_unknown<IWbemClassObject> out_signature;

		MakeCallParameter(in_signaure, out_signature, name, pname_1, pval_1,
			pname_2, pval_2, pname_3, pval_3, pname_4, pval_4, pname_5, pval_5);

		// Execute Method
		hres = InternalInvokeMethodAsync(progress, name, in_signaure);
		if (FAILED(hres) || progress == nullptr) THROW_COM_ERROR(hres, L"failed on invoking method (%s)", name);
		return 0;
	}





protected:
	std::wstring m_obj_path;
	IWbemClassObject * m_obj;

	std::wstring m_rel_path;
	std::wstring m_class_name;
	CStorageManager * m_manager;
};



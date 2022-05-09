#include "pch.h"

#include "../include/utility.h"
#include <cctype>
LOCAL_LOGGER_ENABLE(L"utility", LOGGER_LEVEL_NOTICE);

void GetComError(wchar_t * out_msg, size_t buf_size, HRESULT res, const wchar_t * msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);

	IErrorInfo * com_err = NULL;
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

void GetWmiError(wchar_t * out_msg, size_t buf_size, HRESULT res, const wchar_t * msg, ...)
{
	static HMODULE module_wbem =NULL;
	if (module_wbem==NULL) module_wbem = LoadLibrary(L"C:\\Windows\\System32\\wbem\\wmiutils.dll");

	va_list argptr;
	va_start(argptr, msg);

	IErrorInfo * com_err = NULL;
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

template <class T>
void AddArrayData(boost::property_tree::wptree & pt, SAFEARRAY * val)
{
	T * arr = (T*)(val->pvData);
	for (size_t ii = 0; ii < val->rgsabound[0].cElements; ++ii)
	{
		boost::property_tree::wptree pval;
		pval.put_value(arr[ii]);
		pt.push_back(std::make_pair(L"", pval));
	}
}

void ReadWmiObject(boost::property_tree::wptree & obj_out, IWbemClassObject * obj_in)
{
	JCASSERT(obj_in);
	HRESULT hr = obj_in->BeginEnumeration(WBEM_FLAG_LOCAL_ONLY);
	while (WBEM_S_NO_ERROR == hr)
	{
		BSTR val_name;
		VARIANT val;
		CIMTYPE type;
		long lFlavor = 0;
		hr = obj_in->Next(0, &val_name, &val, &type, &lFlavor);
		if (FAILED(hr)) break;
		if (val_name)
		{
			LOG_DEBUG(L"get field=%s, type=%d, vt=%d", val_name, type, val.vt);
			if ( (val.vt &(~VT_TYPEMASK)) == VT_ARRAY)
			{	// handle array
				VARTYPE element_type = val.vt & VT_TYPEMASK;
				boost::property_tree::wptree pt_arr;
				switch (element_type)
				{
				case VT_BSTR:	AddArrayData<BSTR>(pt_arr, val.parray); break;
				case VT_I2:		AddArrayData<short>(pt_arr, val.parray);	break;
				case VT_UI2:	AddArrayData<UINT16>(pt_arr, val.parray); break;
				case VT_I4:		AddArrayData<int>(pt_arr, val.parray);	break;
				case VT_UI4:	AddArrayData<UINT32>(pt_arr, val.parray); break;
				case VT_I8:		AddArrayData<INT64>(pt_arr, val.parray);	break;
				case VT_UI8:	AddArrayData<UINT64>(pt_arr, val.parray);	break;
//				case VT_BOOL:	AddArrayData<BOOL>(pt_arr, val.parray); break;
				default:		THROW_ERROR(ERR_APP, L"unsuported type"); break;
				}
				obj_out.put_child(val_name, pt_arr);
			}
			else
			{
				switch (val.vt)
				{
				case VT_BSTR:	obj_out.add(val_name, val.bstrVal); break;
				case VT_I2:		obj_out.add(val_name, val.iVal);	break;
				case VT_UI2:	obj_out.add(val_name, val.uiVal);	break;
				case VT_I4:		obj_out.add(val_name, val.intVal);	break;
				case VT_UI4:	obj_out.add(val_name, val.uintVal); break;
				case VT_I8:		obj_out.add(val_name, val.llVal); break;
				case VT_UI8:	obj_out.add(val_name, val.ullVal);	break;
				case VT_BOOL:	obj_out.add<bool>(val_name, val.boolVal != 0); break;
				}
			}
		}
		VariantClear(&val);
	}
	hr = obj_in->EndEnumeration();
}

size_t AllocDriveInfo(size_t partition_num, DRIVE_LAYOUT_INFORMATION_EX *& info)
{
	size_t buf_size = sizeof(DRIVE_LAYOUT_INFORMATION_EX)
		+ sizeof(PARTITION_INFORMATION_EX) * (partition_num - 1);
	BYTE * _buf = new BYTE[buf_size];
	memset(_buf, 0, buf_size);
	info = (DRIVE_LAYOUT_INFORMATION_EX*)(_buf);
	return buf_size;
}

size_t LayoutInfoSize(size_t partition_num)
{
	size_t buf_size = sizeof(DRIVE_LAYOUT_INFORMATION_EX)
		+ sizeof(PARTITION_INFORMATION_EX) * (partition_num - 1);
	return buf_size;
}

bool WaitAsync(IVdsAsync * async, VDS_ASYNC_OUTPUT * wait_out)
{
	HRESULT wait_res;
//	VDS_ASYNC_OUTPUT wait_out;
	memset(wait_out, 0, sizeof(VDS_ASYNC_OUTPUT));

	HRESULT hres = async->Wait(&wait_res, wait_out);
	if (FAILED(hres))
	{
		THROW_COM_ERROR(hres, L"failed on waiting for creating volume");
		return false;
	}
	if (FAILED(wait_res))
	{
		THROW_COM_ERROR(wait_res, L"failed on the operation");
		return false;
	}
	return true;
}

// A8h reflected is 15h, i.e. 10101000 <--> 00010101
static int reflect(int data, int len)
{
	int ref = 0;
	for (int i = 0; i < len; i++) 
	{
		if (data & 0x1) ref |= (1 << ((len - 1) - i));
		data = (data >> 1);
	}
	return ref;
}

// Function to calculate the CRC32
static DWORD calculate_crc32(BYTE *buffer, size_t len)
{
	int byte_length = 8;			/*length of unit (i.e. byte) */
	int msb = 0;
	int polynomial = 0x04C11DB7;	/* IEEE 32bit polynomial */
	unsigned int regs = 0xFFFFFFFF;	/* init to all ones */
	int regs_mask = 0xFFFFFFFF;		/* ensure only 32 bit answer */
	int regs_msb = 0;
	unsigned int reflected_regs;

	for (int i = 0; i < len; i++) 
	{
		int data_byte = buffer[i];
		data_byte = reflect(data_byte, 8);
		for (int j = 0; j < byte_length; j++) 
		{
			msb = data_byte >> (byte_length - 1);	/* get MSB */
			msb &= 1;								/* ensure just 1 bit */
			regs_msb = (regs >> 31) & 1;			/* MSB of regs */
			regs = regs << 1;						/* shift regs for CRC-CCITT */
			if (regs_msb ^ msb) 
			{										/* MSB is a 1 */
				regs = regs ^ polynomial;			/* XOR with generator poly */
			}
			regs = regs & regs_mask;				/* Mask off excess upper bits */
			data_byte <<= 1;						/* get to next bit */
		}
	}
	regs = regs & regs_mask;
	reflected_regs = reflect(regs, 32) ^ 0xFFFFFFFF;
	return reflected_regs;
}

DWORD CalculateCRC32(DWORD * buf, size_t len)
{
	size_t blen = len * 4;
	return calculate_crc32((BYTE*)buf, blen);
}

float ByteToGb(UINT64 size)
{
	double ss = (double)size;
	ss /= (1024.0*1024.0*1024.0);
	return (float)ss;
}

bool EnabledDebugPrivilege()
{
	HANDLE hToken;
	BOOL br = FALSE;

	br = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
	if (!br) THROW_WIN32_ERROR(L"failed on rising privilege");
	jcvos::auto_handle<HANDLE> htoken(hToken);

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	br = LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &tp.Privileges[0].Luid);
	if (!br) THROW_WIN32_ERROR(L"lookup privilege error");
	//		tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
	br = (GetLastError() == ERROR_SUCCESS);
	return br!=FALSE;
}

void PrintProvider(IVdsProvider * provider)
{
	VDS_PROVIDER_PROP provider_prop;
	InitMem(&provider_prop);
	HRESULT	hres = provider->GetProperties(&provider_prop);
	wprintf_s(L"\t provider:%s, id:" WSTR_GUID_FMT L"\n", provider_prop.pwszName, GUID_PRINTF_ARG(provider_prop.id));
}

void PrintSubSystem(IVdsSubSystem * subsystem)
{
	VDS_SUB_SYSTEM_PROP prop;
	InitMem(&prop);
	subsystem->GetProperties(&prop);
	wprintf_s(L"subsystem: friendly name:%s\n\t identification:%s\n\t id:" WSTR_GUID_FMT L"\n", 
		prop.pwszFriendlyName, prop.pwszIdentification, GUID_PRINTF_ARG(prop.id));
}

void PrintDrive(IVdsDrive * drive)
{
	VDS_DRIVE_PROP prop;
	InitMem(&prop, 0);
	drive->GetProperties(&prop);
	wprintf_s(L"drive: friendly name:%s\n\t identification:%s\n\t id:" WSTR_GUID_FMT L"\n", 
		prop.pwszFriendlyName, prop.pwszIdentification, GUID_PRINTF_ARG(prop.id));
}

void PrintDisk2(VDS_DISK_PROP2 & prop)
{
//	wprintf_s(L"")
}


bool GetStringValueFromQuery(std::wstring & res, IWbemServices* pIWbemServices, const wchar_t* query, const wchar_t* valuename)
{
	auto_unknown<IEnumWbemClassObject> pEnumCOMDevs;
	ULONG uReturned = 0;
//	CString	result = L"";

	HRESULT hres = pIWbemServices->ExecQuery(BSTR(L"WQL"),
		BSTR(query), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs);
	if (FAILED(hres) || pEnumCOMDevs == NULL)
	{
//		THROW_COM_ERROR(hres, L"failed on query wql=%s", query);
		LOG_COM_ERROR(hres, L"failed on query wql=%s", query);
		return false;
	}

//	while (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1)
	while (1)
	{
		auto_unknown<IWbemClassObject> pCOMDev;
		hres = pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned);
		if (!SUCCEEDED(hres) || uReturned != 1) break;

		VARIANT pVal;
		VariantInit(&pVal);
		if (pCOMDev->Get(valuename, 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL)
		{
			res = pVal.bstrVal;
			VariantClear(&pVal);
			return true;
		}
		VariantInit(&pVal);
	}
	return false;
}
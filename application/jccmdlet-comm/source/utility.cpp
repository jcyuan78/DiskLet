#include "pch.h"
//#include "../include/disk_info.h"
#include "../include/utility.h"

using namespace System;

/*
Clone::PartitionType GuidToPartitionType(System::Guid ^ type_id)
{
	Clone::PartitionType type;

	if (type_id->Equals(System::Guid("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}")))
	{
		type = Clone::PartitionType::EFI_Partition;
	}
	else if (type_id->Equals(System::Guid("{e3c9e316-0b5c-4db8-817d-f92df00215ae}")))
	{
		type = Clone::PartitionType::Microsoft_Reserved;
	}
	else if (type_id->Equals(System::Guid("{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}")))
	{
		type = Clone::PartitionType::Basic_Data;
	}
	else if (type_id->Equals(System::Guid("{5808c8aa-7e8f-42e0-85d2-e1e90434cfb3}")))
	{
		type = Clone::PartitionType::LDM_Metadata;
	}
	else if (type_id->Equals(System::Guid("{af9b60a0-1431-4f62-bc68-3311714a69ad}")))
	{
		type = Clone::PartitionType::LDM_Data;
	}
	else if (type_id->Equals(System::Guid("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}")))
	{
		type = Clone::PartitionType::Microsoft_Recovery;
	}
	else type = Clone::PartitionType::Unkown_Partition;

	return type;
}

*/

void GetComError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...)
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

void GetWmiError(wchar_t* out_msg, size_t buf_size, HRESULT res, const wchar_t* msg, ...)
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
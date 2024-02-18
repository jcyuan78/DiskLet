#include "pch.h"

#include "../include/utility.h"

#include "SlotSpeedGetter.h"
#include <wbemcli.h>
#include <comutil.h>
#include <Setupapi.h>
#include <Cfgmgr32.h>
#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "wbemuuid.lib")
//#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comsupp.lib")

#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

DEFINE_GUID(GUID_DEVCLASS_SCSIADAPTER, 0x4D36E97B, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);


typedef BOOL(WINAPI *FN_SetupDiGetDeviceProperty)(
	__in       HDEVINFO DeviceInfoSet,
	__in       PSP_DEVINFO_DATA DeviceInfoData,
	__in       const DEVPROPKEY *PropertyKey,
	__out      DEVPROPTYPE *PropertyType,
	__out_opt  PBYTE PropertyBuffer,
	__in       DWORD PropertyBufferSize,
	__out_opt  PDWORD RequiredSize,
	__in       DWORD Flags
	);

//std::wstring GetStringValueFromQuery(IWbemServices* pIWbemServices, const std::wstring & query, const std::wstring & valuename)
//{
//	IEnumWbemClassObject* pEnumCOMDevs = NULL;
//	IWbemClassObject* pCOMDev = NULL;
//	ULONG uReturned = 0;
//	std::wstring	result = L"";
//
//	try
//	{
//		if (SUCCEEDED(pIWbemServices->ExecQuery(_bstr_t(L"WQL"),
//			_bstr_t(query), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumCOMDevs)))
//		{
//			while (pEnumCOMDevs && SUCCEEDED(pEnumCOMDevs->Next(10000, 1, &pCOMDev, &uReturned)) && uReturned == 1)
//			{
//				VARIANT pVal;
//				VariantInit(&pVal);
//
//				if (pCOMDev->Get(L"DeviceID", 0L, &pVal, NULL, NULL) == WBEM_S_NO_ERROR && pVal.vt > VT_NULL)
//				{
//					result = pVal.bstrVal;
//					VariantClear(&pVal);
//				}
//				VariantInit(&pVal);
//			}
//		}
//	}
//	catch (...)
//	{
//		result = L"";
//	}
//
//	SAFE_RELEASE(pCOMDev);
//	SAFE_RELEASE(pEnumCOMDevs);
//	return result;
//}

std::wstring GetDeviceIDFromPhysicalDriveID(const INT physicalDriveId, const BOOL IsKernelVerEqualOrOver6)
{
	const std::wstring query2findstorage = L"ASSOCIATORS OF {Win32_DiskDrive.DeviceID='\\\\.\\PhysicalDrive%d'} WHERE ResultClass=Win32_PnPEntity";
	const std::wstring query2findcontroller = L"ASSOCIATORS OF {Win32_PnPEntity.DeviceID='%s'} WHERE AssocClass=Win32_SCSIControllerDevice";
	wchar_t query[128];
	std::wstring result;
	IWbemLocator* pIWbemLocator = NULL;
	IWbemServices* pIWbemServices = NULL;
	BOOL flag = FALSE;

	try
	{
		if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
			IID_IWbemLocator, (LPVOID *)&pIWbemLocator)))
		{
			long securityFlag = 0;
			if (IsKernelVerEqualOrOver6) { securityFlag = WBEM_FLAG_CONNECT_USE_MAX_WAIT; }
			if (SUCCEEDED(pIWbemLocator->ConnectServer(_bstr_t(L"\\\\.\\root\\cimv2"),
				NULL, NULL, 0L, securityFlag, NULL, NULL, &pIWbemServices)))
			{
				if (SUCCEEDED(CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
					NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE)))
				{
//					query.Format(query2findstorage, physicalDriveId);
					swprintf_s(query, query2findstorage.c_str(), physicalDriveId);
					std::wstring StorageID;
					bool br = GetStringValueFromQuery(StorageID, pIWbemServices, query, L"DeviceID");
					if(br && !StorageID.empty())
					{
						//query.Format(query2findcontroller, StorageID);
						swprintf_s(query, query2findcontroller.c_str(), StorageID);
						std::wstring ControllerID;
						br = GetStringValueFromQuery(ControllerID, pIWbemServices, query, L"DeviceID");
						result = ControllerID;
					}
					else
					{
						result = L"";
					}
				}
			}
		}
	}
	catch (...)
	{
		result = L"";
	}
	SAFE_RELEASE(pIWbemServices);
	SAFE_RELEASE(pIWbemLocator);
	return result;
}

void  ConvertOSResult(SlotMaxCurrSpeed & result, const OSSlotMaxCurrSpeed & OSLevelResult)
{
//	SlotMaxCurrSpeed result;
	result.Current.LinkWidth = PCIeDataWidth(OSLevelResult.Current.LinkWidth);
	result.Current.SpecVersion = PCIeSpecification(OSLevelResult.Current.SpecVersion);
	result.Maximum.LinkWidth = PCIeDataWidth(OSLevelResult.Maximum.LinkWidth);
	result.Maximum.SpecVersion = PCIeSpecification(OSLevelResult.Maximum.SpecVersion);
//	return result;
}



bool GetSlotMaxCurrSpeedFromDeviceID(SlotMaxCurrSpeed & speed, const std::wstring DeviceID)
{
	DWORD CurrentDevice = 0;
	OSSlotMaxCurrSpeed OSLevelResult = {0};
	GUID SCSIAdapterGUID = GUID_DEVCLASS_SCSIADAPTER;
	HDEVINFO ClassDeviceInformations = SetupDiGetClassDevs(&SCSIAdapterGUID, nullptr, 0, DIGCF_PRESENT);

	BOOL LastResult;
	do
	{
		SP_DEVINFO_DATA DeviceInfoData = {sizeof(SP_DEVINFO_DATA)};
		LastResult = SetupDiEnumDeviceInfo(ClassDeviceInformations, CurrentDevice, &DeviceInfoData);

		BOOL DeviceIDFound = FALSE;	
		TCHAR DeviceIDBuffer[MAX_PATH] = {};
		BOOL GetDeviceResult = CM_Get_Device_ID(DeviceInfoData.DevInst, DeviceIDBuffer, sizeof(DeviceIDBuffer), 0);
		DeviceIDFound = (GetDeviceResult == ERROR_SUCCESS) && (DeviceID == DeviceIDBuffer);
//			(DeviceID.Compare(DeviceIDBuffer) == 0);
		if (LastResult && DeviceIDFound)
		{
			DEVPROPTYPE PropertyType;
			BYTE ResultBuffer[1024] = {0};
			DWORD RequiredSize;

			FN_SetupDiGetDeviceProperty SetupDiGetDeviceProperty = 
				(FN_SetupDiGetDeviceProperty) GetProcAddress(GetModuleHandle(TEXT("Setupapi.dll")), "SetupDiGetDevicePropertyW");
			//Compatible with pre-vista era windows.

			SetupDiGetDeviceProperty(ClassDeviceInformations, &DeviceInfoData,
				&DEVPKEY_PciDevice_MaxLinkWidth, &PropertyType, ResultBuffer,
				sizeof(ResultBuffer), &RequiredSize, 0);
			OSLevelResult.Maximum.LinkWidth = ResultBuffer[0];
			SetupDiGetDeviceProperty(ClassDeviceInformations, &DeviceInfoData,
				&DEVPKEY_PciDevice_MaxLinkSpeed, &PropertyType, ResultBuffer,
				sizeof(ResultBuffer), &RequiredSize, 0);
			OSLevelResult.Maximum.SpecVersion = ResultBuffer[0];

			SetupDiGetDeviceProperty(ClassDeviceInformations, &DeviceInfoData,
				&DEVPKEY_PciDevice_CurrentLinkWidth, &PropertyType, ResultBuffer,
				sizeof(ResultBuffer), &RequiredSize, 0);
			OSLevelResult.Current.LinkWidth = ResultBuffer[0];
			SetupDiGetDeviceProperty(ClassDeviceInformations, &DeviceInfoData,
				&DEVPKEY_PciDevice_CurrentLinkSpeed, &PropertyType, ResultBuffer,
				sizeof(ResultBuffer), &RequiredSize, 0);
			OSLevelResult.Current.SpecVersion = ResultBuffer[0];
			
			break;
		}

		++CurrentDevice;
	} while (LastResult);

//	return ConvertOSResult(OSLevelResult);
	ConvertOSResult(speed, OSLevelResult);
	return true;
}

//SlotMaxCurrSpeed GetPCIeSlotSpeed(const INT physicalDriveId, const BOOL IsKernelVerEqualOrOver6)
//{
//	std::wstring DeviceID = GetDeviceIDFromPhysicalDriveID(physicalDriveId, IsKernelVerEqualOrOver6);
//	return GetSlotMaxCurrSpeedFromDeviceID(DeviceID);
//}

std::wstring SlotSpeedToString(SlotSpeed speedtoconv)
{
	std::wstring result = L"";
	wchar_t rr[32];
	if (speedtoconv.SpecVersion == 0 || speedtoconv.LinkWidth == 0)
	{
		result = L"----";
//		result.Format(L"----");
	}
	else
	{
//		result.Format(L"PCIe %d.0 x%d", speedtoconv.SpecVersion, speedtoconv.LinkWidth);
		swprintf_s(rr, L"PCIe %d.0 x%d", speedtoconv.SpecVersion, speedtoconv.LinkWidth);
		result = rr;
	}
	return result;
}
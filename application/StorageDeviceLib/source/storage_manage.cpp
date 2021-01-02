#include "pch.h"

#include "../include/utility.h"
#include "storage_manager.h"
#include "disk_info.h"

LOCAL_LOGGER_ENABLE(L"storage_manage", LOGGER_LEVEL_NOTICE);
namespace prop_tree = boost::property_tree;

const GUID GPT_TYPE_MSR =	GUID({ 0xe3c9e316, 0x0b5c, 0x4db8, {0x81, 0x7d, 0xf9, 0x2d, 0xf0, 0x02, 0x15, 0xae} });
const GUID GPT_TYPE_EFI =	GUID({ 0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b} });
const GUID GPT_TYPE_BASIC = GUID({ 0xebd0a0a2, 0xb9e5, 0x4433, {0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7} });

CStorageManager::CStorageManager(void) 
	: m_services(NULL), m_backup(NULL)
{
}

CStorageManager::~CStorageManager(void)
{
	if (m_services) m_services->Release();
	if (m_backup) m_backup->Release();
	CoUninitialize();
}

bool CStorageManager::Initialize(bool dll)
{
	HRESULT hres;
	if (!dll)
	{
		hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (hres != S_OK)	THROW_COM_ERROR(hres, L"Failed on initialize COM, code=0x%X", hres);
		hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
			RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
			NULL, EOAC_NONE, NULL);
		if (FAILED(hres))	THROW_COM_ERROR(hres, L"Failed on initialize security, code=0x%X", hres);
	}
	auto_unknown<IWbemLocator> loc;
	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, loc);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on creating wbem locator");

	hres = loc->ConnectServer(BSTR(L"Root\\Microsoft\\Windows\\Storage"), NULL, NULL, 0, NULL, 0, 0, &m_services);
	if (FAILED(hres) || !m_services) THROW_COM_ERROR(hres, L"failed on connecting wmi services");

	hres = CoSetProxyBlanket(m_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
		NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	if (FAILED(hres))	THROW_COM_ERROR(hres, L"failed on setting proxy blanket");

	return true;
}

bool CStorageManager::ListDisk(void)
{
	m_disk_list.ClearObj();

	HRESULT hres;

	// 枚举物理磁盘
	auto_unknown<IEnumWbemClassObject> penum;
	hres = m_services->CreateInstanceEnum(BSTR(L"MSFT_Disk"), WBEM_FLAG_FORWARD_ONLY , NULL, &penum);
	if (FAILED(hres) || !penum) THROW_COM_ERROR(hres, L"failed on quering physical disk, error=0x%X", hres);
	while (1)
	{
		auto_unknown<IWbemClassObject> obj;
		ULONG count = 0;
		hres = penum->Next(WBEM_INFINITE, 1, &obj, &count);
		if (FAILED(hres))	THROW_COM_ERROR(hres, L"failed on getting physical disk, error=0x%X", hres);
		if (hres == S_FALSE || obj == NULL) break;
		// 处理查询结果

		CDiskInfo * disk_info = jcvos::CDynamicInstance<CDiskInfo>::Create();
		UINT32 disk_index = disk_info->Initialize(obj, this);
		if (disk_index >= m_disk_list.size())	m_disk_list.resize(disk_index + 1);
		m_disk_list.at(disk_index) = disk_info;
	}

	penum.reset();
	hres = m_services->CreateInstanceEnum(BSTR(L"MSFT_DiskToPartition"), WBEM_FLAG_FORWARD_ONLY , NULL, &penum);
	if (FAILED(hres) || !penum) THROW_COM_ERROR(hres, L"failed on quering disk-partition, error=0x%X", hres);
	//hres = m_services->CreateClassEnum(BSTR(L"MSFT_Disk"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &penum);
	//if (FAILED(hres) || !penum) THROW_COM_ERROR(hres, L"failed on quering physical disk, error=0x%X", hres);
	while (1)
	{
		auto_unknown<IWbemClassObject> obj;
		ULONG count = 0;
		hres = penum->Next(WBEM_INFINITE, 1, &obj, &count);
		if (FAILED(hres)) LOG_COM_ERROR(hres, L"failed on getting object");
		if (hres == S_FALSE || obj == NULL) break;
		prop_tree::wptree prop;
		if (!(obj==NULL)) ReadWmiObject(prop, obj);
		std::wstring & disk_path = prop.get<std::wstring>(L"Disk");
		std::wstring & partition_path = prop.get<std::wstring>(L"Partition");
		// find disk
		for (auto it = m_disk_list.begin(); it != m_disk_list.end(); ++it)
		{
			CDiskInfo * disk_info = *it;
			if (disk_info == NULL) continue;
			if (disk_info->IsObject(disk_path))
			{
				disk_info->AddPartition(partition_path);
				break;
			}
		}


		//VARIANT val;
		//CIMTYPE type;
		//long flavor;
		//hres = obj->Get(L"__PATH", 0, &val, &type, &flavor);
		//wprintf_s(L"path=%s\n", val.bstrVal);
	}
	return true;
}

bool CStorageManager::GetPhysicalDisk(IDiskInfo *& disk, UINT index)
{
	JCASSERT(disk == 0 && index < m_disk_list.size());
	CDiskInfo * info = m_disk_list.at(index);
	disk = static_cast<IDiskInfo*>(info);
	if (disk) disk->AddRef();
	return true;
}

bool CreateStorageManager(IStorageManager * & manager)
{
	manager = static_cast<IStorageManager*>(jcvos::CDynamicInstance<CStorageManager>::Create());
	return true;
}

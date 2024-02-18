#include "pch.h"

#include "../include/storage_manager.h"
#include "../include/utility.h"
#include "static_init.h"
#include <lib_authorizer.h>

LOCAL_LOGGER_ENABLE(L"libclone.manager", LOGGER_LEVEL_NOTICE);

#define LOG_CONFIG_FILE L"libclone.cfg"

const std::string g_app_name = "com.kanenka.renewdisk.v01";


///////////////////////////////////////////////////////////////////////////////
// ==== StorageManager ====

//jcvos::CStaticInstance<CStorageManager> global_storage_manager;

IStorageManager * NewStorageManager(void)
{
	IStorageManager * manager = nullptr;
	bool br = CreateStorageManager(manager);
	if (!br || manager == nullptr) throw gcnew System::ApplicationException(
		L"[err] allocation error, failed on creating CStorageManager object");
	return manager;
}

Clone::StorageManager::StorageManager(void) : m_manager(nullptr), m_listener(nullptr)
{
	LOG_STACK_TRACE();
	// set logfile path
	std::wstring fullpath, fn;
	wchar_t dllpath[MAX_PATH] = { 0 };

//	swprintf_s(dllpath, L"msg=%ls", L"hello word");
	EnabledDebugPrivilege();
	m_manager = NewStorageManager();
	LOG_DEBUG(L"msg=%s", L"Initialize COM");
	bool br = m_manager->Initialize(true);
	if (!br) throw gcnew System::ApplicationException(L"[err] failed on initialzing CStorageManager by DLL");
	m_listener = new CDeviceListener;


	
}

Clone::StorageManager::~StorageManager(void)
{
	m_manager->StopListDisk();
	m_manager->Release();
	delete m_listener;
}

bool Clone::StorageManager::ListDisk(void)
{
	JCASSERT(m_manager);
	return m_manager->ListDisk();
}

UINT Clone::StorageManager::GetPhysicalDiskNum(void)
{
	JCASSERT(m_manager);
	return m_manager->GetPhysicalDiskNum();
}

Clone::DiskInfo ^ Clone::StorageManager::GetPhysicalDisk(UINT index)
{
	DiskInfo ^ disk_info = nullptr;
	jcvos::auto_interface<IDiskInfo> disk;
	m_manager->GetPhysicalDisk(disk, index);
	if (disk) disk_info = gcnew DiskInfo(disk);
	return disk_info;
}

bool Clone::StorageManager::RegisterWindow(UINT64 handle)
{
	m_listener->SetCallbackWindow((HWND)handle);
	m_manager->StartListDisk(static_cast<IDeviceChangeListener*>(m_listener));
	return true;
}

bool Clone::StorageManager::LoadConfig(const std::wstring& fn)
{
	return global_init.LoadConfig(fn);

}



///////////////////////////////////////////////////////////////////////////////
// ==== DiskClone ====

Clone::DiskClone::DiskClone(void) : m_disk_clone(nullptr)
{
	m_disk_clone = new CDiskClone;
	m_disk_clone->m_force_MBR = global_init.m_force_mbr;
	m_disk_clone->m_source_is_system = global_init.m_source_is_system;
}

Clone::DiskClone::~DiskClone(void)
{
	delete m_disk_clone;
}

Clone::InvokingProgress ^ Clone::DiskClone::CopyDisk(DiskInfo ^ src_disk, DiskInfo ^ dst_disk)
{
	LOG_STACK_TRACE();
	if (src_disk == nullptr || dst_disk == nullptr) throw gcnew Clone::CloneException(L"src disk or dst disk is null", DR_WRONG_PARAMETER);

	std::vector<CopyStrategy> strategy;
	jcvos::auto_interface<IDiskInfo> ss;
	jcvos::auto_interface<IDiskInfo> dd;
	src_disk->GetDiskInfo(ss);
	dst_disk->GetDiskInfo(dd);
	if (ss == nullptr || dd == nullptr) throw gcnew Clone::CloneException(L"src disk or dst disk is inavailable", DR_WRONG_PARAMETER);

	InvokingProgress^ progress = nullptr;
	try
	{
		// ¼EéLisense
		Authority::CApplicationAuthorize* authorizer = Authority::CApplicationAuthorize::Instance();
		Authority::LICENSE_ERROR_CODE ir = authorizer->CheckAuthority(g_app_name);
		if (ir != Authority::LICENSE_OK)
		{	// ¼Eédevice licese
			LOG_NOTICE(L"delivery license is inavailable");
			const wchar_t* src_name = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(src_disk->model_name)).ToPointer();
			ir = authorizer->CheckDeviceLicense(g_app_name, src_name);
			if (ir != Authority::LICENSE_OK)
			{
				LOG_NOTICE(L"device license for source disk %s is inavailable", src_name);
				const wchar_t* dst_name = (const wchar_t*)(System::Runtime::InteropServices::Marshal::StringToHGlobalUni(dst_disk->model_name)).ToPointer();
				ir = authorizer->CheckDeviceLicense(g_app_name, dst_name);
				if (ir != Authority::LICENSE_OK)
				{
					LOG_NOTICE(L"device license for destination disk %s is inavailable", dst_name);
					throw gcnew Clone::CloneException(L"License inavailable", Authority::LICENSE_EMPTY);
				}
			}		
		}
		LOG_NOTICE(L"License verified");
		LOG_NOTICE(L"start clone, config: force_mbr=%d, source_system=%d, replace_efi=%d",
			m_disk_clone->m_force_MBR, m_disk_clone->m_source_is_system, m_disk_clone->m_replace_efi_part);

		DISKINFO_RESULT res = m_disk_clone->MakeCopyStrategy(strategy, ss, dd);
		if (res != DR_OK) throw gcnew Clone::CloneException(L"failed on creating copy strategy", res);
		jcvos::auto_interface<IJCProgress> pp;
		m_disk_clone->AsyncCopyDisk(pp, strategy, ss, dd);
		progress = gcnew InvokingProgress(pp);
	}
	catch (jcvos::CJCException& err)
	{
		String^ msg = gcnew String(err.WhatT());
		throw gcnew Clone::CloneException(msg, err.GetErrorID());
	}
	return progress;
}


///////////////////////////////////////////////////////////////////////////////
// ==== Invoking progress ====


Clone::InvokingProgress::InvokingProgress(IJCProgress * pp) : m_progress(nullptr)
{
	if (pp == nullptr) throw gcnew System::ApplicationException(L"IJCProgress cannot be null");
	m_progress = pp;
	m_progress->AddRef();
}

Clone::InvokingProgress::~InvokingProgress(void)
{
	m_progress->Release();
}

//Clone::PROGRESS ^ Clone::InvokingProgress::GetProgress(UINT32 timeout)
//{
//	std::wstring status;
//	int percent;
//
//	PROGRESS ^ pp = gcnew PROGRESS;
//
//	pp->result = m_progress->WaitForStatusUpdate(timeout, status, percent);
//	pp->status = gcnew String(status.c_str());
//	pp->progress = (float)((float)percent *100.0 / INT_MAX);
//	if (pp->result >= 0) pp->cur_state = 0;
//	else pp->cur_state = 1;
//
//	return pp;
//}

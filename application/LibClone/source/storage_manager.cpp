#include "pch.h"

#include "../include/storage_manager.h"
#include "../include/utility.h"

#include <lib_authorizer.h>

LOCAL_LOGGER_ENABLE(L"libclone.manager", LOGGER_LEVEL_NOTICE);

#define LOG_CONFIG_FILE L"libclone.cfg"

const std::string g_app_name = "com.kanenka.renewdisk.v01";


///////////////////////////////////////////////////////////////////////////////
// ==== StorageManager ====

IStorageManager * NewStorageManager(void)
{
	IStorageManager * manager = nullptr;
	bool br = CreateStorageManager(manager);
	if (!br || manager == nullptr) throw gcnew System::ApplicationException(
		L"[err] allocation error, failed on creating CStorageManager object");
	return manager;
}

Clone::StorageManager::StorageManager(void) : m_manager(nullptr)
{
	CApplicationAuthorize* authorizer = CApplicationAuthorize::Instance();
	bool br = authorizer->CheckAuthority(g_app_name);
	if (!br) throw gcnew AuthorizeException(L"[err] authorization failed for storage manager");

	// set logfile path
	std::wstring fullpath, fn;
	wchar_t dllpath[MAX_PATH] = { 0 };
	//	GetCurrentDirectory(MAX_PATH, dllpath);
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE this_dll = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	::GetModuleFileName(this_dll, dllpath, MAX_PATH);
	jcvos::ParseFileName(dllpath, fullpath, fn);
	LOGGER_CONFIG(LOG_CONFIG_FILE, fullpath.c_str());

	EnabledDebugPrivilege();
	m_manager = NewStorageManager();
	br = m_manager->Initialize(true);
	if (!br) throw gcnew System::ApplicationException(L"[err] failed on initialzing CStorageManager by DLL");
}

Clone::StorageManager::~StorageManager(void)
{
	m_manager->Release();
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

///////////////////////////////////////////////////////////////////////////////
// ==== DiskClone ====

Clone::DiskClone::DiskClone(void) : m_disk_clone(nullptr)
{
	CApplicationAuthorize* authorizer = CApplicationAuthorize::Instance();
	bool br = authorizer->CheckAuthority(g_app_name);
	if (!br) throw gcnew AuthorizeException(L"[err] authorization failed for disk clone");
	m_disk_clone = new CDiskClone;
}

Clone::DiskClone::~DiskClone(void)
{
	delete m_disk_clone;
}

Clone::InvokingProgress ^ Clone::DiskClone::CopyDisk(DiskInfo ^ src_disk, DiskInfo ^ dst_disk)
{
	std::vector<CopyStrategy> strategy;
	jcvos::auto_interface<IDiskInfo> ss;
	jcvos::auto_interface<IDiskInfo> dd;
	src_disk->GetDiskInfo(ss);
	dst_disk->GetDiskInfo(dd);
	bool br = m_disk_clone->MakeCopyStrategy(strategy, ss, dd);
	jcvos::auto_interface<IJCProgress> pp;
	m_disk_clone->AsyncCopyDisk(pp, strategy, ss, dd);

	InvokingProgress ^ progress = gcnew InvokingProgress(pp);
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

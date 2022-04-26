#include "pch.h"
#include "disk_info.h"
#include "../include/utility.h"
#include "storage_manager.h"

LOCAL_LOGGER_ENABLE(L"volume_info", LOGGER_LEVEL_NOTICE);
namespace prop_tree = boost::property_tree;

CVolumeInfo::CVolumeInfo(void)
{
	m_class_name = L"MSFT_Volume";
}

CVolumeInfo::~CVolumeInfo(void)
{
}

bool CVolumeInfo::GetProperty(VOLUME_PROPERTY & prop) const
{
	prop.m_size = m_size;
	prop.m_empty_size = m_empty_size;

	prop.m_drive_letter = m_logical_disk;
	prop.m_name = m_name;
	prop.m_clust_num = m_clust_num;
	prop.m_clust_size = m_clust_size;
	prop.m_dos_path = m_dos_path;
	prop.m_filesystem = m_filesystem;

	return true;
}

HANDLE CVolumeInfo::OpenVolume(DWORD access, DWORD attr)
{
	std::wstring vol_name = m_name.substr(0, m_name.size()-1);
	LOG_NOTICE(L"open volume %s", vol_name.c_str());
	return CreateFile(vol_name.c_str(), access, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
		attr, NULL);
}

bool CVolumeInfo::CreateShadowVolume(IVolumeInfo *& _shadow)
{
	if (m_name.empty())
	{
		LOG_ERROR(L"[ERR] Invalid volume name");
		return false;
	}
	VSS_ID                  snapshotSetID = { 0 };
	BOOL                    bRetVal = true;
	LOG_NOTICE(L"CreateSnapshotSet...");
	VSS_SNAPSHOT_PROP prop = { 0 };

	try
	{
		auto_unknown<IVssBackupComponents> backup;
		bool br = CreateSnapshotSet(backup, snapshotSetID, prop);
		if (!br || backup == nullptr)  THROW_ERROR(ERR_APP, L"CreateSnapshotSet failed");
		CShadowVolume * shadow = jcvos::CDynamicInstance<CShadowVolume>::Create();

		shadow->m_size = m_size;
		shadow->m_empty_size = m_empty_size;
		shadow->m_clust_size = m_clust_size;
		shadow->m_clust_num = m_clust_num;
		shadow->m_name = prop.m_pwszSnapshotDeviceObject;
		backup.detach(shadow->m_backup);
		_shadow = static_cast<IVolumeInfo*>(shadow);
		VssFreeSnapshotProperties(&prop);
	}
	catch (jcvos::CJCException & err)
	{
		VssFreeSnapshotProperties(&prop);
		throw err;
	}
	return true;
}

bool CVolumeInfo::CreateVolume(CVolumeInfo *& vol, const std::wstring & path, CStorageManager * manager)
{
	JCASSERT(manager && vol == nullptr);
	auto_unknown<IWbemClassObject> obj;
	GetObjectFromPath(obj, path, manager);
	return CreateVolume(vol, obj, manager);
}

bool CVolumeInfo::CreateVolume(CVolumeInfo *& vol, IWbemClassObject * obj, CStorageManager * manager)
{
	vol = jcvos::CDynamicInstance<CVolumeInfo>::Create();
	vol->m_manager = manager;
	vol->CWmiObjectBase::Initialize(obj, manager);
	return true;
}

bool CVolumeInfo::CreateSnapshotSet(IVssBackupComponents *& backup, VSS_ID & snapshotSetId, VSS_SNAPSHOT_PROP & prop)
{
	JCASSERT(backup == nullptr);

	//IVssAsync            *pAsync = NULL;
	HRESULT              hres = S_OK;
	bool                 bRetVal = true;
	auto_unknown<IVssAsync> pPrepare;
	auto_unknown<IVssAsync> pDoShadowCopy;

	hres = CreateVssBackupComponents(&backup);      //Release if no longer needed
	if (hres != S_OK)
	{
		LOG_ERROR(L"CreateVssBackupComponents failed, code=0x%x", hres);
		return false;
	}

	hres = backup->InitializeForBackup();
	if (hres != S_OK)
	{
		LOG_ERROR(L"InitializeForBackup failed, code=0x%x", hres);
		LOG_WIN32_ERROR(L" initializing");
		bRetVal = false;
		goto TheEnd;
	}

	hres = backup->SetBackupState(false, false, VSS_BT_FULL);
	if (hres != S_OK)
	{
		LOG_ERROR(L"Set backup state failed, code=0x%x", hres);
		bRetVal = false;
		goto TheEnd;
	}

	hres = backup->SetContext(VSS_CTX_BACKUP);
	if (hres != S_OK)
	{
		LOG_ERROR(L"IVssBackupComponents SetContext failed, code=0x%x", hres);
		bRetVal = false;
		goto TheEnd;
	}

	wchar_t str_vol[256];
	/////
	hres = backup->StartSnapshotSet(&snapshotSetId);
	if (hres != S_OK)
	{
		LOG_ERROR(L"StartSnapshotSet failed, code=0x%x", hres);
		bRetVal = false;
		goto TheEnd;
	}

	VSS_ID snapshot_id;
	wcscpy_s(str_vol, m_name.c_str());
	hres = backup->AddToSnapshotSet(str_vol, GUID_NULL, &snapshot_id);
	if (hres != S_OK)
	{
		LOG_ERROR(L"AddToSnapshotSet failed, vol_name=%s, code=0x%x", str_vol, hres);
		bRetVal = FALSE;
		goto TheEnd;
	}

	hres = backup->SetBackupState(false, false, VSS_BT_FULL);
	if (hres != S_OK)
	{
		LOG_ERROR(L"SetBackupState failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}

	hres = backup->PrepareForBackup(&pPrepare);
	if (hres != S_OK)
	{
		LOG_ERROR(L"PrepareForBackup failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}

	LOG_NOTICE(L"Preparing for backup...");
	hres = pPrepare->Wait();
	if (hres != S_OK)
	{
		LOG_ERROR(L"IVssAsync Wait failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}

	hres = backup->DoSnapshotSet(&pDoShadowCopy);
	if (hres != S_OK)
	{
		LOG_ERROR(L"DoSnapshotSet failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}

	LOG_NOTICE(L"Taking snapshots...");
	hres = pDoShadowCopy->Wait();
	if (hres != S_OK)
	{
		LOG_ERROR(L"IVssAsync Wait failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}


	LOG_NOTICE(L"Get the snapshot device object from the properties...");
	hres = backup->GetSnapshotProperties(snapshot_id, &prop);
	if (hres != S_OK)
	{
		LOG_ERROR(L"GetSnapshotProperties failed, code=0x%x", hres);
		bRetVal = FALSE;
		goto TheEnd;
	}
	bRetVal = true;
TheEnd:
	return bRetVal;
}

void CVolumeInfo::SetWmiProperty(IWbemClassObject * obj)
{
	prop_tree::wptree prop;
	ReadWmiObject(prop, obj);

	m_size = prop.get<UINT64>(L"Size");
	m_empty_size = prop.get<UINT64>(L"SizeRemaining");

	m_name = prop.get<std::wstring>(L"Path", L"");
	LOG_DEBUG(L"volum path = %s", m_name.c_str());

	wchar_t ll = prop.get<int>(L"DriveLetter", 0);
	if (ll > 0)
	{
		wchar_t str_ll[10];
		swprintf_s(str_ll, L"%c", ll);
		m_logical_disk = str_ll;
		LOG_DEBUG(L"volume drive letter = %s", m_logical_disk.c_str());
	}

	m_filesystem = prop.get<std::wstring>(L"FileSystem", L"");
	std::wstring vol_name = m_name;

	m_clust_size = 0;
	m_clust_num = 0;
	DWORD sector_per_cluster = 0, byte_per_sector = 0, free_clusters = 0, total_clusters = 0;
	BOOL br = GetDiskFreeSpace(vol_name.c_str(), &sector_per_cluster, &byte_per_sector, &free_clusters, &total_clusters);
	if (br)
	{
		m_clust_size = sector_per_cluster * byte_per_sector;
		m_clust_num = total_clusters;
	}
	else	LOG_WIN32_ERROR(L"[err] failed on get volume info");
	
	std::wstring dev_name = m_name.substr(4, vol_name.size() - 5);
	wchar_t path[MAX_PATH];
	DWORD ir = QueryDosDevice(dev_name.c_str(), path, MAX_PATH);
	if (ir == 0)
	{
		LOG_WIN32_ERROR(L"[err] failed on getting dos device of volume %s", dev_name.c_str());
	}
	else
	{
		m_dos_path = path;
	}
}

bool CVolumeInfo::FormatVolume(IWbemClassObject *& vol_obj, const std::wstring & fs_type, const std::wstring & label, bool full, bool compress)
{
	auto_unknown<IWbemClassObject> out_param;
	InvokeMethod(L"Format", out_param, L"FileSystem", fs_type, L"FileSystemLabel", label,
		L"Full", full, L"Compress", compress, L"Force", true);

	CJCVariant return_val;
	HRESULT hres = out_param->Get(BSTR(L"ReturnValue"), 0, &return_val, NULL, 0);
	UINT32 res = return_val.val<UINT32>();
	if (res != 0) THROW_ERROR(ERR_APP, L"failed on creating partion, [%d]:%s", res, GetMSFTErrorMsg(res));

	CJCVariant volume_val;
	CIMTYPE ctype;
	hres = out_param->Get(BSTR(L"FormattedVolume"), 0, &volume_val, &ctype, 0);
	if (FAILED(hres) || ((VARIANT&)volume_val).vt != VT_UNKNOWN)
	{
		THROW_COM_ERROR(hres, L"[warning] failed on getting volume object");
	}
	IUnknown * obj = ((VARIANT&)volume_val).punkVal;	JCASSERT(obj);
	hres = obj->QueryInterface(&vol_obj);
	if (FAILED(hres) || (vol_obj == nullptr))
	{
		THROW_COM_ERROR(hres, L"[warning] failed on getting volume interface");
	}
	return true;
}



///////////////////////////////////////////////////////////////////////////////
// -- shadow volume
CShadowVolume::CShadowVolume(void)
	:m_backup(NULL)
{
}

CShadowVolume::~CShadowVolume(void)
{
	if (m_backup) m_backup->Release();
}

bool CShadowVolume::GetProperty(VOLUME_PROPERTY & prop) const
{
	prop.m_size = m_size;
	prop.m_empty_size = m_empty_size;

	prop.m_drive_letter = m_logical_disk;
	prop.m_name = m_name;
	prop.m_clust_num = m_clust_num;
	prop.m_clust_size = m_clust_size;
	prop.m_dos_path = L"";
	return true;
}

HANDLE CShadowVolume::OpenVolume(DWORD access, DWORD attr)
{
//	std::wstring vol_name = m_name.substr(0, m_name.size() - 1);
	LOG_NOTICE(L"open volume %s", m_name.c_str());

	return CreateFile(m_name.c_str(), access, FILE_SHARE_READ, NULL, OPEN_EXISTING, 
		attr, NULL);
}

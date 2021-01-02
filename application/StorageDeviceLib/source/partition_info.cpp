#include "pch.h"
#include "disk_info.h"
#include "../include/utility.h"
#include "storage_manager.h"

LOCAL_LOGGER_ENABLE(L"partition_info", LOGGER_LEVEL_NOTICE);
namespace prop_tree = boost::property_tree;

CPartitionInfo::CPartitionInfo(void)
	: m_volume(NULL)
{
	m_class_name = L"MSFT_Partition";
}

CPartitionInfo::~CPartitionInfo(void)
{
	RELEASE(m_volume);
}


bool CPartitionInfo::GetVolumeProperty(VOLUME_PROPERTY & prop) const
{
	if (m_volume) return m_volume->GetProperty(prop);
	else return false;
}

bool CPartitionInfo::GetDiskProperty(DISK_PROPERTY & prop) const
{
	return m_parent->GetProperty(prop);
}

bool CPartitionInfo::GetVolume(IVolumeInfo *& vol)
{
	JCASSERT(vol == NULL);
	vol = m_volume;
	if (vol) vol->AddRef();
	return true;
}

bool CPartitionInfo::GetParent(IDiskInfo *& disk)
{
	JCASSERT(disk == NULL);
	disk = static_cast<IDiskInfo*>(m_parent);
	JCASSERT(disk);
	disk->AddRef();
	return true;
}

bool CPartitionInfo::Mount(const std::wstring & str)
{
	UINT32 res;
	if (str.empty())	res = InvokeMethod(L"AddAccessPath", L"AssignDriveLetter", true);
	else res = InvokeMethod(L"AddAccessPath", L"AccessPath", str.c_str());
	if (res != 0)	THROW_ERROR(ERR_APP, L"failed on mounting partition. [%d]:%s", 
		res, GetMSFTErrorMsg(res));
	Refresh();
	return true;
}

bool CPartitionInfo::GetSupportedSize(UINT64 & min_size, UINT64 & max_size, std::wstring & extended)
{
	auto_unknown<IWbemClassObject> out_params;
	InvokeMethod(L"GetSupportedSize", out_params);
	CJCVariant ret_val;
	HRESULT hres = out_params->Get(L"ReturnValue", 0, &ret_val, NULL, 0);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting return value");
	UINT32 res = ret_val.val<UINT32>();
	if (res != 0) THROW_ERROR(ERR_APP, L"failed on getting supported size, [%d]:%s", res,
		GetMSFTErrorMsg(res));

	hres = out_params->Get(L"SizeMin", 0, &ret_val, NULL, 0);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting SizeMin");
	min_size = ret_val.val<UINT64>();

	hres = out_params->Get(L"SizeMax", 0, &ret_val, NULL, 0);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting SizeMax");
	max_size = ret_val.val<UINT64>();

	hres = out_params->Get(L"ExtendedStatus", 0, &ret_val, NULL, 0);
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting ExtendedStatus");
	const wchar_t * str_ext = ret_val.val<const wchar_t*>();
	if (str_ext) extended = str_ext;
	return true;
}


bool CPartitionInfo::Resize(UINT64 new_size)
{
	UINT32 res = InvokeMethod(L"Resize", L"Size", new_size);
	if (res != 0) LOG_ERROR(L"failed on resize partition, size=%lld. [%d]:%s",
		new_size, res, GetMSFTErrorMsg(res));
	Refresh();
	return (res==0);
}

bool CPartitionInfo::ResizeAsync(IJCProgress *& progress, UINT64 new_size)
{
	UINT32 res = InvokeMethodAsync(progress, L"Resize", L"Size", new_size);
	if (res != 0) LOG_ERROR(L"failed on resize partition, size=%lld. [%d]:%s",
		new_size, res, GetMSFTErrorMsg(res));
	return true;
}

bool CPartitionInfo::DeletePartition(void)
{
	JCASSERT(m_parent);
	UINT32 res = InvokeMethod(L"DeleteObject");
	if (res != 0) THROW_ERROR(ERR_APP, L"failed on deleting partition [%d]:%s", res, GetMSFTErrorMsg(res));
	m_parent->RemovePartition(this);
	return true;
}

bool CPartitionInfo::FormatVolume(IVolumeInfo *& new_vol, const std::wstring & fs_type, const std::wstring & label, bool full, bool compress)
{
	if (m_volume == NULL) THROW_ERROR(ERR_APP, L"[err] volume is null");

	auto_unknown<IWbemClassObject> vol_obj;
	bool br = m_volume->FormatVolume(vol_obj, fs_type, label, full, compress);
	if (!br || vol_obj == NULL)
	{
		LOG_ERROR(L"[err] failed on format volume %s", m_volume->m_name.c_str());
		return false;
	}
	jcvos::auto_interface<CVolumeInfo> vol_info;
	br = CVolumeInfo::CreateVolume(vol_info, vol_obj, m_manager);
	if (!br || vol_info == NULL)
	{
		LOG_ERROR(L"[err] failed on creating volume info object");
		return false;
	}
	
	RELEASE(m_volume);
	vol_info.detach(m_volume);
	new_vol = static_cast<IVolumeInfo*>(m_volume);
	new_vol->AddRef();

//	auto_unknown<IWbemClassObject> out_param;
//	InvokeMethod(L"Format", out_param,  L"FileSystem", fs_type, L"FileSystemLabel", label, 
//			L"Full", full, L"Compress", compress, L"Force", true);
//
//	CJCVariant return_val;
//	HRESULT hres = out_param->Get(BSTR(L"ReturnValue"), 0, &return_val, NULL, 0);
//	UINT32 res = return_val.val<UINT32>();
//	if (res != 0) THROW_ERROR(ERR_APP, L"failed on creating partion, [%d]:%s", res, GetMSFTErrorMsg(res));
//
//	CJCVariant volume_val;
//	CIMTYPE ctype;
//	hres = out_param->Get(BSTR(L"FormattedVolume"), 0, &volume_val, &ctype, 0);
//	if (SUCCEEDED(hres) && ((VARIANT&)volume_val).vt == VT_UNKNOWN)
//	{
//		IUnknown * obj = ((VARIANT&)volume_val).punkVal;	JCASSERT(obj);
//		auto_unknown<IWbemClassObject> volume_obj;
//		hres = obj->QueryInterface(&volume_obj);
//		if (SUCCEEDED(hres) && !(volume_obj == NULL))
//		{
//			jcvos::auto_interface<CVolumeInfo> vol_info;
//			bool br = CVolumeInfo::CreateVolume(vol_info, volume_obj, m_manager);
////			bool br = CPartitionInfo::CreatePartitionInfo(part_info, volume_obj, this, m_manager);
//			if (br && vol_info)
//			{
//				RELEASE(m_volume);
//				vol_info.detach(m_volume);
//			}
//		}
//		else LOG_COM_ERROR(hres, L"[warning] failed on getting partition interface");
//	}
//	else LOG_COM_ERROR(hres, L"[warning] failed on getting partition object");
	return true;
}

HANDLE CPartitionInfo::OpenPartition(UINT64 & offset, UINT64 & size, DWORD access)
{
	JCASSERT(m_parent);
	offset = m_prop.m_offset;
	size = m_prop.m_size;
	HANDLE handle = m_parent->OpenDisk(access);
	return handle;
}

void CPartitionInfo::SetWmiProperty(IWbemClassObject * obj)
{
	prop_tree::wptree prop;
	ReadWmiObject(prop, obj);

	auto paths= prop.get_child_optional(L"AccessPaths");
	if (paths)
	{
		for (auto it = paths->begin(); it != paths->end(); ++it)
		{
			std::wstring str_path = it->second.get_value<std::wstring>();
			LOG_DEBUG(L"access paths=%s", str_path.c_str());
		}
	}

	wchar_t ll = prop.get<int>(L"DriveLetter", 0);
	if (ll > 0)
	{
		wchar_t str_ll[10];
		swprintf_s(str_ll, L"%c", ll);
		m_prop.m_drive_letter = str_ll;
		LOG_DEBUG(L"volume drive letter = %s", m_prop.m_drive_letter.c_str());
	}

	m_prop.m_index = prop.get<UINT32>(L"PartitionNumber");
	LOG_DEBUG(L"partition index = %d", m_prop.m_index);
	std::wstring & guid = prop.get<std::wstring>(L"Guid", L"");
	if (!guid.empty())
	{
		LOG_DEBUG(L"Guid=%s", guid.c_str());
		CLSIDFromString(guid.c_str(), &m_prop.m_guid);
	}
	std::wstring & uid = prop.get<std::wstring>(L"UniquaID", L"");
	if (!uid.empty())
	{
		LOG_DEBUG(L"UniqualID=%s", uid.c_str());
	}

	std::wstring & disk_id = prop.get<std::wstring>(L"DiskId", L"");
	LOG_DEBUG(L"DiskId=%s", disk_id.c_str());

	std::wstring & gpt_type = prop.get<std::wstring>(L"GptType", L"");
	if (!gpt_type.empty())
	{
		LOG_DEBUG(L"GptType=%s", gpt_type.c_str());
		CLSIDFromString(gpt_type.c_str(), &m_prop.m_gpt_type);
	}
	m_prop.m_size = prop.get<UINT64>(L"Size");
	m_prop.m_offset = prop.get<UINT64>(L"Offset");
	RELEASE(m_volume);
	ListVolumes();
}

bool CPartitionInfo::CreatePartitionInfo(CPartitionInfo * & part, const std::wstring & path, 
		CDiskInfo * parent,  CStorageManager * manager)
{
	JCASSERT(manager && part==NULL);
	auto_unknown<IWbemClassObject> obj;
	GetObjectFromPath(obj, path, manager);
	return CreatePartitionInfo(part, obj, parent, manager);
}

bool CPartitionInfo::CreatePartitionInfo(CPartitionInfo *& part, IWbemClassObject * obj, 
		CDiskInfo * parent, CStorageManager * manager)
{
	JCASSERT(manager && part==NULL);
	part = jcvos::CDynamicInstance<CPartitionInfo>::Create();
	part->CWmiObjectBase::Initialize(obj, manager);
	part->m_parent = parent;
	return true;
}

bool CPartitionInfo::ListVolumes(void)
{
	JCASSERT(m_manager);
	// 枚举物理磁盘
	HRESULT hres;
	auto_unknown<IEnumWbemClassObject> penum;
	hres = m_manager->m_services->CreateInstanceEnum(BSTR(L"MSFT_PartitionToVolume"), WBEM_FLAG_FORWARD_ONLY, NULL, &penum);
	if (FAILED(hres) || !penum) THROW_COM_ERROR(hres, L"failed on quering physical disk, error=0x%X", hres);
	while (1)
	{
		auto_unknown<IWbemClassObject> obj;
		ULONG count = 0;
		hres = penum->Next(WBEM_INFINITE, 1, &obj, &count);
		if (FAILED(hres))	THROW_COM_ERROR(hres, L"failed on getting physical disk, error=0x%X", hres);
		if (hres == S_FALSE || obj == NULL) break;
		// 处理查询结果
		prop_tree::wptree prop;
		if (!(obj == NULL)) ReadWmiObject(prop, obj);

		std::wstring & volume_path = prop.get<std::wstring>(L"Volume");
		std::wstring & partition_path = prop.get<std::wstring>(L"Partition");

		// find disk
		if (IsObject(partition_path))
		{
			CVolumeInfo::CreateVolume(m_volume, volume_path, m_manager);
			break;
		}
	}
	return true;
}

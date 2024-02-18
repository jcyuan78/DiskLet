#include "pch.h"

#include "../include/storage_device.h"
#include "../include/disk_info.h"

#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "VssApi.lib")
#pragma comment (lib, "Advapi32.lib")
#pragma comment (lib, "wbemuuid.lib")
#pragma comment (lib, "OleAut32.lib")

using namespace Clone;
using namespace System;

#include "../include/utility.h"

LOCAL_LOGGER_ENABLE(L"libclone.diskinfo", LOGGER_LEVEL_NOTICE);

//-----------------------------------------------------------------------------
// -- volume info	
Clone::VolumeInfo::VolumeInfo(IVolumeInfo * vol_info)
{
	JCASSERT(vol_info);
	m_volume = vol_info;
	m_volume->AddRef();

	VOLUME_PROPERTY prop;
	vol_info->GetProperty(prop);

	name = gcnew String(prop.m_name.c_str());
	//m_size = prop.m_size;
	//m_empty_size = prop.m_empty_size;
	capacity = DiskSize(BYTE_TO_SECTOR(prop.m_size));
	free = DiskSize(BYTE_TO_SECTOR(prop.m_empty_size));

	DriveLetter = gcnew String(prop.m_drive_letter.c_str());
	cluster_size = (DWORD)(prop.m_clust_size/1024);
	DosPath = gcnew String(prop.m_dos_path.c_str());
}

//-----------------------------------------------------------------------------
// -- partition info	
Clone::PartInfo::PartInfo(IPartitionInfo * part_info)
{
	JCASSERT(part_info);
	m_part_info = part_info;
	m_part_info->AddRef();
	PARTITION_PROPERTY prop;
	m_part_info->GetProperty(prop);

	index = prop.m_index;
//	m_size = prop.m_size;
	capacity = DiskSize(BYTE_TO_SECTOR(prop.m_size));
//	offset = prop.m_offset;
	offset = DiskSize(BYTE_TO_SECTOR(prop.m_offset));
	DriveLetter = gcnew String(prop.m_drive_letter.c_str());
	wchar_t str[64];
	StringFromGUID2(prop.m_gpt_type, str, 64);
	type_id = gcnew System::Guid(gcnew System::String(str));
	type = GuidToPartitionType(type_id);
	StringFromGUID2(prop.m_guid, str, 64);
	guid = gcnew System::Guid(gcnew System::String(str));
}

DiskInfo ^ Clone::PartInfo::GetDisk(void)
{
	jcvos::auto_interface<IDiskInfo> disk;
	bool br = m_part_info->GetParent(disk);
	if (!br || disk == nullptr) throw gcnew System::ApplicationException(L"failed on getting disk");
	return gcnew DiskInfo(disk);
}

VolumeInfo ^ Clone::PartInfo::GetVolume(void)
{
	jcvos::auto_interface<IVolumeInfo> vol;
	bool br = m_part_info->GetVolume(vol);
	if (!br || vol == NULL) throw gcnew System::ApplicationException(L"volume is null");
	return gcnew VolumeInfo(vol);
}

PartInfo ^ Clone::PartInfo::SetDriveLetter(String ^ drive)
{
	JCASSERT(m_part_info);
	std::wstring letter;
	if (drive)	ToStdString(letter, drive);
	m_part_info->Mount(letter);
	// retry driver letter
	PARTITION_PROPERTY prop;
	m_part_info->GetProperty(prop);
	DriveLetter = gcnew String(prop.m_drive_letter.c_str());

	return this;
}

PartInfo ^ Clone::PartInfo::Resize(UINT64 mb)
{
	UINT64 new_sec = mb * 1024 * 1024;
	JCASSERT(m_part_info);
	m_part_info->Resize(new_sec);

	PARTITION_PROPERTY prop;
	m_part_info->GetProperty(prop);
	//m_size = prop.m_size;
	capacity = DiskSize(BYTE_TO_SECTOR(prop.m_size));
//	offset = prop.m_offset;
	offset = DiskSize(BYTE_TO_SECTOR(prop.m_offset));

	return this;
}

bool Clone::PartInfo::GetPartitionInfo(IPartitionInfo * & part)
{
	JCASSERT(part == NULL);
	part = m_part_info;
	part->AddRef();
	return true;
}

VolumeInfo ^ Clone::PartInfo::Format(FileSystemType type, const std::wstring & label, bool full, bool compress)
{
	JCASSERT(m_part_info);
	std::wstring  str_type;
	ToStdString(str_type, type.ToString());
	jcvos::auto_interface<IVolumeInfo> vol;
	m_part_info->FormatVolume(vol, str_type, label, full, compress);
	VolumeInfo ^ vv = gcnew VolumeInfo(vol);
	return vv;
}

bool Clone::PartInfo::AssignDriveLetter(String ^ letter)
{
	JCASSERT(m_part_info);
	if (letter == nullptr) throw gcnew System::ApplicationException(L"letter cannot be null");
	std::wstring str_letter;
	ToStdString(str_letter, letter);
	m_part_info->Mount(str_letter);
	return true;
}

bool Clone::PartInfo::GetSupportedSize(void)
{
	JCASSERT(m_part_info);
	UINT64 min_size = 0, max_size = 0;
	std::wstring extended;
	m_part_info->GetSupportedSize(min_size, max_size, extended);
	wprintf_s(L"min size=%lld, max_size=%lld, ext=[%s]\n", min_size, max_size, extended.c_str());
	return true;
}


//-----------------------------------------------------------------------------
// -- disk info	
Clone::DiskInfo::DiskInfo(IDiskInfo * disk_info)
{
	JCASSERT(disk_info);
	m_disk_info = disk_info;
	m_disk_info->AddRef();

	DISK_PROPERTY prop;
	m_disk_info->GetProperty(prop);
	m_secs = BYTE_TO_SECTOR(prop.m_size);
	capacity = DiskSize(BYTE_TO_SECTOR(prop.m_size));

	name = gcnew String(prop.m_name.c_str());
	partition = m_disk_info->GetParttionNum();
	disk_id = GUIDToSystemGuid(prop.m_guid);
	index = prop.m_index;
	bus_type = prop.m_bus_type;
	model_name = gcnew String(prop.m_model_name.c_str());
	serial_number = gcnew String(prop.m_serial_num.c_str());
	vendor_name = gcnew String(prop.m_vendor_name.c_str());
	firmware = gcnew String(prop.m_firmware.c_str());

}

Clone::DiskInfo::DiskStatus Clone::DiskInfo::status::get()
{
	DiskStatus st;
	int _st = m_disk_info->GetDiskStatus();
	st = (DiskStatus)(_st);
	return st;
}

bool Clone::DiskInfo::read_only::get()
{
	return m_disk_info->IsReadOnly();
}

void Clone::DiskInfo::read_only::set(bool r)
{
	m_disk_info->SetReadOnly(r);
}


PartInfo ^ Clone::DiskInfo::GetPartition(int index)
{
	JCASSERT(m_disk_info);
	jcvos::auto_interface<IPartitionInfo> partition;
	bool br = m_disk_info->GetPartition(partition, index);
	if (!br || partition == NULL) return nullptr;
	PartInfo ^ part = gcnew PartInfo(partition);
	return part;
}

StorageDevice ^ Clone::DiskInfo::GetStorageDevice(void)
{
	JCASSERT(m_disk_info);
	jcvos::auto_interface<IStorageDevice> storage;
	bool br = m_disk_info->GetStorageDevice(storage);
	if (!br || storage == NULL) throw gcnew System::ApplicationException(L"Failed on getting storage device");
	return gcnew StorageDevice(storage);
}

PartInfo ^ Clone::DiskInfo::CreatePartition(UINT64 offset, UINT64 size, GUID type)
{
	jcvos::auto_interface<IPartitionInfo> out_part;
	bool br = m_disk_info->CreatePartition(out_part, offset, size, NULL, type, { 0 }, 0);
	if (!br || out_part == NULL) return nullptr;
	return gcnew PartInfo(out_part);
}

void Clone::DiskInfo::CacheGptHeader(void)
{
	m_disk_info->CacheGptHeader();
}

void Clone::DiskInfo::SetDiskId(System::Guid ^ guid)
{
	GUID id;
	SystemGuidToGUID(id, guid);
	m_disk_info->SetDiskId(id);
}

void Clone::DiskInfo::SetPartitionId(UINT index, System::Guid ^ guid)
{
	GUID id;
	SystemGuidToGUID(id, guid);
	m_disk_info->SetPartitionId(id, index);
}

void Clone::DiskInfo::SetPartitionType(UINT index, System::Guid ^ type)
{
	GUID id;
	SystemGuidToGUID(id, type);
	m_disk_info->SetPartitionType(id, index);
}

void Clone::DiskInfo::UpdateCache(void)
{
	m_disk_info->UpdateGptHeader();
}




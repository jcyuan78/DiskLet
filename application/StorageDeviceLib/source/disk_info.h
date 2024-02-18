#pragma once

#include "wmi_object_base.h"

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
class CDiskInfo;

enum WMI_DISK_OPERATION_STATUS {
	UNKNOWN = 0, OTHER = 1, OK = 2, DEGRADED = 3, STRESSED = 4,

	//Predictive Failure	5
	DS_ERROR = 6,
	//Non - Recoverable Error	7
	//Starting		8			//The disk is in the process of starting.
	//Stopping		9			//The disk is in the process of stopping.
	//Stopped		10			//The disk was stopped or shut down in a clean and orderly fashion.
	//In Service	11			//The disk is being configured, maintained, cleaned, or otherwise administered.
	//No Contact	12			//The storage provider has knowledge of the disk, but has never been able to establish communication with it.
	//Lost Communication	13	//The storage provider has knowledge of the disk and has contacted it successfully in the past, but the disk is currently unreachable.
	//Aborted		14			//Similar to Stopped, except that the disk stopped abruptly and may require configuration or maintenance.
	//Dormant		15			//The disk is reachable, but it is inactive.
	//Supporting Entity in Error	16		//This status value does not necessarily indicate trouble with the disk, but it does indicate that another device or connection that the disk depends on may need attention.
	//Completed		17			//The disk has completed an operation.This status value should be combined with OK, Error, or Degraded, depending on the outcome of the operation.
	ONLINE = 0xD010,
	NOT_READY = 0xD011,
	NO_MEDIA = 0xD012,
	OFFLINE = 0xD013,
	FAILED = 0xD014,
};

// move to idiskinfo.h

//class PartitionEntry
//{
//public:
//	GUID type;
//	GUID part_id;
//	UINT64 first_lba;
//	UINT64 last_lba;
//	UINT64 attribute;
//	wchar_t name[36];
//};
//
//class GptHeader
//{
//public:
//	char signature[8];
//	DWORD revision;
//	DWORD header_size;
//	DWORD header_crc;
//	DWORD reserved1;
//	UINT64 current_lba;
//	UINT64 backup_lba;
//	UINT64 first_usable;
//	UINT64 last_usable;
//	GUID disk_gid;
//	UINT64 starting_lba;
//	DWORD entry_num;
//	DWORD entry_size;
//	DWORD entry_crc;
//	char reserved2[420];
//};

///////////////////////////////////////////////////////////////////////////////
// -- volume info ----------------------------------------------------------

class CVolumeInfo : public IVolumeInfo, public CWmiObjectBase
{
public:
	CVolumeInfo(void);
	~CVolumeInfo(void);

public:
	friend class CPartitionInfo;

public:
	virtual bool GetProperty(VOLUME_PROPERTY & prop) const ;
	virtual HANDLE OpenVolume(DWORD access, DWORD attr=0);
	virtual bool CreateShadowVolume(IVolumeInfo *& shadow);

protected:
	static bool CreateVolume(CVolumeInfo *& vol, const std::wstring & path, CStorageManager * manager);
	static bool CreateVolume(CVolumeInfo *& vol, IWbemClassObject * obj, CStorageManager * manager);
	bool CreateSnapshotSet(_Out_ IVssBackupComponents*& backup, _Out_ VSS_ID& snapshotSetId, _Out_ VSS_SNAPSHOT_PROP & prop);

	virtual void SetWmiProperty(IWbemClassObject * obj);

	bool FormatVolume(IWbemClassObject * & vol_obj, const std::wstring & fs_type,
		const std::wstring & label, bool full, bool compress);


protected:
	UINT64 m_size;
	UINT64 m_empty_size;

	DWORD m_clust_size;
	DWORD m_clust_num;
//	DEVICE_TYPE m_disk_type;
	std::wstring m_name;	// 由分区GUID构成
	std::wstring m_logical_disk;
	std::wstring m_filesystem;
	std::wstring m_dos_path;
};

class CShadowVolume : public IVolumeInfo
{
public:
	CShadowVolume(void);
	~CShadowVolume(void);

public:
	virtual bool GetProperty(VOLUME_PROPERTY & prop) const ;
	virtual HANDLE OpenVolume(DWORD access, DWORD attr=0);
	virtual bool CreateShadowVolume(IVolumeInfo *& shadow)
	{	// 不支持 shadow
		THROW_ERROR(ERR_APP, L"Cannot create a shadow volume on a shadow");
		return false;
	}
	friend class CVolumeInfo;


protected:
	UINT64 m_size;
	UINT64 m_empty_size;

	DWORD m_clust_size;
	DWORD m_clust_num;
//	DEVICE_TYPE m_disk_type;
	std::wstring m_name;	// 由分区GUID构成
	std::wstring m_logical_disk;
	std::wstring m_filesystem;
//	std::wstring m_dos_path;

	// 仅针对shadow volume
	IVssBackupComponents * m_backup;
};

///////////////////////////////////////////////////////////////////////////////
// -- partition info ----------------------------------------------------------

class CPartitionInfo : public IPartitionInfo, public CWmiObjectBase
{
public:
	CPartitionInfo(void);
	~CPartitionInfo(void);
	static bool CreatePartitionInfo(CPartitionInfo * & part, const std::wstring & path,
		CDiskInfo * parent,  CStorageManager * manager);
	static bool CreatePartitionInfo(CPartitionInfo * & part, IWbemClassObject * obj,
		CDiskInfo * parent,  CStorageManager * manager);

public:
	friend class CDiskInfo;
public:
	virtual UINT GetIndex(void) const { return m_prop.m_index; }
	virtual void GetProperty(PARTITION_PROPERTY & prop) const { prop = m_prop; }
	virtual bool GetVolumeProperty(VOLUME_PROPERTY & prop) const;
	virtual bool GetDiskProperty(DISK_PROPERTY & prop) const;

	virtual bool GetVolume(IVolumeInfo * &vol);
	virtual bool GetParent(IDiskInfo * & disk);
	virtual bool Mount(const std::wstring & str);
	virtual bool GetSupportedSize(UINT64 & min_size, UINT64 & max_size, std::wstring & extended);
	virtual bool Resize(UINT64 new_size);
	virtual bool ResizeAsync(IJCProgress * & progress, UINT64 new_size);

	virtual bool DeletePartition(void);
	virtual bool FormatVolume(IVolumeInfo * & new_vol, const std::wstring & fs_type,
		const std::wstring & label, bool full, bool compress);
	virtual HANDLE OpenPartition(UINT64 & offset, UINT64 & size, DWORD access);


protected:
	virtual void SetWmiProperty(IWbemClassObject * obj);
	bool ListVolumes(void);

protected:
	PARTITION_PROPERTY m_prop;
	CDiskInfo * m_parent;
	CVolumeInfo * m_volume;

};


///////////////////////////////////////////////////////////////////////////////
// -- disk info ---------------------------------------------------------------


class CDiskInfo : public IDiskInfo, public CWmiObjectBase
{
public:
	CDiskInfo(void);
	~CDiskInfo(void);

public:
	friend class CPartitionInfo;
	friend class CStorageManager;
	enum DEVICE_CLASS {
		UNKNOWN=0, SCSI_DEVICE=1, ATA_DEVICE=2, ATA_PASSTHROUGH=3, NVME_DEVICE=4, NVME_PASSTHROUGH = 5,
	};

public:
	virtual bool GetPartition(IPartitionInfo *& part, UINT32 index);
	virtual UINT32 GetIndex(void) { return m_property.m_index; }

	virtual UINT GetParttionNum(void) { return (UINT)(m_partitions.size()); }
	virtual int GetDiskStatus(void);
	virtual bool IsReadOnly(void) const { return m_readonly; }
	virtual void SetReadOnly(bool read_only);
	virtual bool GetProperty(DISK_PROPERTY & prop);

	// -- operation
	virtual bool CreatePartition(IPartitionInfo * & part, UINT64 offset_sec, UINT64 secs,
		const wchar_t *name, const GUID & type, const GUID & diskid, ULONGLONG attribute);
	virtual HANDLE OpenDisk(DWORD access);
	virtual int Online(bool on_off);
	virtual bool ClearDisk(const GUID & guid);

	virtual bool CacheGptHeader(void);
	virtual bool SetDiskId(const GUID & id);
	virtual bool SetPartitionId(const GUID & id, UINT index);
	virtual bool SetPartitionType(const GUID & type, UINT index);
	virtual bool UpdateGptHeader(void);
	virtual void CloseCache(void);
	virtual bool GetStorageDevice(IStorageDevice * & dev);

protected:
	UINT Initialize(IWbemClassObject * obj, CStorageManager * manager);
	virtual void SetWmiProperty(IWbemClassObject * obj);
	// 通过wmi path创建partition
	void AddPartition(const std::wstring & path);
	void AddPartition(CPartitionInfo * part);
	void RefreshPartitionList(void);
	void RemovePartition(CPartitionInfo * part);
	DEVICE_CLASS DetectDevice(IStorageDevice * & dev/*, HANDLE handle*/);

	template <class DEV_TYPE>
	bool CreateStorageDevice(IStorageDevice*& dev, DWORD access = (GENERIC_READ | GENERIC_WRITE) )
	{
		wchar_t path[32];
		swprintf_s(path, L"\\\\.\\PhysicalDrive%d", m_property.m_index);
		HANDLE hdev = CreateFile(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hdev == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"failed on opening device: %s", path);
		DEV_TYPE* _dev = jcvos::CDynamicInstance<DEV_TYPE>::Create();
		if (!_dev) THROW_ERROR(ERR_APP, L"mem full, failed on creating storage device object");
		bool br = _dev->Connect(hdev, true, path, m_property.m_bus_type);
		if (!br)
		{
			LOG_ERROR(L"[err] failed on connecting device to handle.");
			RELEASE(_dev);
			return false;
		}
		dev = static_cast<IStorageDevice*>(_dev);
		return true;
	}

	bool UpdateCRC(void);

public:
	std::wstring m_disk_id;
	std::wstring m_model;
	DWORD m_mbr_signature;
	DWORD m_mbr_checksum;
	//UINT64 m_starting_offset;
	//UINT64 m_usable_length;
	std::wstring m_bus_type;

protected:
	DEVICE_CLASS m_device_class;
	DISK_PROPERTY m_property;
	UINT16 m_operation_status;

	ObjectList<CPartitionInfo> m_partitions;
	bool m_readonly;

	GptHeader * m_header;
	GptHeader* m_secondy_header;
	PartitionEntry * m_pentry;
	PartitionEntry* m_secondy_pentry;
	IStorageDevice* m_sdev;
	//HANDLE m_hdisk;

	bool m_offline;
};


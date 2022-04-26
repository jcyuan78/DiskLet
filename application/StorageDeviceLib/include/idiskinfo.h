#pragma once

#include <stdext.h>
#include <jcparam.h>
#include "istorage_device.h"

typedef UINT64 SEC_SIZE;
class IDiskInfo;

///////////////////////////////////////////////////////////////////////////////
// -- IProgress ：异步任务的工作状态
class IJCProgress : public IJCInterface
{
public:
	enum {PROGRESSING=0};
public:
	virtual int GetProgress(void) = 0;

	// Result: <0:处理中，=0:处理结束，没有错误，>0:处理结束，错误代码
	virtual int GetResult(void) = 0;
	virtual void GetStatus(std::wstring & status) = 0;
//	virtual void WaitComplete(int timeout) = 0;
	virtual int WaitForStatusUpdate(DWORD timeout, std::wstring & status, int & progress) =  0;
};

///////////////////////////////////////////////////////////////////////////////
// -- 
class VOLUME_PROPERTY
{
public:
	std::wstring m_name;
	UINT64 m_size;
	UINT64 m_empty_size;
	std::wstring m_drive_letter;
	std::wstring m_dos_path;
	UINT64 m_clust_num;
	UINT64 m_clust_size;
	std::wstring m_filesystem;
};

class IVolumeInfo : public IJCInterface
{
public:
	virtual bool GetProperty(VOLUME_PROPERTY & prop) const = 0;
	virtual HANDLE OpenVolume(DWORD access, DWORD attr=0) = 0;
	virtual bool CreateShadowVolume(IVolumeInfo *& shadow) = 0;
};

class PARTITION_PROPERTY
{
public:
	UINT m_index;
	UINT64 m_offset;	// in byte
	UINT64 m_size;		// size in byte
	GUID m_guid;
	GUID m_gpt_type;
	std::wstring m_drive_letter;
};

class DISK_PROPERTY
{
public:
	enum PartitionStyle {PARTSTYLE_MBR=1, PARTSTYLE_GPT=2, PARTSTYLE_ROW=0};
	enum BusType {
		BUS_UNKONW = 0, BUS_SCSI = 1, ATAPI = 2,
		BUS_ATA = 3, IEEE1394 = 4, SSA = 5, Fibre_Channel = 6,
		BUS_USB = 7, BUS_RAID = 8, iSCSI = 9, SAS = 10, BUS_SATA = 11,
		SD = 12, MMC = 13, Virtual = 14,
		File_Backed_Virtual = 15, Storage_Spaces = 16,
		BUS_NVMe = 17
	};
public:
	PartitionStyle	m_style;
	UINT32			m_index;
	UINT64			m_size;
	std::wstring	m_name;
	GUID			m_guid;
	BusType			m_bus_type;
	std::wstring	m_model_name;
	std::wstring	m_serial_num;
	std::wstring	m_vendor_name;
	std::wstring	m_firmware;
};

class IPartitionInfo : public IJCInterface
{
public:
	virtual void GetProperty(PARTITION_PROPERTY & prop) const = 0;
	virtual bool GetVolumeProperty(VOLUME_PROPERTY & prop) const = 0;
	virtual bool GetDiskProperty(DISK_PROPERTY & prop) const = 0;

	virtual bool GetVolume(IVolumeInfo * &vol) = 0;
	virtual bool GetParent(IDiskInfo * & disk) = 0;
	virtual bool Mount(const std::wstring & str) = 0;
	virtual bool GetSupportedSize(UINT64 & min_size, UINT64 & max_size, std::wstring & extended) = 0;
	virtual bool Resize(UINT64 new_size) = 0;
	virtual bool ResizeAsync(IJCProgress * & progress, UINT64 new_size) = 0;
	virtual bool DeletePartition(void) = 0;
	virtual HANDLE OpenPartition(UINT64 & offset, UINT64 & size, DWORD access) = 0;
	virtual bool FormatVolume(IVolumeInfo * & new_vol, const std::wstring & fs_type,
		const std::wstring & label, bool full, bool compress)=0;
};

class IDiskInfo : public IJCInterface
{
public:
	enum DiskStatus {
		UNKNOWN = 0, ONLINE = 1, NOT_READY = 2, NO_MEDIA = 3, FAILED = 5, MISSING = 6, OFFLINE = 4,
	};
public:
	// -- property
	virtual bool GetPartition(IPartitionInfo *& part, UINT32 index) = 0;
	virtual UINT32 GetIndex(void) = 0;
	virtual bool GetProperty(DISK_PROPERTY & prop) = 0;
	virtual UINT GetParttionNum(void) = 0;
	virtual int GetDiskStatus(void) = 0;
	virtual bool IsReadOnly(void) const = 0;
	virtual bool GetStorageDevice(IStorageDevice * & dev) = 0;

	virtual void SetReadOnly(bool read_only) = 0;

	// -- operation
	// 设置磁盘状态，treu:online或者false:offilne
	virtual bool CreatePartition(IPartitionInfo * & part, UINT64 offset, UINT64 size, 
		const wchar_t *name, const GUID & type, const GUID & diskid, ULONGLONG attribute) = 0;
	virtual int Online(bool online_offline) = 0;
	virtual bool ClearDisk(const GUID & guid) = 0;
	virtual HANDLE OpenDisk(DWORD access) = 0;
	virtual bool CacheGptHeader(void) = 0;
	virtual bool SetDiskId(const GUID & id) = 0;
	virtual bool SetPartitionId(const GUID & id, UINT index) = 0;
	virtual bool SetPartitionType(const GUID & type, UINT index) = 0;
	virtual bool UpdateGptHeader(void) = 0;
	virtual void CloseCache(void) = 0;
};

class IStorageManager : public IJCInterface
{
public:
	virtual bool Initialize(bool dll = false) = 0;
	virtual bool ListDisk(void) = 0;
	virtual UINT GetPhysicalDiskNum(void) = 0;
	virtual bool GetPhysicalDisk(IDiskInfo * & disk, UINT index) = 0;
};

bool CreateStorageManager(IStorageManager * & manager);


struct CopyStrategy
{
public:
	enum _OPERATION {
		OPERATION = 0xF000,
		COPY_PARTITION = 0x1000, COPY_CREATE_BOOT = 0x3000,
		CLEAR_DISK = 0x4000, CREATE_BOOT_PARTITION = 0x5000,
		MAKE_BOOT = 0x6000, COPY_GUID = 0x7000,

		COPY_METHOD = 0xF00,
		COPY_VOLUME = 0x200, COPY_BY_SECTOR = 0x300,
		COPY_SIZE = 0xF0, COPY_EXPAND = 0x10, COPY_SHRINK = 0x20, COPY_NO_CHANGE = 0x30,
		SYS_PARTITION = 1,
	};

	UINT32 m_op;
	UINT32 m_src_part;		// 源分区的index
	UINT64 src_size;
	UINT64 dst_size;
	UINT64 dst_offset;
};


class IStratageMaker : public IJCInterface
{
public:
	virtual bool SetParameter(const std::wstring & param_name, void * value) = 0;
	virtual bool MakeCopyStrategy(std::vector<CopyStrategy> & strategy, 
		IDiskInfo * src, IDiskInfo * dst, UINT64 disk_size = (-1)) = 0;

};


extern const GUID GPT_TYPE_EFI;
extern const GUID GPT_TYPE_MSR;
extern const GUID GPT_TYPE_BASIC;

class PartitionEntry
{
public:
	GUID type;
	GUID part_id;
	UINT64 first_lba;
	UINT64 last_lba;
	UINT64 attribute;
	wchar_t name[36];
};

class GptHeader
{
public:
	char signature[8];
	DWORD revision;
	DWORD header_size;
	DWORD header_crc;
	DWORD reserved1;
	UINT64 current_lba;
	UINT64 backup_lba;
	UINT64 first_usable;
	UINT64 last_usable;
	GUID disk_gid;
	UINT64 starting_lba;
	DWORD entry_num;
	DWORD entry_size;
	DWORD entry_crc;
	char reserved2[420];
};


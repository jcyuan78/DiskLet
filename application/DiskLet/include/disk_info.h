#pragma once

using namespace System;
using namespace System::Management::Automation;

//#include "../../DiskManageLib/disk_manager_lib.h"
#include "../../StorageDeviceLib/storage_device_lib.h"
#define RELEASE_INTERFACE(ptr) {if (ptr) ptr->Release(); ptr=NULL; 	}

#pragma make_public(IDiskInfo)
#pragma make_public(IVolumeInfo)
#pragma make_public(IPartitionInfo)
#pragma make_public(IStorageDevice)

//#include "jcbuffer.h"

#define SEC_1GB (2*1024*1024LL)
#define SEC_1TB (2*1024*1024*1024LL)
#define SEC_10GB (10*2*1024*1024LL)
#define SEC_10TB (10*2*1024*1024*1024LL)


namespace Clone
{
	ref class DiskInfo;
	ref class StorageDevice;

	public enum class PartitionType {
		EFI_Partition, Microsoft_Reserved, Basic_Data, LDM_Metadata, LDM_Data, Microsoft_Recovery,
		Unkown_Partition, Empty_Partition,
	};


	public enum class FileSystemType {
		ExFAT, FAT, FAT32, NTFS, ReFS
	};

	// DiskSize，表示磁盘大小或者offset的数据结构。
	//	原始数据单位为sector，可转换为MB(int)，GB(float)，TB(float)，缺省为MB
	public value class DiskSize : System::IFormattable
	{
	public:
		DiskSize(const UINT64 ss) : m_secs(ss) {};

	public:
		static DiskSize operator +(DiskSize a, DiskSize b)
		{
			return DiskSize(a.m_secs + b.m_secs);
		}
		static DiskSize operator -(DiskSize a, DiskSize b)
		{
			return DiskSize(a.m_secs - b.m_secs);
		}

		static DiskSize operator +(DiskSize a, UINT64 mb)
		{
			return DiskSize(a.m_secs + MbToSector(mb));
		}

		static DiskSize operator - (DiskSize a, UINT64 mb)
		{
			return DiskSize(a.m_secs - MbToSector(mb));
		}

	public:
		property UINT64 MB {
			UINT64 get() { return SectorToMb(m_secs); }
			void set(UINT64 mm) { m_secs = MbToSector(mm); }
		}
		property float GB {
			float get() { return SectorToGb(m_secs); }
		}
		property float TB {
			float get() { return SectorToTb(m_secs); }
		}
	public:
		virtual String ^ ToString(System::String ^ format, IFormatProvider ^ formaterprovider) 
		{
			if (m_secs > SEC_1TB) return String::Format(L"{0:G4}TB", SectorToTb(m_secs));
			else if (m_secs > SEC_1GB) return String::Format(L"{0:G4}GB", SectorToGb(m_secs));
			else			return String::Format(L"{0}MB", SectorToMb(m_secs));
		}
	protected:
		UINT64 m_secs;
	};



	//-----------------------------------------------------------------------------
	// -- volume info	
	public ref class VolumeInfo : public Object
	{
	public:
		VolumeInfo(IVolumeInfo * part_info);
		!VolumeInfo(void) { RELEASE_INTERFACE(m_volume); }
		~VolumeInfo(void) { RELEASE_INTERFACE(m_volume); }

	public:
		//		DiskInfo ^ GetDisk(void);


	public:
		//		property String ^ name;
		property String ^ name;
		//property float capacity {float get();	};		// 分区大小，以sector单位
		//property float free {float get(); };
		property DiskSize capacity;
		property DiskSize free;

		property String ^ DriveLetter;
		property DWORD cluster_size;	// 以K为单位
		property String ^ DosPath;

	protected:
		IVolumeInfo * m_volume;
		//UINT64 m_size;
		//UINT64 m_empty_size;
	};

	//-----------------------------------------------------------------------------
	// -- partition info	
	public ref class PartInfo : public Object
	{
	public:
		PartInfo(IPartitionInfo * part_info);
		!PartInfo(void) { RELEASE_INTERFACE(m_part_info); }
		~PartInfo(void) { RELEASE_INTERFACE(m_part_info); }

	public:
		DiskInfo ^ GetDisk(void);
		VolumeInfo ^ GetVolume(void);
		PartInfo ^ SetDriveLetter(String ^ drive);
		PartInfo ^ Resize(UINT64 mb);		// MB为单位
		bool GetPartitionInfo(IPartitionInfo * & part);
		VolumeInfo ^ Format(FileSystemType type, const std::wstring & label, bool full, bool compress);
		bool AssignDriveLetter(String ^ letter);
		bool GetSupportedSize(void);

	public:
		//		property String ^ name;
		property UINT	index;
		property DiskSize capacity;
		property DiskSize offset;
		property String ^ DriveLetter;
		property Clone::PartitionType type;
		property System::Guid ^ guid;


	protected:
		IPartitionInfo * m_part_info;
		System::Guid ^	type_id;
	};





	//-----------------------------------------------------------------------------
	// -- disk info	
	public ref class DiskInfo : public Object
	{
	public:
		DiskInfo(IDiskInfo * disk_info);
		~DiskInfo(void)		{	RELEASE_INTERFACE(m_disk_info);		}
		!DiskInfo(void)		{	RELEASE_INTERFACE(m_disk_info);		}

	public:
		enum class DiskStatus {
			UNKNOWN = 0, ONLINE = 1, NOT_READY = 2, NO_MEDIA = 3, FAILED = 5, MISSING = 6, OFFLINE = 4,
		};

	public:
	// -- properties
		PartInfo ^ GetPartition(int index);
		StorageDevice ^ GetStorageDevice(void);

	// -- methods
		PartInfo ^ CreatePartition(UINT64 offset, UINT64 size, GUID type);
		DiskInfo ^ SetReadable(void) { m_disk_info->SetReadOnly(false); return this; }
		DiskStatus OnLine(void) {
			int _st = m_disk_info->Online(true);
			return (DiskStatus)(_st);
		}
		DiskStatus OffLine(void) {
			int _st = m_disk_info->Online(false);
			return (DiskStatus)(_st);
		}
		void ClearDisk(void)	{	m_disk_info->ClearDisk({ 0 });	}


	// -- operations for manual partition.
		void CacheGptHeader(void);
		void SetDiskId(System::Guid ^ guid);
		void SetPartitionId(UINT index, System::Guid  ^ guid);
		void SetPartitionType(UINT index, System::Guid ^ type);
		void UpdateCache(void);

		void GetDiskInfo(IDiskInfo * & info)
		{
			JCASSERT(info == NULL && m_disk_info);
			info = m_disk_info;
			info->AddRef();
		}
//	protected:

	public:
		property UINT32		index;
		property String ^	name;
		property UINT		partition;
		property DiskSize	capacity;

		property DiskStatus status {DiskStatus get(); }
		property bool		read_only {
			bool get();
			void set(bool readonly);
		}
		property Guid ^		disk_id;
		property UINT32		bus_type;
		property String ^	model_name;
		property String ^	serial_number;
		property String ^	vendor_name;
		property String ^	firmware;

	protected:
		UINT64	  m_secs;	// 磁盘大小，sector单位

		IDiskInfo * m_disk_info;
	};



}


Clone::PartitionType GuidToPartitionType(System::Guid^ type_id);

inline System::Guid^ GptTypeToGuid(const Clone::PartitionType type)
{
	System::Guid^ type_id = nullptr;

	switch (type)
	{
	case Clone::PartitionType::EFI_Partition:
		type_id = gcnew System::Guid("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}");
		break;
	case Clone::PartitionType::Microsoft_Reserved:
		type_id = gcnew System::Guid("{e3c9e316-0b5c-4db8-817d-f92df00215ae}");
		break;
	case Clone::PartitionType::Basic_Data:
		type_id = gcnew System::Guid("{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}");
		break;
	case Clone::PartitionType::LDM_Metadata:
		type_id = gcnew System::Guid("{5808c8aa-7e8f-42e0-85d2-e1e90434cfb3}");
		break;
	case Clone::PartitionType::LDM_Data:
		type_id = gcnew System::Guid("{af9b60a0-1431-4f62-bc68-3311714a69ad}");
		break;
	case Clone::PartitionType::Microsoft_Recovery:
		type_id = gcnew System::Guid("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}");
		break;
	default:
		type_id = gcnew System::Guid();
		break;
	}
	return type_id;

}

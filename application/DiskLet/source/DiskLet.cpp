#include "pch.h"

#include "DiskLet.h"
#include "smart-decode.h"
#include "../include/storage_device.h"

#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "VssApi.lib")
#pragma comment (lib, "Advapi32.lib")
#pragma comment (lib, "wbemuuid.lib")
#pragma comment (lib, "OleAut32.lib")

//#include "../include/utility.h"
#include <jccmdlet-comm.h>
#include "global_init.h"


using namespace DiskLet;
using namespace System;

LOCAL_LOGGER_ENABLE(L"disklet", LOGGER_LEVEL_NOTICE);

void DiskLet::DiskLetBase::ShowPipeMessage(void)
{
	jcvos::CMessagePipe * msg_pipe = jcvos::MESSAGE_PIPE::Instance();
	JCASSERT(msg_pipe);
	msg_pipe->WriteMessage(L"\0", 2);

	jcvos::auto_array<wchar_t> str_msg(MAX_MSG_BUF_SIZE);
	while (1)
	{
		size_t ii = msg_pipe->ReadMessage(str_msg, MAX_MSG_BUF_SIZE);
		if (str_msg[0] == 0) break;
		String ^ msg = gcnew String(str_msg);
		WriteVerbose(msg);
	}
}

void DiskLet::DiskLetBase::ShowProgress(IJCProgress * progress, int id, String ^ activity)
{
	ProgressRecord ^ running = gcnew ProgressRecord(id, activity, L"resizing");
	running->RecordType = ProgressRecordType::Processing;
	while (1)
	{
		int pp = progress->GetProgress();
		pp /= (INT_MAX / 100);
		std::wstring status;
		progress->GetStatus(status);
//		wprintf_s(L"progress = %d %%\r", pp);
		if (!status.empty())
		{
			String ^ ss = gcnew String(status.c_str());
			running->StatusDescription = ss;
		}
		running->PercentComplete = pp;
		WriteProgress(running);
		Sleep(500);
		int res = progress->GetResult();
		LOG_DEBUG(L"check progress = %d, status = %d", pp, res);
		if (res >= 0) break;
	}
	running->RecordType = ProgressRecordType::Completed;
}

void DiskLet::DiskCmdBase::GetDisk(IDiskInfo *& disk)
{
	if (Disk)
	{
		Disk->GetDiskInfo(disk);
	}
	else if (DiskNumber >= 0)
	{
		UINT disk_num = global.m_manager->GetPhysicalDiskNum();
		if (DiskNumber < 0 || DiskNumber >= disk_num) throw gcnew System::ApplicationException(
			String::Format(L"Invald index {0}. Index should be in 0 ~ {1}", DiskNumber, disk_num - 1));
		global.m_manager->GetPhysicalDisk(disk, DiskNumber);
	}
	else
	{
		global.GetDisk(disk);
	}

	if (disk == nullptr) throw gcnew System::ApplicationException(L"Invalid disk object (NULL)");
}

void DiskLetBase::ProcessRecord()
{
	try
	{
		InternalProcessRecord();
	}
	catch (std::exception & err)
	{
		System::String ^ msg = gcnew System::String(err.what());
		System::Exception ^ exp = gcnew PipelineStoppedException(msg);
		ErrorRecord ^er = gcnew	ErrorRecord(exp, L"stderr", ErrorCategory::FromStdErr, this);
		WriteError(er);
	}
	ShowPipeMessage();
}

void GetDisks::InternalProcessRecord()
{
	global.m_manager->ListDisk();
	UINT disk_num = global.m_manager->GetPhysicalDiskNum();
	if (index < disk_num)
	{
		jcvos::auto_interface<IDiskInfo> disk;
		bool br = global.m_manager->GetPhysicalDisk(disk, index);
		if (!br || !disk)
		{
			String ^ err = String::Format(L"[err] failed on getting disk[{0}]", index);
			throw gcnew System::ApplicationException(err);
		}
		Clone::DiskInfo ^ info = gcnew Clone::DiskInfo(disk);
		WriteObject(info);
	}
	else
	{
		for (UINT ii = 0; ii < disk_num; ++ii)
		{
			jcvos::auto_interface<IDiskInfo> disk;
			bool br = global.m_manager->GetPhysicalDisk(disk, ii);
			if (!br || !disk)
			{
				LOG_ERROR(L"[err] failed on getting disk[%d]", ii);
				continue;
			}
			Clone::DiskInfo ^ info = gcnew Clone::DiskInfo(disk);
			WriteObject(info);
		}
	}
}

void DiskLet::GetDiskPartition::InternalProcessRecord()
{
	jcvos::auto_interface<IDiskInfo> disk;
	GetDisk(disk);
	if (!disk) throw gcnew System::ApplicationException(L"Invalid disk object (NULL)");

	UINT part_num = disk->GetParttionNum();
	if (PartitionNumber == 0 || PartitionNumber > part_num)
	{
		for (UINT ii = 1; ii <= part_num; ++ii)
		{
			jcvos::auto_interface<IPartitionInfo> part;
			disk->GetPartition(part, ii);
			if (part)
			{
				Clone::PartInfo ^ pp = gcnew Clone::PartInfo(part);
				WriteObject(pp);
			}
		}
	}
	else
	{
		jcvos::auto_interface<IPartitionInfo> part;
		disk->GetPartition(part, PartitionNumber);
		if (part) WriteObject(gcnew Clone::PartInfo(part));
	}
}

void DiskLet::NewPartition::InternalProcessRecord()
{
	jcvos::auto_interface<IDiskInfo> disk;
	GetDisk(disk);
	//throw gcnew System::NotImplementedException();
//	JCASSERT(disk != nullptr);
	if (disk==nullptr) throw gcnew System::ApplicationException(L"Invalid disk object (NULL)");

	GUID type_id;
	SystemGuidToGUID(type_id, GptTypeToGuid(type));

	UINT64 offset = (start_mb == (UINT64)(-1)) ? (UINT64)(-1) : (start_mb * 1024 * 1024);
	UINT64 size = size_mb * 1024 * 1024;

	jcvos::auto_interface<IPartitionInfo> out_part;

	bool br = disk->CreatePartition(out_part, offset, size, NULL, type_id, { 0 }, 0);
	if (!br || out_part == nullptr) throw gcnew System::ApplicationException(L"failed on creating partition");
	WriteObject(gcnew Clone::PartInfo(out_part));

	//PartInfo ^ part = disk->CreatePartition(offset, size, type_id);
	//if (part) WriteObject(part);
	//else
	//{
	//	wprintf_s(L"no partition returns, please refresh the disk\n");
	//}
}

void DiskLet::CopyPartition::InternalProcessRecord()
{
	if (!src) throw gcnew System::ApplicationException(L"missing sorce disk");
	if (!dst && filename==nullptr) throw gcnew System::ApplicationException(L"missing destination disk");
	if (dst && filename) throw gcnew System::ApplicationException(L"cannot specify both destination and file.");

	jcvos::auto_interface<IPartitionInfo> src_part;
	src->GetPartitionInfo(src_part);
	if (!src_part) throw gcnew System::ApplicationException(L"failed on getting src partition");
	jcvos::auto_interface<IDiskInfo> src_disk;
	src_part->GetParent(src_disk);
	if (!src_disk) throw gcnew System::ApplicationException(L"failed on getting src disk");
	jcvos::auto_interface<IStorageDevice> src_dev;
	src_disk->GetStorageDevice(src_dev);
	if (!src_dev ) throw gcnew System::ApplicationException(L"failed on getting storage device");
	jcvos::auto_interface<IVolumeInfo> src_vol;
	src_part->GetVolume(src_vol);

	jcvos::auto_interface<IPartitionInfo> dst_part;
	if (dst) dst->GetPartitionInfo(dst_part);
	if (!dst_part ) throw gcnew System::ApplicationException(L"failed on getting dst partition");
	jcvos::auto_interface<IDiskInfo> dst_disk;
	dst_part->GetParent(dst_disk);
	if (!dst_disk ) throw gcnew System::ApplicationException(L"failed on getting dst disk");
	jcvos::auto_interface<IStorageDevice> dst_dev;
	dst_disk->GetStorageDevice(dst_dev);
	if (!dst_dev ) throw gcnew System::ApplicationException(L"failed on getting storage device");



	bool using_map = !(SectorBySector.ToBool() );
	bool shadow = Shadow.ToBool();

	CDiskClone clone;
	clone.SetDevice(src_dev, dst_dev);
	jcvos::auto_interface<IJCProgress> progress;

	if (CopyVolume)
	{
		if (ByFile.ToBool())
		{
			clone.AsyncCopyFile(progress, src_part, dst_part, shadow);
		}
		else
		{
			if (dst) clone.AsyncCopyVolumeBySector(progress, src_vol, dst_part, shadow, using_map);
			else if (filename)
			{
				std::wstring str_fn;
				ToStdString(str_fn, filename);
				clone.AsyncCopyVolumeToFile(progress, src_vol, str_fn);
			}
		}
	}
	else
	{
		clone.AsyncCopyPartition(progress, src_part, dst_part);
	}

	ProgressRecord ^ running = gcnew ProgressRecord(100, L"Copy partition", L"copying");
	running->RecordType = ProgressRecordType::Processing;
	while (1)
	{
		int pp = progress->GetProgress();
		pp /= (INT_MAX / 100);
		std::wstring status;
		progress->GetStatus(status);
		if (!status.empty())
		{
			String ^ ss = gcnew String(status.c_str());
			//		running->CurrentOperation = ss;
			running->StatusDescription = ss;
		}
		running->PercentComplete = pp;
		WriteProgress(running);
		Sleep(500);
		int res = progress->GetResult();
		if (res >= 0) break;
	}
	running->RecordType = ProgressRecordType::Completed;
}

void DiskLet::SetPartitionType::InternalProcessRecord()
{
//	throw gcnew System::NotImplementedException();
	if (!partition) throw gcnew System::ApplicationException(L"missing sorce disk");
	Clone::DiskInfo ^ disk = partition->GetDisk();
	if (!disk) throw gcnew System::ApplicationException(L"parent disk is null");

	disk->CacheGptHeader();
	System::Guid ^ type_id = GptTypeToGuid(type);
	disk->SetPartitionType(partition->index, type_id);
	disk->UpdateCache();
}

void DiskLet::FormatPartition::InternalProcessRecord()
{
	if (Partition == nullptr) throw gcnew System::ApplicationException(L"partition cannot be null");
	jcvos::auto_interface<IPartitionInfo> partition;
	Partition->GetPartitionInfo(partition);
	if (partition==nullptr)  throw gcnew System::ApplicationException(L"partition cannot be null");
	std::wstring str_label;
	if (Label) ToStdString(str_label, Label);
	std::wstring  str_type;
	ToStdString(str_type, FileSystem.ToString());

	jcvos::auto_interface<IVolumeInfo> volume;
	partition->FormatVolume(volume, str_type, str_label, !(QuickFormat), Compress.ToBool());
//	Clone::VolumeInfo ^ vol = Partition->Format(FileSystem, str_label, !(QuickFormat), Compress.ToBool());
	Clone::VolumeInfo ^ vol = gcnew Clone::VolumeInfo(volume);
	WriteObject(vol);
}

void DiskLet::ClearDisk::InternalProcessRecord()
{
	if (Disk == nullptr)
	{
		UINT disk_num = global.m_manager->GetPhysicalDiskNum();
		if (Number < 0 || Number >= disk_num) throw gcnew System::ApplicationException(
			String::Format(L"Invald index {0}. Index should be in 0 ~ {1}", Number, disk_num-1) );
		jcvos::auto_interface<IDiskInfo> disk;
		global.m_manager->GetPhysicalDisk(disk, Number);
		Disk = gcnew Clone::DiskInfo(disk);
	}
	//else
	//{
	//	Disk->GetDiskInfo(disk);
	//}
	if (Disk == nullptr) throw gcnew System::ApplicationException(L"invalid disk info");

	if (ShouldProcess(L"Clear the disk?"))
	{
		if (ShouldContinue(L"Are you sure to cleare all data?", 
				String::Format(L"This command will clear disk[{0}]: {1}", Disk->index, Disk->name))
		)
		{
			Disk->ClearDisk();
//			disk->ClearDisk({ 0 });
		}
		else
		{
			wprintf_s(L"User cancel the processing\n");
		}
	}
	//else
	//{
	//	wprintf_s(L"user cancel the should process\n");
	//}
//	throw gcnew System::NotImplementedException();
}

//void DiskLet::LongTimeTest::InternalProcessRecord()
//{
//	ProgressRecord ^ running = gcnew ProgressRecord(100, L"running", L"working");
//	WriteProgress(running);
//	running->RecordType = ProgressRecordType::Processing;
//	for (int ii = 0; ii < 100; ii++)
//	{
//		running->PercentComplete = ii;
//		WriteProgress(running);
//		Sleep(100);
//	}
//	running->RecordType = ProgressRecordType::Completed;
////	throw gcnew System::NotImplementedException();
//}

void DiskLet::SetPartitionSize::InternalProcessRecord()
{
//	throw gcnew System::NotImplementedException();
	jcvos::auto_interface<IPartitionInfo> part;
	if (Partition == nullptr)
	{
		jcvos::auto_interface<IDiskInfo> disk;
		UINT disk_num = global.m_manager->GetPhysicalDiskNum();
		if (DiskNumber < 0 || DiskNumber >= disk_num) throw gcnew System::ApplicationException(
			String::Format(L"Invald disk index {0}. Index should be in 0 ~ {1}", DiskNumber, disk_num - 1));
		global.m_manager->GetPhysicalDisk(disk, DiskNumber);

		UINT part_num = disk->GetParttionNum();
		if (Index <= 0 || Index >= part_num) throw gcnew System::ApplicationException(
			String::Format(L"Invalid partition index {0}, Index should be in 1~{1}", Index, part_num) );

		disk->GetPartition(part, Index);
	}
	else
	{
		Partition->GetPartitionInfo(part);
	}
	if (!part) throw gcnew System::ApplicationException(L"Invalid partition object (NULL)");
	UINT64 size_byte = size_mb * 1024 * 1024;

	jcvos::auto_interface<IJCProgress> progress;
	part->ResizeAsync(progress, size_byte);
	ShowProgress(progress, 101, L"Resize Partition");
}

void DiskLet::DecodeSmart::InternalProcessRecord()
{
	if (!data) throw gcnew System::ApplicationException(L"missing input data");
	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf;
	data->GetData(ibuf);
	if (!ibuf) throw gcnew System::ApplicationException(L"no data in input buffer");
	BYTE * _buf = ibuf->Lock();
	jcvos::auto_array<CSmartAttribute> attri(MAX_SMART_ATTRI_NUM, 0);
	//CSmartAttribute *attri = new CSmartAttribute[MAX_SMART_ATTRI_NUM];
	//memset(attri, 0, sizeof(CSmartAttribute)*MAX_SMART_ATTRI_NUM);
	size_t attri_num = SmartDecodeById(attri, MAX_SMART_ATTRI_NUM, _buf, ibuf->GetSize());
	ibuf->Unlock(_buf);

	PSObject ^ list = gcnew PSObject;
	for (size_t ii = 0; ii < attri_num; ++ii)
	{
		BYTE id = attri[ii].m_id;
		String ^ name = String::Format(L"ID_{0:X2}", id);
		PSNoteProperty ^ item;
		switch (id)
		{
		case 0xC2: {// 温度
			String ^ val = String::Format(L"CUR:{0}; MIN:{1}; MAX:{2}",
				attri[ii].m_raw[0], attri[ii].m_raw[2], attri[ii].m_raw[4]);
			item = gcnew PSNoteProperty(name, val);
			break;					}
		default:
			item = gcnew PSNoteProperty(name, attri[ii].m_raw_val);
		}

		list->Members->Add(item);
	}
	WriteObject(list);
}

void DiskLet::GetLogPage::InternalProcessRecord()
{
	if (dev == nullptr)
	{
		jcvos::auto_interface<IStorageDevice> dd;
		global.GetDevice(dd);
		if (!dd) throw gcnew System::ApplicationException(L"device is not selected");
		dev = gcnew Clone::StorageDevice(dd);
		//dev = global.GetDevice();
	}
	JcCmdLet::BinaryType ^ data = dev->GetLogPage(id, secs);
	WriteObject(data);
}

void DiskLet::SelectDevice::InternalProcessRecord()
{
	global.m_manager->ListDisk();
	UINT disk_num = global.m_manager->GetPhysicalDiskNum();

	jcvos::auto_interface<IDiskInfo> disk;
	bool br = global.m_manager->GetPhysicalDisk(disk, index);
	if (!br || !disk)
	{
		String ^ err = String::Format(L"[err] failed on getting disk[{0}]", index);
		throw gcnew System::ApplicationException(err);
	}
	Clone::DiskInfo ^ info = gcnew Clone::DiskInfo(disk);
//	WriteObject(info);

	jcvos::auto_interface<IStorageDevice> dev;
	br = disk->GetStorageDevice(dev);
	if (!br || dev == nullptr)
	{
		String ^ err = String::Format(L"[err] failed on getting device of disk[{0}]", index);
		throw gcnew System::ApplicationException(err);
	}
	global.SelectDevice(dev);
	Clone::StorageDevice ^device = gcnew Clone::StorageDevice(dev);
	WriteObject(device);
}

void DiskLet::SelectDisk::InternalProcessRecord()
{
	global.m_manager->ListDisk();
	UINT disk_num = global.m_manager->GetPhysicalDiskNum();

	jcvos::auto_interface<IDiskInfo> disk;
	bool br = global.m_manager->GetPhysicalDisk(disk, index);
	if (!br || !disk)
	{
		String ^ err = String::Format(L"[err] failed on getting disk[{0}]", index);
		throw gcnew System::ApplicationException(err);
	}
	Clone::DiskInfo ^ info = gcnew Clone::DiskInfo(disk);
	WriteObject(info);

	global.SelectDisk(disk);
}

#pragma once

using namespace System;
using namespace System::Management::Automation;

//#pragma make_public(JcCmdLet::BinaryType)

//#include "DiskInfo.h"
//#include "storage_device.h"
#include "global_init.h"
#include "../include/disk_info.h"


//extern StaticInit global;

namespace DiskLet
{
	// -- my cmdlet base class, to handle exceptions	
	//  所有DiskLet中CmdLet的基础类，提供基础服务
	public ref class DiskLetBase : public Cmdlet
	{
	public:
		virtual void InternalProcessRecord() {};
	protected:
		void ShowPipeMessage(void);

	protected:
		void ShowProgress(IJCProgress * progress, int id, String ^ activity);
	protected:
		virtual void ProcessRecord() override;
	};

	// 基于Disk操作的基础类，提供Disk选择的公共参数
	public ref class DiskCmdBase : public DiskLetBase
	{
	public:
		DiskCmdBase(void)
		{
			Disk = nullptr;
			DiskNumber = -1;
		}
	public:
		[Parameter(/*Position = 0, ParameterSetName = "ByObject",*/ 
			/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = false,
			HelpMessage = "specify disk")]
		property Clone::DiskInfo ^ Disk;

		[Parameter(/*Position = 0, */Mandatory = false, /*ParameterSetName = "ByIndex",*/
			/*ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,*/
			HelpMessage = "Clear a disk")]
		property int DiskNumber;

	protected:
		void GetDisk(IDiskInfo * & disk);
	};



	[CmdletAttribute(VerbsCommon::Get, "Disks")]
	public ref class GetDisks : public DiskLetBase
	{
	public:
		GetDisks(void) { index = -1; };
		~GetDisks(void) {};

	public:
		[Parameter(Position = 0, HelpMessage = "specify the index of disk, default returns all disks")]
		property UINT32 index;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Get, "DiskPartition")]
	public ref class GetDiskPartition : public DiskCmdBase
	{
	public:
		GetDiskPartition(void) { PartitionNumber = 0; };
		~GetDiskPartition(void) {};

	public:
		[Parameter(Position = 1, HelpMessage = "specify the index of partition, default returns all disks")]
		property UINT32 PartitionNumber;

	public:
		virtual void InternalProcessRecord() override;
	};



	[CmdletAttribute(VerbsCommon::New, "DiskPartition")]
	public ref class NewPartition : public DiskCmdBase
	{
	public:
		NewPartition(void) { 
			start_mb = UINT64(-1); size_mb = 0; type = Clone::PartitionType::Basic_Data; };
		~NewPartition(void) {};

	public:

		[Parameter(Position = 1,
			HelpMessage = "partition offset in MB")]
		property UINT64 start_mb;

		[Parameter(Position = 2,
			HelpMessage = "partition size in MB")]
		property size_t size_mb;

		[Parameter(Position = 3,
			HelpMessage = "partition size in BYTE")]
		property Clone::PartitionType type;

	public:
		virtual void InternalProcessRecord() override;
	};



	[CmdletAttribute(VerbsCommon::Copy, "Partition")]
	public ref class CopyPartition : public DiskLetBase
	{
	public:
		CopyPartition(void) { };
		~CopyPartition(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			HelpMessage = "source disk for copy")]
		property Clone::PartInfo ^ src;

		[Parameter(Position = 1, Mandatory = true, ParameterSetName = "ToDisk",
			HelpMessage = "destination disk for copy")]
		property Clone::PartInfo ^ dst;

		[Parameter(Position = 2, Mandatory = true, ParameterSetName ="ToFile",
			HelpMessage = "Copy to file")]
		property String ^ filename;


		[Parameter(Position = 3, HelpMessage = "copy as a volume")]
		property SwitchParameter CopyVolume;
		[Parameter(Position = 4, HelpMessage = "copy sector by sector")]
		property SwitchParameter SectorBySector;
		[Parameter(Position = 5, HelpMessage = "copy file by file")]
		property SwitchParameter ByFile;
		[Parameter(Position = 6, HelpMessage = "using shadow")]
		property SwitchParameter Shadow;


	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Set, "PartitionSize")]
	public ref class SetPartitionSize : public DiskLetBase
	{
	public:
		SetPartitionSize(void) { Partition = nullptr; };
		~SetPartitionSize(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true, ParameterSetName = "ByObject",
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true, 
			HelpMessage = "partition for set type")]
		property Clone::PartInfo ^ Partition;

		[Parameter(Position = 0, Mandatory = true, ParameterSetName = "ByIndex",
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "Index of disk")]
		property int DiskNumber;

		[Parameter(Position = 1, Mandatory = true, ParameterSetName = "ByIndex",
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "Index of partition")]
		property int Index;

		[Parameter(Position = 1, HelpMessage = "partition type to set")]
		property UINT64 size_mb;

	public:
		virtual void InternalProcessRecord() override;
	};


	[CmdletAttribute(VerbsCommon::Set, "PartitionType")]
	public ref class SetPartitionType : public DiskLetBase
	{
	public:
		SetPartitionType(void) { partition = nullptr; };
		~SetPartitionType(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true, 
			HelpMessage = "partition for set type")]
		property Clone::PartInfo ^ partition;

		[Parameter(Position = 1, HelpMessage = "partition type to set")]
		property Clone::PartitionType type;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Format, "Partition")]
	public ref class FormatPartition : public DiskLetBase
	{
	public:
		FormatPartition(void) { QuickFormat = true; };
		~FormatPartition(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true, 
			HelpMessage = "partition for set type")]
		property Clone::PartInfo ^ Partition;

		[Parameter(Position = 1, Mandatory = true, HelpMessage = "typeof file system")]
		property Clone::FileSystemType FileSystem;

		[Parameter(Position = 2, HelpMessage = "label of the volume")]
		property String ^ Label;

		[Parameter(Position = 3, HelpMessage = "compress the file system")]
		property SwitchParameter Compress;

		[Parameter(Position = 4, HelpMessage = "quick format or full format")]
		property bool QuickFormat;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Clear, "DiskDisk", SupportsShouldProcess =true)]
	public ref class ClearDisk : public DiskLetBase
	{
	public:
		ClearDisk(void) { Number = -1; };
		~ClearDisk(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true, ParameterSetName = "ByObject",
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "Clear a disk")]
		property Clone::DiskInfo ^ Disk;

		[Parameter(Position = 0, Mandatory = true, ParameterSetName = "ByIndex",
			ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			HelpMessage = "Index of disk")]
		property int Number;


	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- storage device
	[CmdletAttribute(VerbsCommon::Select, "Disk")]
	public ref class SelectDisk : public DiskLetBase
	{
	public:
		SelectDisk(void) { index = 0; };
		~SelectDisk(void) {};

	public:
		[Parameter(Position = 0, HelpMessage = "disk index")]
		property UINT index;

	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- smart decode
	[CmdletAttribute(VerbsCommon::Format, "Smart")]
	public ref class DecodeSmart : public DiskLetBase
	{
	public:
		DecodeSmart(void) {};
		~DecodeSmart(void) {};
	public:
		[Parameter(Position = 0,
			HelpMessage = "controller data type"
		)]
		property String^ by_pos;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true)]
		property JcCmdLet::BinaryType ^ data;

	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- storage device
	[CmdletAttribute(VerbsCommon::Select, "Device")]
	public ref class SelectDevice : public DiskLetBase
	{
	public:
		SelectDevice(void) { index = 0; };
		~SelectDevice(void) {};

	public:
		[Parameter(Position = 0, HelpMessage = "disk index")]
		property UINT index;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Get, "LogPage")]
	public ref class GetLogPage : public DiskLetBase
	{
	public:
		GetLogPage(void) {dev = nullptr; };
		~GetLogPage(void) {};
	public:
		[Parameter(Position = 0, HelpMessage = "controller data type")]
		property Clone::StorageDevice ^ dev;

		[Parameter(Position = 1, Mandatory = true, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true,
			HelpMessage = "log page id")]
		property BYTE id;

		[Parameter(Position = 2, Mandatory = true, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true,
			HelpMessage = "data size in sector")]
		property size_t secs;

	public:
		virtual void InternalProcessRecord() override;
	};
};	

//
//void SystemGuidToGUID(GUID & out_id, System::Guid ^ in_id);
//System::Guid ^ GUIDToSystemGuid(const GUID & in_id);
//
//
//Clone::PartitionType GuidToPartitionType(System::Guid ^ type_id);
//System::Guid ^ GptTypeToGuid(const Clone::PartitionType & type);
//
//
//void ToStdString(std::wstring & dst, System::String ^ src);

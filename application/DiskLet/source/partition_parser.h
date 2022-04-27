#pragma once
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <storage_device_lib.h>
#include "../include/disk_info.h"
#include <jccmdlet-comm.h>

using namespace System;
using namespace System::Management::Automation;



namespace DiskLet
{

	public value class LbaType : System::IFormattable
	{
	public:
		LbaType(const UINT64 ss) : m_secs(ss) {};

	public:
		static LbaType operator +(LbaType a, LbaType b)
		{
			return LbaType(a.m_secs + b.m_secs);
		}
		static LbaType operator -(LbaType a, LbaType b)
		{
			return LbaType(a.m_secs - b.m_secs);
		}

		static LbaType operator +(LbaType a, UINT64 mb)
		{
			return LbaType(a.m_secs + MbToSector(mb));
		}

		static LbaType operator - (LbaType a, UINT64 mb)
		{
			return LbaType(a.m_secs - MbToSector(mb));
		}
//		static LbaType^ operator = (UINT64 lba) { return LbaType(lba); }

	public:
		//property UINT64 MB {
		//	UINT64 get() { return SectorToMb(m_secs); }
		//	void set(UINT64 mm) { m_secs = MbToSector(mm); }
		//}
		//property float GB {
		//	float get() { return SectorToGb(m_secs); }
		//}
		//property float TB {
		//	float get() { return SectorToTb(m_secs); }
		//}
	public:
		virtual String^ ToString(System::String^ format, IFormatProvider^ formaterprovider)
		{
			return String::Format(L"{0:X08}", m_secs);
		}
	protected:
		UINT64 m_secs;
	};


	public ref class PartitionInfo: public Object
	{
	public:
		PartitionInfo(PartitionEntry& p)
		{
			part_id = GUIDToSystemGuid(p.part_id);
			Guid^ type_id = GUIDToSystemGuid(p.type);
			type = GuidToPartitionType(type_id);
			first_lba = LbaType(p.first_lba);
			last_lba = LbaType(p.last_lba);
		}
	public:
		Clone::PartitionType type;
		Guid ^ part_id;
		LbaType first_lba;
		LbaType last_lba;
		String^ name;
	};

	public ref class GptInfo : public Object
	{
	public:
		GptInfo(const GptHeader& g)
		{
			current_lba = LbaType(g.current_lba);
			backup_lba = LbaType(g.backup_lba);
			first_usable = LbaType(g.first_usable);
			last_usable = LbaType(g.last_usable);
			disk_id = GUIDToSystemGuid(g.disk_gid);
			entry_num = g.entry_num;
			partition_list = gcnew Collections::ArrayList;
		}
	public:
		void AddPartition(PartitionInfo^ part)
		{
			partition_list->Add(part);
		}
	public:
		LbaType current_lba;
		LbaType backup_lba;
		LbaType first_usable;
		LbaType last_usable;
		UINT entry_num;
		Guid^ disk_id;

		System::Collections::ArrayList^ partition_list;
	};


	[CmdletAttribute(VerbsData::ConvertTo, "PartitionInfo")]
	public ref class ConvertToPartitionInfo : public JcCmdLet::JcCmdletBase
	{
	public:
		ConvertToPartitionInfo(void)
		{
			GPT = SwitchParameter(false);
		};
		~ConvertToPartitionInfo(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			Mandatory = true, HelpMessage = "input data")]
		[ValidateNotNullOrEmpty]
		property JcCmdLet::BinaryType ^ data;

		[Parameter(HelpMessage = "is including GPT")]
		property SwitchParameter GPT;


	public:
		virtual void InternalProcessRecord() override
		{
			BYTE* buf = data->LockData();
			GptHeader* gg = reinterpret_cast<GptHeader*>(buf);
			GptInfo^ gpt = gcnew GptInfo(*gg);

			buf += SECTOR_SIZE;
			PartitionEntry* pp = reinterpret_cast<PartitionEntry*>(buf);
			for (UINT ii = 0; ii < gpt->entry_num; ++ii, ++pp)
			{
				if (pp->part_id != GUID({ 0 }))
				{
					PartitionInfo^ part = gcnew PartitionInfo(*pp);
					//				WriteObject(part);
					gpt->AddPartition(part);
				}
			}
			data->Unlock();
			WriteObject(gpt);
		}
	};

};


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
		static LbaType^ Create(UINT64 lba) { return LbaType(lba); }

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
		UINT64 GetValue(void) { return m_secs; }
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
			name = gcnew String(p.name);
			attr = p.attribute;

		}
		PartitionInfo(void) {
		type = Clone::PartitionType::Empty_Partition;
		}
	public:
		void ToBinary(PartitionEntry* p)
		{
			memset(p, 0, sizeof(PartitionEntry));
			if (type == Clone::PartitionType::Empty_Partition) 	{	return;	}
			Guid^ type_id = GptTypeToGuid(type);
			SystemGuidToGUID(p->type, type_id);
			SystemGuidToGUID(p->part_id, part_id);
			p->first_lba = first_lba.GetValue();
			p->last_lba = last_lba.GetValue();
			p->attribute = attr;
			std::wstring str_name;
			ToStdString(str_name, name);
			memcpy_s(p->name, str_name.size() * 2, str_name.c_str(), str_name.size() * 2);
//			wcscpy_s(p->name, str_name.c_str());
		}
	public:
		Clone::PartitionType type;
		Guid ^ part_id=nullptr;
		LbaType first_lba;
		LbaType last_lba;
		String^ name=nullptr;
		UINT64 attr=0;
	};

	public ref class GptInfo : public Object
	{
	public:
		GptInfo(const GptHeader& g)
		{
			raw_header = new GptHeader;
			memcpy_s(raw_header, sizeof(GptHeader), &g, sizeof(GptHeader));
			current_lba = LbaType(g.current_lba);
			backup_lba = LbaType(g.backup_lba);
			first_usable = LbaType(g.first_usable);
			last_usable = LbaType(g.last_usable);
			disk_id = GUIDToSystemGuid(g.disk_gid);
			entry_num = g.entry_num;
			partition_list = gcnew Collections::ArrayList;
		}
		~GptInfo(void)
		{
			delete raw_header;
		}
	public:
		void AddPartition(PartitionInfo^ part)
		{
			partition_list->Add(part);
		}
		void ToBinary(jcvos::IBinaryBuffer* & buf)
		{
			size_t buf_size = SECTOR_SIZE + raw_header->entry_size * partition_list->Count;
			jcvos::CreateBinaryBuffer(buf, buf_size);

			BYTE* _buf = buf->Lock();
			PartitionEntry* pp = reinterpret_cast<PartitionEntry*>(_buf + SECTOR_SIZE);
			for (size_t ii = 0; ii < partition_list->Count; ++ii, pp++)
			{
				PartitionInfo^ part = dynamic_cast<PartitionInfo^>(partition_list[ii]);
				part->ToBinary(pp);
			}

			// set header
			raw_header->current_lba = current_lba.GetValue();
			raw_header->backup_lba = backup_lba.GetValue();
			raw_header->first_usable = first_usable.GetValue();
			raw_header->last_usable = last_usable.GetValue();
			raw_header->entry_num = entry_num;
			SystemGuidToGUID(raw_header->disk_gid, disk_id);
			// 计算CRC
	// 计算分区表CRC	  
			size_t entry_buf_size = raw_header->entry_size * partition_list->Count;
			size_t entry_size = entry_num * raw_header->entry_size;
			//size_t dword_size = entry_size / 4;
			size_t dword_size = min(entry_buf_size, entry_size) / 4;
			pp = reinterpret_cast<PartitionEntry*>(_buf + SECTOR_SIZE);
			
			DWORD crc_entry = CalculateCRC32((DWORD*)(pp), dword_size);
			// 计算表头的CRC;
			raw_header->entry_crc = crc_entry;
			raw_header->header_crc = 0;
			raw_header->header_crc = CalculateCRC32((DWORD*)(raw_header), 23);

			memcpy_s(_buf, sizeof(GptHeader), raw_header, sizeof(GptHeader));

			buf->Unlock(_buf);
		}
	public:
		LbaType current_lba;
		LbaType backup_lba;
		LbaType first_usable;
		LbaType last_usable;
		UINT entry_num;
		Guid^ disk_id;

		GptHeader * raw_header;

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
//			size_t data_len = data->Length;
			BYTE* buf = data->LockData();
			BYTE* end_buf = buf + data->Length;
			GptHeader* gg = reinterpret_cast<GptHeader*>(buf);
			GptInfo^ gpt = gcnew GptInfo(*gg);

			buf += SECTOR_SIZE;
			PartitionEntry* pp = reinterpret_cast<PartitionEntry*>(buf);
			for (;(BYTE*)pp < end_buf; ++pp)
			{
				PartitionInfo^ part = nullptr;
				if (pp->part_id != GUID({ 0 })) part = gcnew PartitionInfo(*pp);
				else part = gcnew PartitionInfo;
				gpt -> AddPartition(part);

			}
			data->Unlock();
			WriteObject(gpt);
		}
	};

	[CmdletAttribute(VerbsData::ConvertFrom, "PartitionInfo")]
	public ref class ExportPartitionInfo : public JcCmdLet::JcCmdletBase
	{
	public:
		ExportPartitionInfo(void) { GPT = SwitchParameter(false); };
		~ExportPartitionInfo(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true, ValueFromPipelineByPropertyName = true,
			Mandatory = true, HelpMessage = "input data")]
		[ValidateNotNullOrEmpty]
		property GptInfo^ gpt_info;

		[Parameter(HelpMessage = "is including GPT")]
		property SwitchParameter GPT;


	public:
		virtual void InternalProcessRecord() override
		{
		//	size_t buf_size = SECTOR_SIZE + 128 * gpt_info->partition_list;
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
//			jcvos::CreateBinaryBuffer(buf, buf_size);
			gpt_info->ToBinary(buf);
			JcCmdLet::BinaryType^ out = gcnew JcCmdLet::BinaryType(buf);
			WriteObject(out);
		}
	};

#if 0
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ==== class partition table ====
	public ref class PartitionTable : public Object
	{
	public:
		PartitionTable(void) {};
		PartitionTable(JcCmdLet::BinaryType^ data);

	protected:
		GptHeader* m_header;
		PartitionEntry* m_entries;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ==== for partition entries ====
	[CmdletAttribute(VerbsData::Import, "PartitionTable")]
	public ref class ImportPartitionTable : public JcCmdLet::JcCmdletBase
	{
	public:
		ImportPartitionTable(void) { };
		~ImportPartitionTable(void) {};

	public:
		[Parameter(Position = 0,
			ValueFromPipelineByPropertyName = true, ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify device object")]
		property JcCmdLet::BinaryType input_data;

	public:
		virtual void InternalProcessRecord() override;
	};
#endif
};


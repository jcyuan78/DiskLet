#pragma once

#include "ssd-simulator.h"
#include "trace-statistic.h"

#include <stdint.h>
#include <map>
#include <vector>

#include <jccmdlet-comm.h>

#include <boost/cast.hpp>
#include <boost/functional/hash.hpp>


#pragma make_public(ITraceStatistic)

using namespace System;
using namespace System::Management::Automation;

inline void LbaToUnit(UINT64 lba, UINT64 secs, UINT& unit, UINT& unit_num)
{
	unit = lba >> 3;
	UINT end_lba = lba + secs;
	if (end_lba == 0) THROW_ERROR(ERR_APP, L"invalid logical address, lba=%d, secs=%d", lba, secs);
	UINT end_unit = ((end_lba - 1) >>3) + 1;
	unit_num = end_unit - unit;
}


typedef int32_t FID;

class LBA_INFO
{
public:
	FID fid;
	uint32_t file_offset;
};

class SEGMENT
{
public:
	SEGMENT(uint64_t f, uint32_t s, uint32_t n) : first_cluster(f), start(s), num(n) {};
	SEGMENT(const SEGMENT & ss) : first_cluster(ss.first_cluster), start(ss.start), num(ss.num) {};
public:
	uint64_t first_cluster;		// 这一段的起始的磁盘lba （4KB单位）
	uint32_t start;				// 这一段在文件中的偏移量
	uint32_t num;
};

class FILE_INFO
{
public:
	FID fid;
	std::wstring fn;
	uint32_t length;		// 以4K为单位
//	uint64_t * cluster_mapping;
	std::vector<SEGMENT> segments;
};

typedef std::map<FID, FILE_INFO *> FN_MAP;

class GlobalInit
{
public:
	GlobalInit(void);
	~GlobalInit(void);

public:
	void SetMaping(FN_MAP * fn_map, LBA_INFO * lba_map, size_t map_size, uint64_t offset);
	LBA_INFO * ClusterToFid(uint64_t cluster);
	void FidToFn(std::wstring & fn, FID fid);
	void AddFile(FID fid, FILE_INFO * file);
	FID FindFidByName(const std::wstring & fn);
	uint64_t GetLba(FID fid, uint64_t offset);

	/// <summary> 设置partion的offset和大小	</summary>
	/// <param name="offset"> partition的起始位置，sector单位</param>
	/// <param name="secs"> parttion的大小，sector单位</param>
	void SetDiskInfo(UINT64 offset, UINT64 secs)
	{
		m_disk_offset = offset;
		m_disk_cap = offset + secs;
	}

	UINT64 GetDiskCapacity(void) const { return m_disk_cap; }
//	UINT64 GetPartitionCapacity(void) const { return m_part_cap; }
	UINT64 GetPartitionOffset(void) const { return m_disk_offset; }

	bool CreateStatistic(const std::wstring& name)
	{
		RELEASE(m_statistic);
		m_statistic = static_cast<ITraceStatistic*>(jcvos::CDynamicInstance<CTraceStatistic>::Create());
		return true;
	}

	void GetStatistic(ITraceStatistic*& statistic)
	{
		JCASSERT(statistic == NULL);
		statistic = m_statistic;
		if (statistic) statistic->AddRef();
	}

protected:
	void ClearMapping(void);

public:
	uint64_t m_first_lba;

protected:
	std::map<std::wstring, FID> * m_fid_map;

	FN_MAP * m_fn_mapping;
	LBA_INFO * m_lba_mapping;
	// map的大小，以8 sec (4KB）为单位;
	size_t m_map_size;
	uint64_t m_offset;

	UINT64 m_disk_cap, m_disk_offset;

	ITraceStatistic* m_statistic;
};

extern GlobalInit global;


typedef jcvos::CGlobalSingleToneNet<CAddressRank> CAddressRankInstance;

namespace WLA {
	// -- my cmdlet base class, to handle exceptions	
//	public ref class WLABase : public Cmdlet
//	{
//	public:
//		virtual void InternalProcessRecord() {};
//	protected:
////		void ShowPipeMessage(void);
//
//	public:
//
//	protected:
////		void ShowProgress(IProgress * progress, int id, String ^ activity);
//
//
//	protected:
//		virtual void ProcessRecord() override;
//	};


	[CmdletAttribute(VerbsCommon::Set, "StaticMapping")]
	public ref class SetStaticMapping : public JcCmdLet::JcCmdletBase
	{
	public:
		SetStaticMapping(void);
		~SetStaticMapping(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject ^ mapping;

		[Parameter(Position = 1, Mandatory = true,
			HelpMessage = "disk size, in sector")]
		property uint64_t secs;

		[Parameter(Position = 2, Mandatory = true,
			HelpMessage = "offset of the device")]
		property uint64_t first_lba;

	public:
		virtual void BeginProcessing()	override;
		virtual void EndProcessing()	override;
		virtual void InternalProcessRecord() override;

	protected:
		LBA_INFO * m_lba_mapping;
		// map的大小，以8 sec (4KB）为单位;
		size_t m_map_size;
		uint64_t m_first_cluster;
		FILE_INFO* m_cur_file_info;
	};


	[CmdletAttribute(VerbsCommon::Add, "FileMapping")]
	public ref class AddFileMapping : public JcCmdLet::JcCmdletBase
	{
	public: 
		AddFileMapping(void) { fid = 0; }
		~AddFileMapping(void) {};

	public:
		//[Parameter(Position = 0, ValueFromPipeline = true,
		//	ValueFromPipelineByPropertyName = true, Mandatory = true,
		//	HelpMessage = "input object")]
		//property PSObject ^ file_mapping;
		[Parameter(Position = 2, HelpMessage = "fid")]
		property int32_t fid;

		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property String  ^ fn;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property Collections::ArrayList  ^ segments;

		[Parameter(Position = 2, HelpMessage = "disk offset")]
		property uint64_t first_lba;

//		property Array ^ segments;



	public:
		virtual void InternalProcessRecord() override;


	};


	[CmdletAttribute(VerbsData::Convert, "Trace")]
	public ref class ProcessTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		ProcessTrace(void) {};
		~ProcessTrace(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject ^ input_obj;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsData::Convert, "Event")]
	public ref class MarkEvent : public JcCmdLet::JcCmdletBase
	{
	public:
		MarkEvent(void) {};
		~MarkEvent(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject ^ input_obj;

		[Parameter(Position = 1, HelpMessage = "disk offset")]
		property uint64_t first_lba;

	public:
		virtual void InternalProcessRecord() override;
	};

	[CmdletAttribute(VerbsCommon::Set, "DiskInfo")]
	public ref class SetDiskInfo : public JcCmdLet::JcCmdletBase
	{
	public:
		SetDiskInfo(void) { offset = -1; capacity = -1; };
		~SetDiskInfo(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true,
			HelpMessage = "partition offset")]
		property UINT64 offset;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true,
			HelpMessage = "partition size (sec)")]
		property UINT64 capacity;

	public:
		virtual void InternalProcessRecord() override
		{
			global.SetDiskInfo(offset, capacity);
		}
	};

	[CmdletAttribute(VerbsCommon::Get, "DiskInfo")]
	public ref class GetDiskInfo : public JcCmdLet::JcCmdletBase
	{
	public:
		GetDiskInfo(void) { };
		~GetDiskInfo(void) {};

	public:

	public:
		virtual void InternalProcessRecord() override
		{
			UINT64 capacity = global.GetDiskCapacity();
			PSObject^ obj = gcnew PSObject;
			AddPropertyMember<UINT64>(obj, L"capacity", capacity);
			WriteObject(obj);
		}
	};


	[CmdletAttribute(VerbsData::Initialize, "AddressRank")]
	public ref class InitAddressRank : public JcCmdLet::JcCmdletBase
	{
	public:
		InitAddressRank(void) {};
		~InitAddressRank(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "number of rank")]
		property size_t rank_size;

	public:
		virtual void BeginProcessing()	override {};
		virtual void EndProcessing()	override {};
		virtual void InternalProcessRecord() override
		{
			CAddressRank* rank = CAddressRankInstance::Instance();
			if (rank == nullptr) throw gcnew System::ApplicationException(L"address rank object is null");
			rank->InitialRank(rank_size);
		}
	};




	[CmdletAttribute(VerbsCommon::Get, "AddressRank")]
	public ref class GetAddressRank : public JcCmdLet::JcCmdletBase
	{
	public:
		GetAddressRank(void) {};
		~GetAddressRank(void) {};

	public:
		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "value type")]
		property int value_type;

	public:
		virtual void BeginProcessing()	override {};
		virtual void EndProcessing()	override {};
		virtual void InternalProcessRecord() override
		{
			CAddressRank* rank = CAddressRankInstance::Instance();
			if (rank == nullptr) throw gcnew System::ApplicationException(L"address rank object is null");
			System::String^ value_name;

			size_t size = rank->GetSize();
			for (size_t ii = 1; ii < size; ++ii)
			{
				CAddressRank::AddressPair& add = rank->GetAddressData(value_type, ii);
				PSObject^ obj = gcnew PSObject;
				AddPropertyMember<UINT64>(obj, L"cluster", add.add);
				AddPropertyMember<UINT64>(obj, L"read_count", add.read_count);
				AddPropertyMember<UINT64>(obj, L"write_count", add.write_count);
				AddPropertyMember<UINT64>(obj, L"access_count", add.access_count);
				WriteObject(obj);
			}
		}
	};

	[CmdletAttribute(VerbsCommon::New, "TraceStatistic")]
	public ref class CreateTraceStatistic : public JcCmdLet::JcCmdletBase
	{
	public:
		CreateTraceStatistic(void) {};
		~CreateTraceStatistic(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "statistic name")]
		property String^ name;

	public:
		virtual void InternalProcessRecord() override
		{
			std::wstring str_name;
			ToStdString(str_name, name);
			global.CreateStatistic(str_name);
		}

	protected:

	};


	[CmdletAttribute(VerbsLifecycle::Invoke, "TraceStatistic")]
	public ref class InvokeTraceStatistic : public JcCmdLet::JcCmdletBase
	{
	public:
		InvokeTraceStatistic(void) : m_statistic(nullptr) { spec_lba = (UINT64)(-1); spec_cluster = (UINT)(-1); };
		~InvokeTraceStatistic(void) { if (m_statistic) m_statistic->Release(); };

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject^ trace;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true, ParameterSetName = "by_lba",
			HelpMessage = "specified lba")]
		property UINT64 spec_lba;

		[Parameter(Position = 2, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true, ParameterSetName = "by_cluster",
			HelpMessage = "specified cluster")]
		property UINT spec_cluster;

	public:
		virtual void BeginProcessing()	override
		{
			if (spec_cluster == (UINT)(-1))
			{
				spec_cluster = boost::numeric_cast<UINT>(spec_lba / 8);
			}
			if (m_statistic == nullptr)
			{
				ITraceStatistic* ss = NULL;
				global.GetStatistic(ss);
				if (ss == nullptr) throw gcnew System::ApplicationException(L"the statistic object has not been created.");
				m_statistic = ss;
				m_statistic->Initialize(spec_cluster);
			}
		}
		virtual void EndProcessing()	override {};
		virtual void InternalProcessRecord() override
		{
			if (m_statistic == nullptr) throw gcnew System::ApplicationException(L"the statistic object has not been created.");
			UINT64 lba, secs;
			int cmd = 0;
			
			if (trace == NULL) throw gcnew System::ApplicationException(L"trace is empty");

			String ^ str_cmd = trace->Members[L"cmd"]->Value->ToString();
			if (str_cmd == L"Write") cmd = ITraceStatistic::CMD_WRITE;
			else if (str_cmd == L"Read") cmd = ITraceStatistic::CMD_READ;
			else return;

			lba = System::Convert::ToUInt64(trace->Members[L"lba"]->Value);
			secs = System::Convert::ToUInt64(trace->Members[L"secs"]->Value);
			UINT cluster, cluster_num;
			LbaToUnit(lba, secs, cluster, cluster_num);
			PSObject ^ res = m_statistic->InvokeTrace(cmd, cluster, cluster_num);
			if (res) WriteObject(res);
		}

	protected:
		ITraceStatistic* m_statistic;
	};

	[CmdletAttribute(VerbsData::Import, "Trace")]
	public ref class TraceStatistics : public JcCmdLet::JcCmdletBase
	{
	public:
		TraceStatistics(void);
		~TraceStatistics(void);

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject^ input_obj;

	public:
		virtual void BeginProcessing()	override;
		virtual void EndProcessing()	override;
		virtual void InternalProcessRecord() override;

	protected:

		UINT* m_read_count;
		UINT* m_write_count;
		UINT64 m_cluster_num;
		UINT64 m_cmd_num;
	};

	[CmdletAttribute(VerbsLifecycle::Invoke, "TraceInterval")]
	public ref class TraceInterval : public JcCmdLet::JcCmdletBase
	{
	public:
		TraceInterval(void) 
		{
			m_read_timestamp = NULL; 
			m_write_timestamp = NULL; 
			m_show_first = false;
		}

		~TraceInterval(void) 
		{
			delete [] m_read_timestamp; 
			delete [] m_write_timestamp; 
		}

	public:
		// 输入为command trace，不需要分解成cluster。
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject^ input_obj;

	public:
		virtual void BeginProcessing()	override;
		virtual void EndProcessing()	override
		{
			delete[] m_read_timestamp; m_read_timestamp = NULL;
			delete[] m_write_timestamp; m_write_timestamp = NULL;
		}

		virtual void InternalProcessRecord() override
		{
			m_cmd_id++;
			String^ cmd = input_obj->Members[L"cmd"]->Value->ToString();
			if (cmd != L"Write" && cmd != L"Read") return;

			uint64_t lba = Convert::ToUInt64(input_obj->Members[L"lba"]->Value);
			uint64_t secs = Convert::ToUInt64(input_obj->Members[L"secs"]->Value);

			uint64_t start_cluster = lba / 8;
			uint64_t end_lba = lba + secs;
			uint64_t end_cluster = (end_lba - 1) / 8 + 1;
			if (end_cluster > m_cluster_num)
			{
				String^ str = String::Format(L"start lba ({0}) secs ({1}) is over range (cluster={2})", lba, secs, m_cluster_num);
				throw gcnew	System::ApplicationException(str);
			}

			// 计算与上一个command的间隔
			UINT read_ts = m_read_timestamp[start_cluster];
			UINT write_ts = m_write_timestamp[start_cluster];
			UINT ts = max(read_ts, write_ts);

			// 输出command
			if (m_show_first || ts > 0)
			{
				UINT delta = m_cmd_id - ts;
				PSNoteProperty^ prop = gcnew PSNoteProperty(L"interval", delta);
				input_obj->Members->Add(prop);

				prop = gcnew PSNoteProperty(L"cluster", start_cluster);
				input_obj->Members->Add(prop);

				prop = gcnew PSNoteProperty(L"cluster_num", (end_cluster - start_cluster));
				input_obj->Members->Add(prop);

				WriteObject(input_obj);
			}

			UINT* ts_update = NULL;
			if (cmd == L"Write") ts_update = m_write_timestamp;
			else if (cmd == L"Read") ts_update = m_read_timestamp;
			else JCASSERT(0);

			for (; start_cluster < end_cluster; start_cluster++)
			{
				ts_update[start_cluster]=m_cmd_id;
			}
		}

	protected:
		// 前一个read/write发生的command id
		UINT* m_read_timestamp;
		UINT* m_write_timestamp;
		UINT m_cmd_id;
		UINT64 m_cluster_num;
		bool m_show_first;
	};



	[CmdletAttribute(VerbsLifecycle::Invoke, "TraceIntervalTime")]
	public ref class TraceIntervalTime : public JcCmdLet::JcCmdletBase
	{
	public:
		TraceIntervalTime(void)
		{
			m_read_timestamp = NULL;
			m_write_timestamp = NULL;
			m_show_first = false;
		}

		~TraceIntervalTime(void)
		{
			delete[] m_read_timestamp;
			delete[] m_write_timestamp;
		}

	public:
		// 输入为command trace，不需要分解成cluster。
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input object")]
		property PSObject^ input_obj;

		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "ignore the first command")]
		property SwitchParameter ignore_first;

	public:
		virtual void BeginProcessing()	override;
		virtual void EndProcessing()	override
		{
			delete[] m_read_timestamp; m_read_timestamp = NULL;
			delete[] m_write_timestamp; m_write_timestamp = NULL;
		}

		virtual void InternalProcessRecord() override
		{
			//m_cmd_id++;
			String^ cmd = input_obj->Members[L"cmd"]->Value->ToString();
			if (cmd != L"Write" && cmd != L"Read") return;

			uint64_t lba = Convert::ToUInt64(input_obj->Members[L"lba"]->Value);
			uint64_t secs = Convert::ToUInt64(input_obj->Members[L"secs"]->Value);
			double cur_ts = Convert::ToDouble(input_obj->Members[L"time"]->Value);

			uint64_t start_cluster = lba / 8;
			uint64_t end_lba = lba + secs;
			uint64_t end_cluster = (end_lba - 1) / 8 + 1;
			if (end_cluster > m_cluster_num)
			{
				String^ str = String::Format(L"start lba ({0}) secs ({1}) is over range (cluster={2})", lba, secs, m_cluster_num);
				throw gcnew	System::ApplicationException(str);
			}

			// 计算与上一个command的间隔
			double read_ts = m_read_timestamp[start_cluster];
			double write_ts = m_write_timestamp[start_cluster];
			double ts = max(read_ts, write_ts);

			// 输出command
			if (m_show_first || ts > 0)
			{
				double delta = cur_ts - ts;
				double rate = 1 / delta;
				PSNoteProperty^ prop = gcnew PSNoteProperty(L"interval", rate);
				input_obj->Members->Add(prop);
				prop = gcnew PSNoteProperty(L"cluster", start_cluster);
				input_obj->Members->Add(prop);

				prop = gcnew PSNoteProperty(L"cluster_num", (end_cluster - start_cluster));
				input_obj->Members->Add(prop);

				WriteObject(input_obj);
			}

			double* ts_update = NULL;
			if (cmd == L"Write") ts_update = m_write_timestamp;
			else if (cmd == L"Read") ts_update = m_read_timestamp;
			else JCASSERT(0);

			for (; start_cluster < end_cluster; start_cluster++)
			{
				ts_update[start_cluster] = cur_ts;
			}
		}

	protected:
		// 前一个read/write发生的command id
		double* m_read_timestamp;
		double* m_write_timestamp;
		//UINT m_cmd_id;
		UINT64 m_cluster_num;
		bool m_show_first;
	};


///////////////////////////////////////////////////////////////////////////////
// -- Simulator
	public ref class SSDrive : public Object
	{
	public:
		SSDrive(CSSDSimulator* ssd) : m_ssd(ssd) { JCASSERT(m_ssd); }
		~SSDrive(void) { delete m_ssd; m_ssd = nullptr; }
		!SSDrive(void) { delete m_ssd; m_ssd = nullptr; }

	public:
		CSSDSimulator* GetSimulator(void) { return m_ssd; }
		void EnableHotIndex(int enable)
		{
			m_ssd->EnableHotIndex(enable);
		}


	public:
		property float logical_saturation {float get(void) { return m_ssd->GetLogicalSaturation(); }}
		property UINT data_block {UINT get(void) { return m_ssd->GetDataBlockNum(); }}
		property UINT slc_block {UINT get(void) { return m_ssd->GetSLCBlockNum(); }}
		property UINT empty_block {UINT get(void) { return m_ssd->GetEmptyBlockNum(); }}
		property UINT64 slc_read {UINT64 get(void) { return m_ssd->m_SLC_read; }}
		property UINT64 tlc_read {UINT64 get(void) { return m_ssd->m_TLC_read; }}
		property UINT64 nomapping_read {UINT64 get(void) { return m_ssd->m_unmapping_read; }}
		property UINT64 host_write {UINT64 get(void) { return m_ssd->m_host_write; }}
		property UINT64 nand_write {UINT64 get(void) { return m_ssd->m_nand_write; }}

	protected:
		CSSDSimulator* m_ssd;
	};

	[CmdletAttribute(VerbsCommon::New, "Drive")]
	public ref class CreateSSDrive : public JcCmdLet::JcCmdletBase
	{
	public:
		CreateSSDrive(void) {}
		~CreateSSDrive(void) {}
		!CreateSSDrive(void) {}


	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "configuration file name, json")]
		property String^ name;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "configuration file name, json")]
		property String^ config;


	public:
		virtual void InternalProcessRecord() override
		{
			std::wstring str_fn;
			ToStdString(str_fn, config);

			CSSDSimulator* ssd = nullptr;
			if (name == L"greedy") ssd = new CSSDSimulator;
			else if (name == L"hot_index") ssd = static_cast<CSSDSimulator*>(new CSSDSimulator_2);
			ssd->Create(str_fn);

			SSDrive^ drive = gcnew SSDrive(ssd);
			WriteObject(drive);
		}
	};

	[CmdletAttribute(VerbsLifecycle::Start, "Simulate")]
	public ref class StartSimulation : public JcCmdLet::JcCmdletBase
	{
	public:
		StartSimulation(void) { drive = nullptr; };
		~StartSimulation(void) {}
		!StartSimulation(void) {}


	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "simulator")]
		property SSDrive^ drive;


	public:
		virtual void InternalProcessRecord() override
		{
			if (drive == nullptr) throw gcnew System::ApplicationException(L"drive cannot be null");
			CSSDSimulator* _drive=nullptr;
			_drive = drive->GetSimulator();		JCASSERT(_drive);
			_drive->Initialize(0);
		}
	};


	[CmdletAttribute(VerbsLifecycle::Invoke, "Simulate")]
	public ref class InvokeSimulation : public JcCmdLet::JcCmdletBase
	{
	public:
		InvokeSimulation(void) { drive = nullptr; };
		~InvokeSimulation(void) {}
		!InvokeSimulation(void) {}


	public:
		[Parameter(Position = 0, ValueFromPipeline = false,
			ValueFromPipelineByPropertyName = false, Mandatory = true,
			HelpMessage = "simulator")]
		property SSDrive^ drive;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			ParameterSetName="by_trace",
			HelpMessage = "trace data")]
		property PSObject^ trace;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			ParameterSetName = "by_value",
			HelpMessage = "trace data: cmd name")]
		property String^ cmd;

		[Parameter(Position = 2, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			ParameterSetName = "by_value",
			HelpMessage = "trace data: lba")]
		property UINT lba;

		[Parameter(Position = 3, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			ParameterSetName = "by_value",
			HelpMessage = "trace data: secs")]
		property UINT secs;

	public:
		virtual void InternalProcessRecord() override
		{
			if (drive == nullptr) throw gcnew System::ApplicationException(L"drive cannot be null");
			CSSDSimulator* _drive = nullptr;
			_drive = drive->GetSimulator();		JCASSERT(_drive);

			if (trace)
			{
				cmd = trace->Members[L"cmd"]->Value->ToString();
				{
					lba = System::Convert::ToInt64(trace->Members[L"lba"]->Value);
					if (lba < 0) return;
					secs = System::Convert::ToInt64(trace->Members[L"secs"]->Value);
				}
			}
			if (cmd == L"Write")	_drive->WriteSecs(lba, secs);
			else if (cmd == L"Read")   _drive->ReadSecs(lba, secs);
		}
	};





}

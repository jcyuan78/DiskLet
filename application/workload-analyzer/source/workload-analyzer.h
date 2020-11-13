#pragma once

#include <stdint.h>
#include <map>
#include <vector>

using namespace System;
using namespace System::Management::Automation;

typedef int32_t FID;
//typedef std::map<FID, std::wstring> FN_MAP;

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

};

namespace WLA {
	// -- my cmdlet base class, to handle exceptions	
	public ref class WLABase : public Cmdlet
	{
	public:
		virtual void InternalProcessRecord() {};
	protected:
//		void ShowPipeMessage(void);

	public:

	protected:
//		void ShowProgress(IProgress * progress, int id, String ^ activity);


	protected:
		virtual void ProcessRecord() override;
	};


	[CmdletAttribute(VerbsCommon::Set, "StaticMapping")]
	public ref class SetStaticMapping : public WLABase
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
	public ref class AddFileMapping : public WLABase
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
	public ref class ProcessTrace : public WLABase
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
	public ref class MarkEvent : public WLABase
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


}

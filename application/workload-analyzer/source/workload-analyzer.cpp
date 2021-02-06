#include "pch.h"

LOCAL_LOGGER_ENABLE(L"wla", LOGGER_LEVEL_DEBUGINFO);

#include "workload-analyzer.h"
//#include "../include/utility.h"
#include <boost/cast.hpp>
using namespace System::Runtime::InteropServices;

// {DFE09968-2E36-45AD-97C9-AEBB2C10E81D}
const GUID CAddressRank::m_guid =
{ 0xdfe09968, 0x2e36, 0x45ad, { 0x97, 0xc9, 0xae, 0xbb, 0x2c, 0x10, 0xe8, 0x1d } };


GlobalInit global;

GlobalInit::GlobalInit(void) 
	: m_fn_mapping(nullptr), m_lba_mapping(nullptr), m_fid_map(nullptr)
	, m_statistic(nullptr)
{
	// for test
	wchar_t dllpath[MAX_PATH] = { 0 };
#ifdef _DEBUG
	GetCurrentDirectory(MAX_PATH, dllpath);
//	wprintf_s(L"current path=%s\n", dllpath);
	memset(dllpath, 0, sizeof(wchar_t) * MAX_PATH);
#endif
	// 获取DLL路径
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE this_dll = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	::GetModuleFileName(this_dll, dllpath, MAX_PATH);
	// Load log config
	std::wstring fullpath, fn;
	jcvos::ParseFileName(dllpath, fullpath, fn);
#ifdef _DEBUG
//	wprintf_s(L"module path=%s\n", fullpath.c_str());
#endif
	LOGGER_CONFIG(L"wla.cfg", fullpath.c_str());
	LOG_DEBUG(L"log config...");
}

GlobalInit::~GlobalInit(void)
{
	ClearMapping();
	RELEASE(m_statistic);
	CSingleTonEntry::Unregister();
}

void GlobalInit::SetMaping(FN_MAP * fn_map, LBA_INFO * lba_map, size_t map_size, uint64_t offset)
{
	ClearMapping();
	m_fn_mapping = new FN_MAP;
	m_lba_mapping = lba_map;	//JCASSERT(m_lba_mapping);
	m_map_size = map_size;
	m_offset = offset;
	m_fid_map = new std::map<std::wstring, FID>;
}

LBA_INFO * GlobalInit::ClusterToFid(uint64_t cluster)
{
	if (cluster >= m_map_size) return nullptr;
	return m_lba_mapping+ cluster;
}

void GlobalInit::FidToFn(std::wstring & fn, FID fid)
{
	FILE_INFO * file;
	auto it = m_fn_mapping->find(fid);
	if (it != m_fn_mapping->end())
	{
		file = it->second;
		if (file) fn = file->fn;
	}
}

void GlobalInit::AddFile(FID fid, FILE_INFO * file)
{
	m_fn_mapping->insert(std::make_pair(fid, file));
}

FID GlobalInit::FindFidByName(const std::wstring & fn)
{
	JCASSERT(m_fid_map);
	auto it = m_fid_map->find(fn);
	if (it != m_fid_map->end()) return it->second;
	for (auto ii = m_fn_mapping->begin(); ii != m_fn_mapping->end(); ++ii)
	{
		if (ii->second->fn == fn)
		{
			FID fid = ii->second->fid;
			m_fid_map->insert(std::make_pair(fn, fid));
			return fid;
		}
	}
	return -3;
}

uint64_t GlobalInit::GetLba(FID fid, uint64_t offset)
{
	JCASSERT(m_fn_mapping);
	auto it = m_fn_mapping->find(fid);
	if (it == m_fn_mapping->end() || it->second == nullptr) return 0;
	std::vector<SEGMENT> & segs = it->second->segments;
	for (auto ii = segs.begin(); ii != segs.end(); ++ii)
	{
		int64_t delta = offset - ii->start;
		if (delta >= 0 && delta < ii->num) return ii->first_cluster + delta;
		//if (offset >= ii->start && offset < (ii->start + ii->num))
		//	return ii->first_cluster + offset;
	}
	
	return uint64_t();
}

void GlobalInit::ClearMapping(void)
{
	if (m_fn_mapping)
	{
		for (auto it = m_fn_mapping->begin(); it != m_fn_mapping->end(); ++it)
		{
			delete (it->second);
		}
		delete m_fn_mapping;
	}
	delete m_fid_map;
	delete[] m_lba_mapping;
}



//void WLA::WLABase::ProcessRecord()
//{
//	try
//	{
//		InternalProcessRecord();
//	}
//	catch (std::exception & err)
//	{
//		System::String ^ msg = gcnew System::String(err.what());
//		System::Exception ^ exp = gcnew PipelineStoppedException(msg);
//		ErrorRecord ^er = gcnew	ErrorRecord(exp, L"stderr", ErrorCategory::FromStdErr, this);
//		WriteError(er);
//	}
////	ShowPipeMessage();
//}



WLA::SetStaticMapping::SetStaticMapping(void) : /*m_fn_mapping(nullptr), */m_lba_mapping(nullptr)
{
}

void WLA::SetStaticMapping::BeginProcessing()
{
	m_map_size = (secs + first_lba) / 8;
	m_first_cluster = first_lba / 8;
	m_lba_mapping = new LBA_INFO[m_map_size];
	memset(m_lba_mapping, 0xFF, sizeof(LBA_INFO)*m_map_size);
	//m_fn_mapping = new FN_MAP;
	global.SetMaping(nullptr, m_lba_mapping, m_map_size, m_first_cluster);
	global.m_first_lba = first_lba;
	m_cur_file_info = nullptr;
}

void WLA::SetStaticMapping::EndProcessing()
{
}

void WLA::SetStaticMapping::InternalProcessRecord()
{
//	Collections::Generic::IEnumerator<PSNoteProperty> ^ it = mapping->Members->GetEnumerator()
	System::Collections::Generic::IEnumerator<System::Management::Automation::PSMemberInfo^> ^ it
		= mapping->Members->GetEnumerator();
	it->Reset();
	it->MoveNext();
//	mapping->Members[L"start"]->Value;
//	"start", "len", "attr", "fid", "fn", "file_offset"

	uint64_t start_lba=System::Convert::ToUInt64(mapping->Members[L"start"]->Value);
	uint64_t length= System::Convert::ToUInt64(mapping->Members[L"len"]->Value);
	String ^ attr = mapping->Members[L"attr"]->Value->ToString();
	FID fid = System::Convert::ToInt32(mapping->Members[L"fid"]->Value);
	if (fid < 0) fid --;
	uint32_t file_offset = System::Convert::ToInt32(mapping->Members[L"file_offset"]->Value) / 8;
	std::wstring fn;
	ToStdString(fn, mapping->Members[L"fn"]->Value->ToString());

	uint64_t start_cluster = start_lba / 8;
	uint64_t cluster_num = length / 8;
	for (uint32_t ii = 0; ii < cluster_num; ++ii)
	{
		m_lba_mapping[start_cluster + ii].fid = fid;
		if (fid < 0) 		m_lba_mapping[start_cluster + ii].file_offset = 0;
		else				m_lba_mapping[start_cluster + ii].file_offset = file_offset + ii;
	}

	if (file_offset == 0)
	{
		m_cur_file_info = new FILE_INFO;
		m_cur_file_info->fid = fid;
		m_cur_file_info->fn = fn;
		m_cur_file_info->length = 0;
		global.AddFile(fid, m_cur_file_info);
	}
	m_cur_file_info->segments.push_back(SEGMENT(start_cluster, m_cur_file_info->length, boost::numeric_cast<UINT32>(cluster_num) ));
	m_cur_file_info->length += boost::numeric_cast<UINT32>(cluster_num);

}

void WLA::ProcessTrace::InternalProcessRecord()
{
	uint64_t lba = System::Convert::ToUInt64(input_obj->Members[L"lba"]->Value);
	uint64_t cluster = lba / 8;
	LBA_INFO * finfo = global.ClusterToFid(cluster);
	FID fid=-1;
	uint32_t file_offset=0;
	if (finfo)
	{
		fid = finfo->fid;
		// 如果lba不属于任何文件， 则给出相对c:盘的lba
		if (fid < 0) file_offset = boost::numeric_cast<UINT32>(lba - global.m_first_lba);
		// 如果lba属于某个文件，则给出相对文件的offset
		else file_offset = finfo->file_offset*8;
	}

	std::wstring fn;
	global.FidToFn(fn, fid);
	PSNoteProperty ^ fid_item = gcnew PSNoteProperty(L"fid", fid);
	input_obj->Members->Add(fid_item);

	PSNoteProperty ^ offset_item = gcnew PSNoteProperty(L"offset", file_offset);
	input_obj->Members->Add(offset_item);

	PSNoteProperty ^ fn_item = gcnew PSNoteProperty(L"fn", gcnew String(fn.c_str()) );
	input_obj->Members->Add(fn_item);
	WriteObject(input_obj);
}

void WLA::AddFileMapping::InternalProcessRecord()
{
	if (fid == 0) fid = 0x10000000;
/*
	std::wstring fn;
	ToStdString(fn, file_mapping->Members[L"fn"]->Value->ToString());
	uint64_t secs = System::Convert::ToUInt64(file_mapping->Members[L"file_length"]->Value);
	PSObject ^ ss = (PSObject ^)(file_mapping->Members[L"segments"]->Value);
	Type ^ type = ss->GetType();
*/
	std::wstring str_fn;
	ToStdString(str_fn, fn);
	LBA_INFO * lba_mapping = global.ClusterToFid(0);
	uint64_t offset = 0;
	size_t count = segments->Count;
	FILE_INFO * file_info = new FILE_INFO;
	file_info->fn = str_fn;
	file_info->fid = fid;
	file_info->length = 0;

	for (size_t ii = 0; ii < count; ++ii)
	{
		PSObject ^ ss = (PSObject^)segments[ii];
		// 这一段的起始cluster，相对于磁盘
		uint64_t start = System::Convert::ToUInt64(ss->Members[L"start"]->Value) / 8;
		// 这一段拥有的cluster数量
		uint64_t clusters = System::Convert::ToUInt64(ss->Members[L"secs"]->Value) / 8;

		file_info->segments.push_back(SEGMENT(start, file_info->length, boost::numeric_cast<UINT32>(clusters) ));
		//wprintf_s(L"segment: file offset = %d/offset=%lld, cluster num = %lld, start lba=%lld\n", file_info->length, offset, start, clusters);
		file_info->length += boost::numeric_cast<uint32_t>(clusters);

		for (uint32_t jj = 0; jj < clusters; ++jj)
		{
			lba_mapping[start + jj].fid = fid;
			lba_mapping[start + jj].file_offset = boost::numeric_cast<UINT32>(offset);
			offset++;
		}
	}

	global.AddFile(fid, file_info);
}

void WLA::MarkEvent::InternalProcessRecord()
{
	String ^ op = input_obj->Members[L"Operation"]->Value->ToString();
	uint64_t lba = 0;
	FID fid = -10;

	if (op == L"ReadFile" || op == L"WriteFile")
	{
		String ^path = input_obj->Members[L"Path"]->Value->ToString();
		std::wstring str_path;
		ToStdString(str_path, path);
		
		// 文件相对地址，lba单位
		uint64_t offset = Convert::ToUInt64(input_obj->Members[L"offset"]->Value);
		if (str_path == L"C:")
		{
			lba = first_lba + offset;
			fid = -3;
		}
		else
		{
			// lba单位=>4KB单位
			uint64_t cluster_offset = offset / 8;
			std::wstring ss = str_path.substr(2);
			fid = global.FindFidByName(ss);
			// 4KB单位=>lba单位
			if (fid >= 0) lba = global.GetLba(fid, cluster_offset) * 8;
		}
	}
	PSNoteProperty ^ fid_item = gcnew PSNoteProperty(L"fid", fid);
	input_obj->Members->Add(fid_item);

	PSNoteProperty ^ lba_item = gcnew PSNoteProperty(L"lba", lba);
	input_obj->Members->Add(lba_item);
	WriteObject(input_obj);
}

/// ///////////////////////////////////////////////////////////////////////////



WLA::TraceStatistics::TraceStatistics(void)
	: m_read_count(NULL), m_write_count(NULL)
{
	//m_read_count = new std::vector<UINT>;
	//m_write_count = new std::vector<UINT>;
}

WLA::TraceStatistics::~TraceStatistics(void)
{
	delete[] m_read_count;
	delete[] m_write_count;
}

void WLA::TraceStatistics::BeginProcessing()
{
	UINT64 secs = global.GetDiskCapacity();
	m_cluster_num = (secs - 1) / 8 + 1;
	LOG_DEBUG(L"start counting, cluster number = %d", m_cluster_num);

	m_read_count = new UINT[m_cluster_num];
	memset(m_read_count, 0, sizeof(UINT) * m_cluster_num);
	m_write_count = new UINT[m_cluster_num];
	memset(m_write_count, 0, sizeof(UINT) * m_cluster_num);
	m_cmd_num = 0;
}

void WLA::TraceStatistics::EndProcessing()
{
	LOG_DEBUG(L"start saving counting");
	wprintf_s(L"cluster num=%lld, command num=%lld\n", m_cluster_num, m_cmd_num);

	CAddressRank* rank = CAddressRankInstance::Instance();
	if (rank == nullptr) throw gcnew System::ApplicationException(L"address rank object is null");
	//CAddressRank::VALUE_TYPE vtype = rank->GetValueType();

	size_t lines = 0;
	for (UINT cc = 0; cc < m_cluster_num; ++cc)
	{
		if (m_read_count[cc] == 0 && m_write_count[cc] == 0) continue;
		//PSNoteProperty^ cluster = gcnew PSNoteProperty(L"cluster", cc);
		//PSNoteProperty^ read_count = gcnew PSNoteProperty(L"read_count", m_read_count[cc]);
		//PSNoteProperty^ write_count = gcnew PSNoteProperty(L"write_count", m_write_count[cc]);

		PSObject^ obj = gcnew PSObject;
		AddPropertyMember<UINT64>(obj, L"cluster", cc);
		AddPropertyMember<UINT64>(obj, L"read_count", m_read_count[cc]);
		AddPropertyMember<UINT64>(obj, L"write_count", m_write_count[cc]);
//		AddPropertyMember<UINT64>(obj, L"access_count", add.access_count);

		//obj->Members->Add(cluster);
		//obj->Members->Add(read_count);
		//obj->Members->Add(write_count);
		WriteObject(obj);
		lines++;

		//switch (vtype)
		//{
		//case CAddressRank::VALUE_READ_COUNT:	rank->AddAddress(cc, m_read_count[cc]); break;
		//case CAddressRank::VALUE_WRITE_COUNT:	rank->AddAddress(cc, m_write_count[cc]); break;
		//case CAddressRank::VALUE_ACCESS_COUNT:	rank->AddAddress(cc, m_read_count[cc] + m_write_count[cc]); break;
		//}
		rank->AddAddress(cc, m_read_count[cc], m_write_count[cc]);

	}
	LOG_DEBUG(L"completed saving. total lines=%d", lines);

	delete[] m_read_count;
	m_read_count = NULL;
	delete[] m_write_count;
	m_write_count = NULL;
}

void WLA::TraceStatistics::InternalProcessRecord()
{
	String^ cmd = input_obj->Members[L"cmd"]->Value->ToString();
	if (cmd != L"Write" && cmd != L"Read") return;
//	uint64_t cluster = Convert::ToUInt64(input_obj->Members[L"cluster"]->Value);
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

	for (; start_cluster < end_cluster; start_cluster++)
	{
		if (cmd == L"Write")		m_write_count[start_cluster] ++;
		else if (cmd == L"Read")	m_read_count[start_cluster] ++;
		m_cmd_num++;
	}


	//if (cmd == L"Write")
	//{
	//	for (; start_cluster < end_cluster; start_cluster ++)	m_write_count[start_cluster] ++;
	//}
	//	
	//else if (cmd == L"Read")
	//{
	//	for (; start_cluster < end_cluster; start_cluster ++)	m_read_count[start_cluster] ++;
	//}

}

void WLA::SetDiskInfo::InternalProcessRecord()
{
	global.SetDiskInfo(offset, capacity);
}


void WLA::TraceInterval::BeginProcessing()
{
	UINT64 secs = global.GetDiskCapacity();
	m_cluster_num = (secs - 1) / 8 + 1;
	LOG_DEBUG(L"start counting, cluster number = %d", m_cluster_num);

	m_read_timestamp = new UINT[m_cluster_num];
	memset(m_read_timestamp, 0, sizeof(UINT) * m_cluster_num);
	m_write_timestamp = new UINT[m_cluster_num];
	memset(m_write_timestamp, 0, sizeof(UINT) * m_cluster_num);
	m_cmd_id = 0;

//	m_show_first = !(ignore_first.ToBool());
	m_show_first = false;
}


void WLA::TraceIntervalTime::BeginProcessing()
{
	UINT64 secs = global.GetDiskCapacity();
	m_cluster_num = (secs - 1) / 8 + 1;
	LOG_DEBUG(L"start counting, cluster number = %d", m_cluster_num);

	m_read_timestamp = new double[m_cluster_num];
	memset(m_read_timestamp, 0, sizeof(UINT) * m_cluster_num);
	m_write_timestamp = new double[m_cluster_num];
	memset(m_write_timestamp, 0, sizeof(UINT) * m_cluster_num);

//	m_show_first = !(ignore_first.ToBool());
	m_show_first = false;
}
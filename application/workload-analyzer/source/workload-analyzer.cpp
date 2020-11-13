#include "pch.h"

#include "workload-analyzer.h"
#include <boost/cast.hpp>
using namespace System::Runtime::InteropServices;

LOCAL_LOGGER_ENABLE(L"wla", LOGGER_LEVEL_DEBUGINFO);


void ToStdString(std::wstring & dst, System::String ^ src)
{
	if (!System::String::IsNullOrEmpty(src))
	{
		const wchar_t * wstr = (const wchar_t*)(Marshal::StringToHGlobalUni(src)).ToPointer();
		dst = wstr;
		Marshal::FreeHGlobal(IntPtr((void*)wstr));
	}
}

GlobalInit global;
GlobalInit::GlobalInit(void) 
	: m_fn_mapping(nullptr), m_lba_mapping(nullptr), m_fid_map(nullptr)
{
}

GlobalInit::~GlobalInit(void)
{
	ClearMapping();
}

void GlobalInit::SetMaping(FN_MAP * fn_map, LBA_INFO * lba_map, size_t map_size, uint64_t offset)
{
	ClearMapping();
//	m_fn_mapping = fn_map;		//JCASSERT(m_fn_mapping);
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



void WLA::WLABase::ProcessRecord()
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
//	ShowPipeMessage();
}



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
	m_cur_file_info->segments.push_back(SEGMENT(start_cluster, m_cur_file_info->length, cluster_num));
	m_cur_file_info->length += cluster_num;

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
		if (fid < 0) file_offset = lba - global.m_first_lba;
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
//
//template <typename T>
//void AddPropertyMember(PSObject ^ list, String ^ name, T ^ val)
//{
//	PSNoteProperty ^ item = gcnew PSNoteProperty(name, val);
//	list->Members->Add(item);
//}
//
//template <typename T1, typename T2>
//void AddPropertyMember(PSObject ^ list, String ^ name, const T2 & val)
//{
//	PSNoteProperty ^ item = gcnew PSNoteProperty(name, gcnew T1(val));
//	list->Members->Add(item);
//}
//

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

		file_info->segments.push_back(SEGMENT(start, file_info->length, clusters));
		//wprintf_s(L"segment: file offset = %d/offset=%lld, cluster num = %lld, start lba=%lld\n", file_info->length, offset, start, clusters);
		file_info->length += boost::numeric_cast<uint32_t>(clusters);

		for (uint32_t jj = 0; jj < clusters; ++jj)
		{
			lba_mapping[start + jj].fid = fid;
			lba_mapping[start + jj].file_offset = offset;
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

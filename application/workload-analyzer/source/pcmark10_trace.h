#pragma once

#include <jccmdlet-comm.h>

#include "workload-analyzer.h"

namespace PCMARK
{
	///////////////////////////////////////////////////////////////////////////////
	[CmdletAttribute(VerbsCommon::Add, "FileNameHash")]
	public ref class AddFileNameHash : public JcCmdLet::JcCmdletBase
	{
	public:
		AddFileNameHash(void) { m_fn_map = new std::map<int, std::wstring>; };
		~AddFileNameHash(void) { delete m_fn_map; }
		!AddFileNameHash(void) {}

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;

	public:
		virtual void BeginProcessing() override
		{

		}
		virtual void InternalProcessRecord() override
		{
			if (input_item == nullptr) throw gcnew System::ApplicationException(L"input obj cannot be null");
			String^ fn = input_item->Members[L"Name"]->Value->ToString();
			int hash = fn->GetHashCode();
			//			uint64_t lba = Convert::ToUInt64(input_obj->Members[L"lba"]->Value);
						// 计算hash
			std::wstring str_fn;
			ToStdString(str_fn, fn);
			auto it = m_fn_map->find(hash);
			if (it != m_fn_map->end())
			{
				if (it->second != str_fn)
				{	// 冲突
					wprintf_s(L"conflict, hash=%08X, str1=%s, str2=%s,", hash, it->second.c_str(), str_fn.c_str());
				}
			}

			//boost::hash<std::wstring> string_hash;
			//string_hash()
			AddPropertyMember<int>(input_item, L"hash", hash);
			WriteObject(input_item);
		}

	protected:
		std::map<int, std::wstring>* m_fn_map;
	};

	///////////////////////////////////////////////////////////////////////////////
	class TRACE_INFO
	{
	public:
		std::wstring m_start;
		std::wstring m_end;
		size_t m_count;
	};

	public enum class OPERATION_CODE
	{
		// Write, Read
		WR, RD, 
		// Make Dir, CReate file, (C)open Dir, OPen file, (RM) delete file/dir, OverWritten file,
		MD, CR, CD, OP, RM, OW,
		CREATE, OPEN, OVERWRITE_DIR, OVERWRITTEN,
		// CLose file, FLush file
		CL, FL,
	};

	public ref class TraceItem : public PSObject
	{
	};

	// 将TRACE转换为4元组（file name hash, op, offset, secs)
	[CmdletAttribute(VerbsCommon::Select, "FileTrace")]
	public ref class SelectFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		SelectFileTrace(void) {};
		~SelectFileTrace(void) {};
		!SelectFileTrace(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;
		[Parameter(Position = 1, Mandatory = true,
			HelpMessage = "start timestamp")]
		property String^ start;
		[Parameter(Position = 2, Mandatory = true,
			HelpMessage = "end timestamp")]
		property String^ end;

	public:
		virtual void BeginProcessing() override
		{
			m_running = false;
		}
		virtual void InternalProcessRecord() override
		{
			String^ timestamp = input_item->Members[L"Time of Day"]->Value->ToString();

			if (m_running == true)
			{
				WriteObject(input_item);
				if (timestamp == end) m_running = false;
			}
			else
			{
				if (timestamp == start)
				{
					WriteObject(input_item);
					m_running = true;
				}
			}
		};


	protected:
		bool m_running;



	};




	// 将TRACE转换为4元组（file name hash, op, offset, secs)
	[CmdletAttribute(VerbsData::ConvertFrom, "FileTrace")]
	public ref class ConvertFromFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		ConvertFromFileTrace(void) { m_fn_map = new std::map<int, std::wstring>; };
		~ConvertFromFileTrace(void) { delete m_fn_map; }
		!ConvertFromFileTrace(void) {}

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;

	public:
		virtual void BeginProcessing() override
		{
			m_reg_rd = gcnew System::Text::RegularExpressions::Regex(L"Offset: ([\\d,]+), Length: ([\\d,]+)");
			m_reg_create = gcnew System::Text::RegularExpressions::Regex(L"(Non-Directory File,| Directory,| Delete,).* OpenResult: (.+)");
			m_opened_file = 0;
			m_trace_list = new std::vector<TRACE_INFO>;
			m_trace_start = nullptr;
			m_pre_op = OPERATION_CODE::WR;
			m_create_count = 0;
			m_flash_count = 0;
			m_trace_count = 0;
			m_trace_started = false;
		}
		virtual void InternalProcessRecord() override
		{		// 处理trace:	（1）删除不相关的列，（2）仅保留相关文件的操作，（3）文件名转换为hash
			if (input_item == nullptr) throw gcnew System::ApplicationException(L"input obj cannot be null");
			String^ fn = input_item->Members[L"Path"]->Value->ToString();
			if (!fn->Contains(L"C:\\PCMark_10_Storage_2021-5-12")) return;

			String^ detail = input_item->Members[L"Detail"]->Value->ToString();
			UINT offset = 0, secs = 0;
			String^ result = input_item->Members[L"Result"]->Value->ToString();

			UINT hash = (UINT)fn->GetHashCode();
			String^ timestamp = input_item->Members[L"Time of Day"]->Value->ToString();
			String^ op = input_item->Members[L"Operation"]->Value->ToString();
//			String^ op_code;
			OPERATION_CODE op_code;
			if (op == L"WriteFile")
			{
				ParseReadWrite(detail, offset, secs);
				op_code = OPERATION_CODE::WR;
			}
			else if (op == L"ReadFile")
			{
				ParseReadWrite(detail, offset, secs);
				op_code = OPERATION_CODE::RD;
			}
			else if (op == L"CreateFile")
			{
				if (result == "SUCCESS")
				{
					auto match = m_reg_create->Match(detail);
					if (match->Success)
					{
						String^ disc = match->Groups[2]->Value;
						String^ dir = match->Groups[1]->Value;
						if (disc == L"Created")
						{
							if (dir == L" Directory,")					op_code = OPERATION_CODE::MD;	// create dir
							else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::CR;	// create filea
							else op_code = OPERATION_CODE::CREATE;
						}
						else if (disc == L"Opened")
						{
							if (dir == L" Directory,")					op_code = OPERATION_CODE::CD;	// open dir
							else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OP;	// open file
							else if (dir == L" Delete,")				op_code = OPERATION_CODE::RM;
							else op_code = OPERATION_CODE::OPEN;
						}
						else if (disc == L"Overwritten")
						{
							if (dir == L" Directory,")					op_code = OPERATION_CODE::OVERWRITE_DIR;	// open dir
							else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OW;	// overwritten file
							else op_code = OPERATION_CODE::OVERWRITTEN;
						}
						else	WriteDebug(String::Format(L"Unknonw result={0}, dir={1}, timestamp={2}", disc, dir, timestamp));
					}
					else		WriteDebug(String::Format(L"Unmatched create detail={0}, timestamp={1}", detail, timestamp));
					m_opened_file++;
				}
			}
			else if (op == L"CloseFile")
			{
				op_code = OPERATION_CODE::CL;
				m_opened_file--;
			}
			else if (op == L"FlushBuffersFile") op_code = OPERATION_CODE::FL;
			else {
				std::wstring str, strtime;
				ToStdString(str, op);
				ToStdString(strtime, timestamp);
//				wprintf_s(L"unknown operatrion %s, time=%s\n", str.c_str(), strtime.c_str());
				return;
			}

			// 查找trace的开头
			if (op_code == OPERATION_CODE::CR || op_code == OPERATION_CODE::OP)
			{
				if (m_pre_op != OPERATION_CODE::CR && m_pre_op != OPERATION_CODE::OP)
				{	// 第一次create
					m_create_count = 0;
					if (m_trace_started == false)
					{
						m_trace_start = timestamp;
						m_trace_count = 0;
					}
				}
				else m_create_count++;
			}
			else if (op_code == OPERATION_CODE::FL)
			{
				if (m_pre_op != OPERATION_CODE::FL)	m_flash_count = 0;
				else
				{
					m_flash_count++;
					// find trace start
					if (m_flash_count == m_create_count && m_flash_count > 5)	m_trace_started = true;
				}
			}
			else if (op_code == OPERATION_CODE::CL)
			{
				if (m_opened_file == 0 && m_trace_started)
				{	// end of trace
					TRACE_INFO trace;
					ToStdString(trace.m_start, m_trace_start);
					ToStdString(trace.m_end, timestamp);
					trace.m_count = m_trace_count;
					m_trace_list->push_back(trace);
					m_trace_count = 0;
					m_trace_start = nullptr;
					m_trace_started = false;
				}
			}
			else
			{
				m_create_count = 0;
				m_flash_count = 0;
			}
			m_pre_op = op_code;

			if (m_trace_start != nullptr) m_trace_count++;
			
#if 0
			std::wstring str_fn;
			ToStdString(str_fn, fn);
			auto it = m_fn_map->find(hash);
			if (it != m_fn_map->end())
			{
				if (it->second != str_fn)
				{	// 冲突
					wprintf_s(L"conflict, hash=%08X, str1=%s, str2=%s\n", hash, it->second.c_str(), str_fn.c_str());
				}
			}
#endif

			PSObject^ out_item = gcnew PSObject;
			wchar_t str_hash[10];
			swprintf_s(str_hash, L"%08X", hash);

			AddPropertyMember(out_item, L"timestamp", timestamp);
			//			AddPropertyMember<UINT>(out_item, L"path_hash", hash);
			AddPropertyMember<String>(out_item, L"path_hash", str_hash);
			AddPropertyMember<OPERATION_CODE>(out_item, L"op_code", op_code);
			AddPropertyMember<UINT>(out_item, L"offset", offset);
			AddPropertyMember<UINT>(out_item, L"secs", secs);
			AddPropertyMember(out_item, L"result", result);
			AddPropertyMember<int>(out_item, L"opened", m_opened_file);
			// for debug
			AddPropertyMember<UINT>(out_item, L"create_count", m_create_count);
			AddPropertyMember<UINT>(out_item, L"flash_coount", m_flash_count);
			AddPropertyMember<size_t>(out_item, L"trace_count", m_trace_count);
			
			WriteObject(out_item);
		}

		virtual void EndProcessing() override
		{
			wprintf_s(L"TRACE SPLIT\n");
			wprintf_s(L"start, end, count\n");
			for (auto it = m_trace_list->begin(); it != m_trace_list->end(); ++it)
			{
				TRACE_INFO & trace = *it;
				wprintf_s(L"%s, %s, %lld\n", trace.m_start.c_str(), trace.m_end.c_str(), trace.m_count);
			}
			delete m_trace_list;
		}

	protected:
		bool ParseReadWrite(String^ detail, UINT& offset, UINT& secs)
		{
			System::Text::RegularExpressions::Match^ match = m_reg_rd->Match(detail);

			if (match->Success)
			{
				auto group = match->Groups[1];
				String^ val = group->Value->ToString()->Replace(",", "");
				offset = Convert::ToUInt32(val) / 512;
				secs = Convert::ToUInt32(match->Groups[2]->Value->Replace(",", "") )/ 512;
			}
			return match->Success;
		}

	protected:
		UINT m_create_count;
		UINT m_flash_count;
		String^ m_trace_start;
//		String^ m_trace_end;
		size_t m_trace_count;
		OPERATION_CODE m_pre_op;
		bool m_trace_started;

		std::vector<TRACE_INFO> * m_trace_list;

		int m_opened_file;
		std::map<int, std::wstring>* m_fn_map;
		System::Text::RegularExpressions::Regex^ m_reg_rd;
		System::Text::RegularExpressions::Regex^ m_reg_create;
	};

	class _TRACE_ITEM
	{
	public:
		UINT m_path_hash;
		UINT m_offset;
		UINT m_secs;
		BYTE m_op_code;
		std::wstring m_timestamp;
	};

	public ref class _TraceData : public Object
	{
	public:
		_TraceData(void) { m_trace = new std::vector<_TRACE_ITEM>; }
		~_TraceData(void) { delete m_trace; }
	public:
		std::vector<_TRACE_ITEM> * m_trace;
	};


	///////////////////////////////////////////////////////////////////////////////
	[CmdletAttribute(VerbsData::Import, "FileTrace")]
	public ref class ImportFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		ImportFileTrace(void) { m_trace = nullptr; };
		~ImportFileTrace(void) { delete m_trace; m_trace = nullptr; }
		!ImportFileTrace(void) { delete m_trace; m_trace = nullptr; }

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;

	public:
		virtual void BeginProcessing() override
		{
			m_trace = gcnew _TraceData;
		}
		virtual void InternalProcessRecord() override
		{
			JCASSERT(m_trace);
			_TRACE_ITEM trace;
			String^ timestamp = input_item->Members[L"timestamp"]->Value->ToString();
			ToStdString(trace.m_timestamp, timestamp);
//			String^ str_hash = input_item->Members[L"path_hash"]->Value->ToString();
//			trace.m_path_hash = Convert::ToUInt32(L"0x" + str_hash);
			String^ op = input_item->Members[L"op_code"]->Value->ToString();
			if (op == L"CR")		trace.m_op_code = 1, m_createfile++;
			else if (op == L"CL")	trace.m_op_code = 2, m_closefile++;
			else if (op == L"WR")	trace.m_op_code = 3, m_writes++;
			else if (op == L"RD")	trace.m_op_code = 4, m_reads++;
			else if (op == L"FL")	trace.m_op_code = 5, m_flush++;
			trace.m_offset = Convert::ToUInt32(input_item->Members[L"offset"]->Value);
			trace.m_secs = Convert::ToUInt32(input_item->Members[L"secs"]->Value);
			//m_trace->m_trace->push_back(trace);

			AddPropertyMember<UINT>(input_item, L"reads", m_reads);
			AddPropertyMember<UINT>(input_item, L"writes", m_writes);
			AddPropertyMember<UINT>(input_item, L"createfiles", m_createfile);
			AddPropertyMember<UINT>(input_item, L"closefiles", m_closefile);
			AddPropertyMember<UINT>(input_item, L"flushes", m_flush);
			WriteObject(input_item);
		}

		virtual void EndProcessing() override
		{
			//			WriteObject(m_trace);
		}

	protected:
		_TraceData^ m_trace;
		UINT m_reads, m_writes, m_createfile, m_closefile, m_flush;

	};

	[CmdletAttribute(VerbsDiagnostic::Measure, "FileTrace")]
	public ref class MeasureFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		MeasureFileTrace(void) {/* m_trace = nullptr; */ };
		~MeasureFileTrace(void) {/* delete m_trace; m_trace = nullptr;*/ }
		!MeasureFileTrace(void) { /*delete m_trace; m_trace = nullptr;*/ }

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property _TraceData^ traces;

	public:
		virtual void BeginProcessing() override
		{
			//			m_trace = gcnew TraceData;
		}
		virtual void InternalProcessRecord() override
		{
			UINT reads = 0, writes = 0, createfile = 0, closefile = 0, flush = 0;

			std::vector<_TRACE_ITEM>* tt = traces->m_trace;
			for (auto it = tt->begin(); it != tt->end(); ++it)
			{
				PSObject^ out_item = gcnew PSObject;
				wchar_t str_hash[10];

				AddPropertyMember<String>(out_item, L"timestamp", it->m_timestamp.c_str());

				swprintf_s(str_hash, L"%08X", it->m_path_hash);
				AddPropertyMember<String>(out_item, L"path_hash", str_hash);

				String^ str_op;
				switch (it->m_op_code)
				{
				case 1:	str_op = "CR"; createfile++;	break;
				case 2: str_op = "CL"; closefile++;		break;
				case 3: str_op = "WR"; writes++;		break;
				case 4: str_op = "RD"; reads++;			break;
				case 5: str_op = "FL"; flush++;			break;
				}
				AddPropertyMember(out_item, L"op_code", str_op);
				AddPropertyMember<int>(out_item, L"offset", it->m_offset);
				AddPropertyMember<int>(out_item, L"secs", it->m_secs);

				AddPropertyMember<UINT>(out_item, L"reads", reads);
				AddPropertyMember<UINT>(out_item, L"writes", writes);
				AddPropertyMember<UINT>(out_item, L"createfiles", createfile);
				AddPropertyMember<UINT>(out_item, L"closefiles", closefile);
				AddPropertyMember<UINT>(out_item, L"flushes", flush);
				WriteObject(out_item);

				if (reads > 200000 && writes > 15000 && createfile > 5000 && closefile > 3000 && flush > 9) break;
			}
		}

		virtual void EndProcessing() override
		{
			//			WriteObject(m_trace);
		}

	protected:
		//		TraceData^ m_trace;
	};


}

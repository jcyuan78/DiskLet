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
		CREATE, OPEN, OVERWRITE_DIR, OVERWRITTEN, CC,
		// CLose file, FLush file
		CL, FL,
		FIL, DIR, NDF
	};

	public ref class TraceItem : public Object
	{
	public:
		property String^ timestamp;
		property double ts;
		property String^ path_hash;
		property OPERATION_CODE op_code;
		property OPERATION_CODE file;
		property UINT offset;
		property UINT secs;
		property int test_file;	// 用0和1表示便于对齐
		property int is_dir;// 用0和1表示便于对齐
		property int  opened;
		property String^ path;
//		String^ result;
	};

	// 将TRACE转换为4元组（file name hash, op, offset, secs)
	//  利用Split-FileTrace的结果，将trace分成不同的片段。
	[CmdletAttribute(VerbsCommon::Select, "FileTrace")]
	public ref class SelectFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		SelectFileTrace(void) { start = nullptr; end = nullptr; };
		~SelectFileTrace(void) {};
		!SelectFileTrace(void) {};

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;
		[Parameter(Position = 1, 
			HelpMessage = "start timestamp")]
		property String^ start;
		[Parameter(Position = 2, 
			HelpMessage = "end timestamp")]
		property String^ end;

	public:
		virtual void BeginProcessing() override
		{
			if (start == nullptr) m_running = true;
			else m_running = false;
		}
		virtual void InternalProcessRecord() override
		{
			String^ timestamp = input_item->Members[L"Time of Day"]->Value->ToString();

			if (m_running == true)
			{
				WriteObject(input_item);
				if (end && timestamp == end) m_running = false;
			}
			else
			{
				if (start && timestamp == start)
				{
					WriteObject(input_item);
					m_running = true;
				}
			}
		};


	protected:
		bool m_running;
	};

/*
	[CmdletAttribute(VerbsDiagnostic::Measure, "FileTrace")]
	public ref class MeasureFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		MeasureFileTrace(void) { };
		~MeasureFileTrace(void) {  }
		!MeasureFileTrace(void) {}
	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property TraceItem^ input_item;
		virtual void BeginProcessing() override
		{
		}
		virtual void InternalProcessRecord() override
		{		// 处理trace:	（1）删除不相关的列，（2）仅保留相关文件的操作，（3）文件名转换为hash
		}
	protected:
	};
*/

	// 将TRACE转换为4元组（file name hash, op, offset, secs)
	//  查找FileTrace的切分点。将ProcessMonitor记录的FileTrace分段。分割成以PCMark10的trace为单位的段落。输出分割的起始和结束timestamp
	[CmdletAttribute(VerbsCommon::Split, "FileTrace")]
	public ref class SplitFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		SplitFileTrace(void) { };
		~SplitFileTrace(void) {  }
		!SplitFileTrace(void) {}

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property TraceItem^ input_item;

	public:
		virtual void BeginProcessing() override
		{
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

			OPERATION_CODE op_code = input_item->op_code;
			if (op_code == OPERATION_CODE::MD || op_code == OPERATION_CODE::CR || op_code == OPERATION_CODE::CD
				|| op_code == OPERATION_CODE::OP || op_code == OPERATION_CODE::RM || op_code == OPERATION_CODE::OW
				|| op_code == OPERATION_CODE::CC)
			{
				if (m_trace_started == false && m_opened_file == 0)
				{
					m_trace_start = input_item->timestamp;
					m_trace_count = 0;
				}
				m_opened_file++;
				if (m_opened_file > 20)	m_trace_started = true;
			}
			else if (op_code == OPERATION_CODE::CL)	m_opened_file--;

			// 查找trace的开头
			if (false) {}
			else if (op_code == OPERATION_CODE::CL)
			{
				if (m_opened_file == 0 && m_trace_started)
				{	// end of trace
					TRACE_INFO trace;
					ToStdString(trace.m_start, m_trace_start);
					ToStdString(trace.m_end, input_item->timestamp);
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
//			WriteProgress();

#if 1

			PSObject^ out_item = gcnew PSObject;
			AddPropertyMember(out_item, L"timestamp", input_item->timestamp);
			AddPropertyMember(out_item, L"path_hash", input_item->path_hash);
			AddPropertyMember<OPERATION_CODE>(out_item, L"op_code", input_item->op_code);
			AddPropertyMember<UINT>(out_item, L"offset", input_item->offset);
			AddPropertyMember<UINT>(out_item, L"secs", input_item->secs);
//			AddPropertyMember(out_item, L"result", result);
			AddPropertyMember<int>(out_item, L"opened", m_opened_file);
			// for debug
			AddPropertyMember<UINT>(out_item, L"create_count", m_create_count);
			AddPropertyMember<UINT>(out_item, L"flash_coount", m_flash_count);
			AddPropertyMember<size_t>(out_item, L"trace_count", m_trace_count);

			WriteObject(out_item);

#endif
		}

		virtual void EndProcessing() override
		{
			wprintf_s(L"TRACE SPLIT\n");
			wprintf_s(L"start, end, count\n");
			for (auto it = m_trace_list->begin(); it != m_trace_list->end(); ++it)
			{
				TRACE_INFO& trace = *it;
//				wprintf_s(L"%s, %s, %lld\n", trace.m_start.c_str(), trace.m_end.c_str(), trace.m_count);
				PSObject^ out_item = gcnew PSObject;
				AddPropertyMember<String>(out_item, L"start", trace.m_start.c_str());
				AddPropertyMember<String>(out_item, L"end", trace.m_end.c_str());
				AddPropertyMember<UINT64>(out_item, L"count", trace.m_count);
//				WriteObject(out_item);
			}
			delete m_trace_list;
		}

	protected:
		//bool ParseReadWrite(String^ detail, UINT& offset, UINT& secs)
		//{
		//	System::Text::RegularExpressions::Match^ match = m_reg_rd->Match(detail);

		//	if (match->Success)
		//	{
		//		auto group = match->Groups[1];
		//		String^ val = group->Value->ToString()->Replace(",", "");
		//		offset = Convert::ToUInt32(val) / 512;
		//		secs = Convert::ToUInt32(match->Groups[2]->Value->Replace(",", "")) / 512;
		//	}
		//	return match->Success;
		//}

	protected:
		UINT m_create_count;
		UINT m_flash_count;
		String^ m_trace_start;
		//		String^ m_trace_end;
		size_t m_trace_count;
		OPERATION_CODE m_pre_op;
		bool m_trace_started;

		std::vector<TRACE_INFO>* m_trace_list;

		int m_opened_file;
		//std::map<int, std::wstring>* m_fn_map;
		//System::Text::RegularExpressions::Regex^ m_reg_rd;
		//System::Text::RegularExpressions::Regex^ m_reg_create;
	};


	public ref class ParseHelper : public Object
	{
	public:
		ParseHelper(void)
		{
			m_reg_rd = gcnew System::Text::RegularExpressions::Regex(L"Offset: ([\\d,]+), Length: ([\\d,]+)");
			m_reg_create = gcnew System::Text::RegularExpressions::Regex(L"(Non-Directory File,| Directory,| Delete,).* OpenResult: (.+)");
			m_reg_timestamp = gcnew System::Text::RegularExpressions::Regex(L"([\\d]+):([\\d]+):([\\d]+)\\.([\\d]*)");
		};

	public:
		bool ParseReadWrite(String^ detail, UINT& offset, UINT& secs)
		{
			System::Text::RegularExpressions::Match^ match = m_reg_rd->Match(detail);

			if (match->Success)
			{
				auto group = match->Groups[1];
				String^ val = group->Value->ToString()->Replace(",", "");
				offset = Convert::ToUInt32(val) / 512;
				secs = Convert::ToUInt32(match->Groups[2]->Value->Replace(",", "")) / 512;
			}
			return match->Success;
		}

		OPERATION_CODE ParseCreate(String^ detail, bool& is_dir)
		{
			is_dir = false;
			OPERATION_CODE op_code = OPERATION_CODE::CC;
			auto match = m_reg_create->Match(detail);
			if (match->Success)
			{
				String^ disc = match->Groups[2]->Value;
				String^ dir = match->Groups[1]->Value;
				if (disc == L"Created")
				{
					if (dir == L" Directory,")					op_code = OPERATION_CODE::MD, is_dir = true;	// create dir
					else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::CR, is_dir = false;	// create filea
					else op_code = OPERATION_CODE::CREATE;
				}
				else if (disc == L"Opened")
				{
					if (dir == L" Directory,")					op_code = OPERATION_CODE::CD, is_dir = true;	// open dir
					else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OP, is_dir = false;	// open file
					else if (dir == L" Delete,")				op_code = OPERATION_CODE::RM;
					else op_code = OPERATION_CODE::OPEN;
				}
				else if (disc == L"Overwritten")
				{
					if (dir == L" Directory,")					op_code = OPERATION_CODE::OVERWRITE_DIR, is_dir = true;	// open dir
					else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OW;	// overwritten file
					else op_code = OPERATION_CODE::OVERWRITTEN;
				}
				//					else	WriteDebug(String::Format(L"Unknonw result={0}, dir={1}, timestamp={2}", disc, dir, timestamp));
			}
			//				else	WriteDebug(String::Format(L"Unmatched create detail={0}, timestamp={1}", detail, timestamp));
			return op_code;
		}

		double ParseTimestamp(String^ str_timestamp)
		{
			double ts = 0;
			auto match = m_reg_timestamp->Match(str_timestamp);
			if (match->Success)
			{
				int hh = Convert::ToInt32(match->Groups[1]->Value);
				int mm = Convert::ToInt32(match->Groups[2]->Value);
				int ss = Convert::ToInt32(match->Groups[3]->Value);
				String^ str_ms = match->Groups[4]->Value;
				double ms = Convert::ToDouble(L"0." + str_ms);
				//				str_ms->Length;
				ts = hh * 3600 + mm * 60 + ss + ms;
			}
			return ts;
		}
	protected:
		System::Text::RegularExpressions::Regex^ m_reg_rd;
		System::Text::RegularExpressions::Regex^ m_reg_create;
		System::Text::RegularExpressions::Regex^ m_reg_timestamp;
	};

	[CmdletAttribute(VerbsData::Convert, "FileTrace")]
	public ref class ConvertFileTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		ConvertFileTrace(void) { /*m_fn_map = new std::map<int, std::wstring>; */};
		~ConvertFileTrace(void) { /*delete m_fn_map;*/ }
		!ConvertFileTrace(void) {}

	public:
		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true, Mandatory = true,
			HelpMessage = "input item")]
		property PSObject^ input_item;

		//[Parameter(Position = 1, Mandatory = true,
		//	HelpMessage = "filter of folder")]
		//property String^ folder_name;

	public:
		virtual void BeginProcessing() override
		{
			m_parser = gcnew ParseHelper;
			m_opened_file = 0;

		}

		virtual void InternalProcessRecord() override
		{
			if (input_item == nullptr) throw gcnew System::ApplicationException(L"input obj cannot be null");

			bool test_file = false;
			String^ fn = input_item->Members[L"Path"]->Value->ToString();
			if (!fn->Contains(L"\\PCMark_10_Storage_")) test_file = true;

			String^ detail = input_item->Members[L"Detail"]->Value->ToString();
			UINT offset = 0, secs = 0;
			String^ result = input_item->Members[L"Result"]->Value->ToString();

			UINT hash = (UINT)fn->GetHashCode();
			String^ timestamp = input_item->Members[L"Time of Day"]->Value->ToString();
			double ts = m_parser->ParseTimestamp(timestamp);
			String^ op = input_item->Members[L"Operation"]->Value->ToString();
			//			String^ op_code;
			bool is_dir = false;
			OPERATION_CODE op_code;
			OPERATION_CODE file = OPERATION_CODE::NDF;
			if (op == L"WriteFile")
			{
				m_parser->ParseReadWrite(detail, offset, secs);
				op_code = OPERATION_CODE::WR;
			}
			else if (op == L"ReadFile")
			{
				m_parser->ParseReadWrite(detail, offset, secs);
				op_code = OPERATION_CODE::RD;
			}
			else if (op == L"CreateFile")
			{
				op_code = OPERATION_CODE::CC;
				if (result == "SUCCESS")
				{
					op_code = m_parser->ParseCreate(detail, is_dir);
					//auto match = m_reg_create->Match(detail);
					//if (match->Success)
					//{
					//	String^ disc = match->Groups[2]->Value;
					//	String^ dir = match->Groups[1]->Value;
					//	if (disc == L"Created")
					//	{
					//		if (dir == L" Directory,")					op_code = OPERATION_CODE::MD, file = OPERATION_CODE::DIR;	// create dir
					//		else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::CR, file = OPERATION_CODE::FIL;	// create filea
					//		else op_code = OPERATION_CODE::CREATE;
					//	}
					//	else if (disc == L"Opened")
					//	{
					//		if (dir == L" Directory,")					op_code = OPERATION_CODE::CD, file = OPERATION_CODE::DIR;	// open dir
					//		else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OP, file = OPERATION_CODE::FIL;	// open file
					//		else if (dir == L" Delete,")				op_code = OPERATION_CODE::RM;
					//		else op_code = OPERATION_CODE::OPEN;
					//	}
					//	else if (disc == L"Overwritten")
					//	{
					//		if (dir == L" Directory,")					op_code = OPERATION_CODE::OVERWRITE_DIR;	// open dir
					//		else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OW;	// overwritten file
					//		else op_code = OPERATION_CODE::OVERWRITTEN;
					//	}
					//	else	WriteDebug(String::Format(L"Unknonw result={0}, dir={1}, timestamp={2}", disc, dir, timestamp));
					//}
					//else	WriteDebug(String::Format(L"Unmatched create detail={0}, timestamp={1}", detail, timestamp));
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
				return;
			}

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
			wchar_t str_hash[10];
			swprintf_s(str_hash, L"%08X", hash);

			if (result == L"SUCCESS")
			{
				TraceItem^ trace = gcnew TraceItem;
				trace->timestamp = timestamp;
				trace->ts = ts;
				trace->path_hash = gcnew String(str_hash);
				trace->op_code = op_code;
				trace->file = file;
				trace->offset = offset;
				trace->secs = secs;
				trace->test_file = test_file;
				trace->is_dir = is_dir;
				trace->opened = m_opened_file;
				trace->path = fn;
				WriteObject(trace);
			}
		}


	protected:
		int m_opened_file;
//		std::map<int, std::wstring>* m_fn_map;

		ParseHelper^ m_parser;
	};


	// 将TRACE转换为4元组（file name hash, op, offset, secs)
	//  将TRACE转换成标准形式，用于后续分析。
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

		[Parameter(Position = 1,  Mandatory = true,
			HelpMessage = "filter of folder")]
		property String^ folder_name;

	public:
		virtual void BeginProcessing() override
		{
			m_reg_rd = gcnew System::Text::RegularExpressions::Regex(L"Offset: ([\\d,]+), Length: ([\\d,]+)");
			m_reg_create = gcnew System::Text::RegularExpressions::Regex(L"(Non-Directory File,| Directory,| Delete,).* OpenResult: (.+)");
			//m_opened_file = 0;
		}
		virtual void InternalProcessRecord() override
		{		// 处理trace:	（1）删除不相关的列，（2）仅保留相关文件的操作，（3）文件名转换为hash
			if (input_item == nullptr) throw gcnew System::ApplicationException(L"input obj cannot be null");

			String^ fn = input_item->Members[L"Path"]->Value->ToString();
			if (!fn->Contains(folder_name)) return;

			String^ detail = input_item->Members[L"Detail"]->Value->ToString();
			UINT offset = 0, secs = 0;
			String^ result = input_item->Members[L"Result"]->Value->ToString();

			UINT hash = (UINT)fn->GetHashCode();
			String^ timestamp = input_item->Members[L"Time of Day"]->Value->ToString();
			String^ op = input_item->Members[L"Operation"]->Value->ToString();
//			String^ op_code;
			OPERATION_CODE op_code;
			OPERATION_CODE file = OPERATION_CODE::NDF;
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
				op_code = OPERATION_CODE::CC;
				if (result == "SUCCESS")
				{
					auto match = m_reg_create->Match(detail);
					if (match->Success)
					{
						String^ disc = match->Groups[2]->Value;
						String^ dir = match->Groups[1]->Value;
						if (disc == L"Created")
						{
							if (dir == L" Directory,")					op_code = OPERATION_CODE::MD, file=OPERATION_CODE::DIR;	// create dir
							else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::CR, file=OPERATION_CODE::FIL;	// create filea
							else op_code = OPERATION_CODE::CREATE;
						}
						else if (disc == L"Opened")
						{
							if (dir == L" Directory,")					op_code = OPERATION_CODE::CD, file = OPERATION_CODE::DIR;	// open dir
							else if (dir == L"Non-Directory File,")		op_code = OPERATION_CODE::OP, file = OPERATION_CODE::FIL;	// open file
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
					else	WriteDebug(String::Format(L"Unmatched create detail={0}, timestamp={1}", detail, timestamp));
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
			wchar_t str_hash[10];
			swprintf_s(str_hash, L"%08X", hash);

			if (result == L"SUCCESS")
			{
#if 1
				TraceItem^ trace = gcnew TraceItem;
				trace->timestamp = timestamp;
				trace->path_hash = gcnew String(str_hash);
				trace->op_code = op_code;
				trace->file = file;
				trace->offset = offset;
				trace->secs = secs;
				WriteObject(trace);
#else
				PSObject^ out_item = gcnew PSObject;

				AddPropertyMember(out_item, L"timestamp", timestamp);
				AddPropertyMember<String>(out_item, L"path_hash", str_hash);
				AddPropertyMember<OPERATION_CODE>(out_item, L"op_code", op_code);
				AddPropertyMember<UINT>(out_item, L"offset", offset);
				AddPropertyMember<UINT>(out_item, L"secs", secs);
				//AddPropertyMember(out_item, L"result", result);
				//AddPropertyMember<int>(out_item, L"opened", m_opened_file);
				// for debug
				//AddPropertyMember<UINT>(out_item, L"create_count", m_create_count);
				//AddPropertyMember<UINT>(out_item, L"flash_coount", m_flash_count);
				//AddPropertyMember<size_t>(out_item, L"trace_count", m_trace_count);
				WriteObject(out_item);
#endif
			}
		}

		virtual void EndProcessing() override
		{
			//wprintf_s(L"TRACE SPLIT\n");
			//wprintf_s(L"start, end, count\n");
			//for (auto it = m_trace_list->begin(); it != m_trace_list->end(); ++it)
			//{
			//	TRACE_INFO & trace = *it;
			//	wprintf_s(L"%s, %s, %lld\n", trace.m_start.c_str(), trace.m_end.c_str(), trace.m_count);
			//}
			//delete m_trace_list;
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
		//UINT m_create_count;
		//UINT m_flash_count;
		//String^ m_trace_start;
		//size_t m_trace_count;
		//OPERATION_CODE m_pre_op;
		//bool m_trace_started;
		//std::vector<TRACE_INFO> * m_trace_list;
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
		property TraceItem^ traces;

	public:
		virtual void BeginProcessing() override
		{
			read_count = 0, 	write_count = 0, 		read_secs = 0, 			write_secs = 0;
			create_count = 0, close_count = 0, flush_count = 0;
			opened_file = 0;
		}
		virtual void InternalProcessRecord() override
		{
			switch (traces->op_code)
			{
			case OPERATION_CODE::WR:
				write_count++;
				write_secs += traces->secs;
				break;
			case OPERATION_CODE::RD:
				read_count++;
				read_secs += traces->secs;
				break;
			case OPERATION_CODE::CL:
				close_count++;
				opened_file--;
				break;
			case OPERATION_CODE::FL:
				flush_count++;
				break;
			default:
				create_count++;
				opened_file--;
			}

			PSObject^ out_item = gcnew PSObject;


			AddPropertyMember(out_item, L"timestamp", traces->timestamp);
			AddPropertyMember(out_item, L"path_hash", traces->path_hash);
			AddPropertyMember<OPERATION_CODE>(out_item, L"op_code", traces->op_code);
			AddPropertyMember<OPERATION_CODE>(out_item, L"file", traces->file);
			AddPropertyMember<UINT>(out_item, L"offset", traces->offset);
			AddPropertyMember<UINT>(out_item, L"secs", traces->secs);
			AddPropertyMember<size_t>(out_item, L"opened_file", opened_file);
			AddPropertyMember<size_t>(out_item, L"read_count", read_count);
			AddPropertyMember<size_t>(out_item, L"write_count", write_count);
			AddPropertyMember<size_t>(out_item, L"read_secs", read_secs);
			AddPropertyMember<size_t>(out_item, L"write_secs", write_secs);
			AddPropertyMember<size_t>(out_item, L"create_count", create_count);
			AddPropertyMember<size_t>(out_item, L"close_count", close_count);
			AddPropertyMember<size_t>(out_item, L"flush_count", flush_count);


			//	String^ str_op;
			//	switch (it->m_op_code)
			//	{
			//	case 1:	str_op = "CR"; createfile++;	break;
			//	case 2: str_op = "CL"; closefile++;		break;
			//	case 3: str_op = "WR"; writes++;		break;
			//	case 4: str_op = "RD"; reads++;			break;
			//	case 5: str_op = "FL"; flush++;			break;
			//	}
			//	AddPropertyMember(out_item, L"op_code", str_op);
			//	AddPropertyMember<int>(out_item, L"offset", it->m_offset);
			//	AddPropertyMember<int>(out_item, L"secs", it->m_secs);

			//	AddPropertyMember<UINT>(out_item, L"reads", reads);
			//	AddPropertyMember<UINT>(out_item, L"writes", writes);
			//	AddPropertyMember<UINT>(out_item, L"createfiles", createfile);
			//	AddPropertyMember<UINT>(out_item, L"closefiles", closefile);
			//	AddPropertyMember<UINT>(out_item, L"flushes", flush);
			//	WriteObject(out_item);

			//	if (reads > 200000 && writes > 15000 && createfile > 5000 && closefile > 3000 && flush > 9) break;
			//}
		}

		virtual void EndProcessing() override
		{
			//			WriteObject(m_trace);
		}

	protected:
		//		TraceData^ m_trace;
		size_t read_count, write_count, read_secs, write_secs;
		size_t create_count, close_count, flush_count;
		size_t opened_file;

	};


}

#pragma once

#include "../include/utility.h"

using namespace System::Management::Automation;
#include <boost/cast.hpp>
#pragma make_public(jcvos::IBinaryBuffer)

namespace JcCmdLet
{
	//-----------------------------------------------------------------------------
	// -- my cmdlet base class, to handle exceptions	
	public ref class JcCmdletBase : public Cmdlet
	{
	public:
		virtual void InternalProcessRecord() {};
	protected:
//		void ShowPipeMessage(void);

	protected:
		virtual void ProcessRecord() override
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
//			ShowPipeMessage();
		}
	};



	//-----------------------------------------------------------------------------
	// -- binary type	
	public ref class BinaryType : public Object
	{
	public:
		BinaryType(jcvos::IBinaryBuffer * ibuf);
		~BinaryType(void);
		!BinaryType(void);
	public:
		property size_t Length { size_t get()
		{
			return m_data->GetSize();
		}; }

	public:
		void GetData(jcvos::IBinaryBuffer * & data) { data = m_data; data->AddRef(); }
		bool GetData(void * & data);
	protected:
		jcvos::IBinaryBuffer * m_data;
	};


	//-----------------------------------------------------------------------------
	// -- show binary
	[CmdletAttribute(VerbsData::Out, "Binary")]
	public ref class ShowBinary : public JcCmdletBase
	{
	public:
		ShowBinary(void) { secs = 2, offset = 0; }
		~ShowBinary(void) {};

	public:
		[Parameter(Position = 0,
			HelpMessage = "start address to show, in sector")]
		property size_t offset;		// in sector
		[Parameter(Position = 1,
			HelpMessage = "length to show, in sector")]
		property size_t secs;		// in sector

		[Parameter(Position = 3, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true)]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ data;


	public:
		virtual void InternalProcessRecord() override;
	};


	// -- import binary
	[CmdletAttribute(VerbsData::Import, "Binary")]
	public ref class ImportBinary : public JcCmdletBase
	{
	public:
		ImportBinary(void) {
			Mapping = SwitchParameter(false);
		};
		~ImportBinary(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			HelpMessage = "specify binary file name to import")]
		[ValidateNotNullOrEmpty]
		property String^ fn;

		[Parameter(HelpMessage = "mapping binay file (default: loading data)")]
		[Alias("m")]
		property SwitchParameter Mapping;

		[Parameter(HelpMessage = "align buffer to page")]
		[Alias("a")]
		property SwitchParameter Align;


	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- export binary
	[CmdletAttribute(VerbsData::Export, "Binary")]
	public ref class ExportBinary : public JcCmdletBase
	{
	public:
		ExportBinary(void) {};
		~ExportBinary(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			HelpMessage = "specify binary file name to import")]
		[Alias("fn")]
		[ValidateNotNullOrEmpty]
		property String^ file_name;

		[Parameter(Position = 1, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true)]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ data;

	protected:
		virtual void BeginProcessing() override
		{
//			LOG_STACK_TRACE();
			std::wstring str_fn;
			ToStdString(str_fn, file_name);
			m_file = System::IO::File::Create(file_name);
		}
		virtual void EndProcessing() override
		{
//			LOG_STACK_TRACE();		
			JCASSERT(m_file);
			m_file->Close();
		}
	public:
		virtual void InternalProcessRecord() override;

	protected:
		System::IO::FileStream ^ m_file;
	};

	//-----------------------------------------------------------------------------
	// -- Select Binary: select a partial of binary data
	[CmdletAttribute(VerbsCommon::Select, "Binary")]
	public ref class SelectBinary : public JcCmdletBase
	{
	public:
		SelectBinary(void) { offset = 0; secs = 1; };
		~SelectBinary(void) {};

	public:
		[Parameter(Position = 1,
			HelpMessage = "start address to select, in sector")]
		property size_t offset;		// in sector
		[Parameter(Position = 2,
			HelpMessage = "length to select, in sector")]
		property size_t secs;		// in sector

		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true)]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ data;

	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- convert binary to array and conver array to binary
	[CmdletAttribute(VerbsData::ConvertTo, "Binary")]
	public ref class ConvertArrayToBinary : public JcCmdletBase
	{
	public:
		ConvertArrayToBinary(void) {};
	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true,
			HelpMessage = "input data in array")]
		[ValidateNotNullOrEmpty]
		property array<BYTE> ^ DataIn;

	protected:
		array<BYTE> ^ m_buf;
		size_t		m_len;

	public:
		virtual void BeginProcessing()	override
		{
			array<BYTE> ^ m_buf = gcnew array<BYTE>(0);
			m_len = 0;
		}

		virtual void InternalProcessRecord() override;
		virtual void EndProcessing()	override;
	};

	[CmdletAttribute(VerbsData::ConvertFrom, "Binary")]
	public ref class ConvertBinaryToArray : public JcCmdletBase
	{
	public:
		ConvertBinaryToArray(void) { WordLen = 1; };
		~ConvertBinaryToArray(void) {};

	public:
		[Parameter(Position = 0, Mandatory = true,
			ValueFromPipeline = true,
			HelpMessage = "conver binay data into BYTE (1), WORD (2) or DWORD (4). (in bytes)")]
		[ValidateNotNullOrEmpty]
		property int WordLen;

		[Parameter(Position = 1, Mandatory = true,
			ValueFromPipeline = true,
			HelpMessage = "input binary type data")]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ Data;

	public:
		virtual void InternalProcessRecord() override;
	};

	//-----------------------------------------------------------------------------
	// -- compare data
	[CmdletAttribute(VerbsData::Compare, "Binary")]
	public ref class CompareBinary : public JcCmdletBase
	{
	public:
		CompareBinary(void) { Secs = 1; };
		~CompareBinary(void) {};

	public:
		[Parameter(Position = 2,
			HelpMessage = "size of to compare in sectors")]
		property size_t Secs;		// in sector

		[Parameter(Position = 0, ValueFromPipeline = true,
			ValueFromPipelineByPropertyName = true)]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ Data1;

		[Parameter(Position = 1)]
		[ValidateNotNullOrEmpty]
		property BinaryType ^ Data2;

	protected:
		BYTE * CheckData(BinaryType ^ data, jcvos::IBinaryBuffer * & buf, size_t & len);

	public:
		virtual void InternalProcessRecord() override;
	};


};

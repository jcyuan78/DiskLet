#pragma once


using namespace System;
using namespace System::Management::Automation;
#include <boost/cast.hpp>
#pragma make_public(jcvos::IBinaryBuffer)

namespace JcCmdLet
{
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
	public ref class ShowBinary : public Cmdlet
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


	protected:
		virtual void ProcessRecord() override;
	};

};

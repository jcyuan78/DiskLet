///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <LibProtocolAnalyzer.h>

using namespace System;
using namespace System::Management::Automation;


namespace palet
{
	public ref class PAPacket : public Object
	{
	public:
		PAPacket(pa::IPacket* packet)
		{
			m_packet = packet;
			if (packet) m_packet->AddRef();
		}
		~PAPacket(void)		{ if (m_packet) m_packet->Release(); }
	public:
	protected:
		pa::IPacket* m_packet;
	};

	public ref class PATrace : public Object
	{
	public:
		PATrace(pa::ITrace* trace)
		{
			m_trace = trace;
			if (m_trace) m_trace->AddRef();
		}
		~PATrace(void)		{ if (m_trace)m_trace->Release();	}
		void GetTracePtr(pa::ITrace*& trace) { trace = m_trace; if (m_trace) trace->AddRef(); }
		void CloseTrace(void) { m_trace->Close(); m_trace->Release(); m_trace = NULL; }
	protected:
		pa::ITrace* m_trace;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Open Trace

	[CmdletAttribute(VerbsCommon::Open, "Trace")]
	public ref class OpenTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		OpenTrace(void) {};
		~OpenTrace(void) {};
	public:
		[Parameter(Position = 0,	/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "trace file name")]
		property String^ fn;
	public:
		virtual void InternalProcessRecord() override;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Close Trace

	[CmdletAttribute(VerbsCommon::Close, "Trace")]
	public ref class CloseTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		CloseTrace(void) {};
		~CloseTrace(void) {};
	public:
		[Parameter(Position = 0, /*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "input trace")]
		property PATrace^ trace;
	public:
		virtual void InternalProcessRecord() override;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Get Packet

	[CmdletAttribute(VerbsCommon::Get, "Packet")]
	public ref class GetPacket : public JcCmdLet::JcCmdletBase
	{
	public:
		GetPacket(void) {};
		~GetPacket(void) {};

	public:
		[Parameter(/*Position = 0, ParameterSetName = "ByObject",*/
			/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "input trace")]
		property PATrace ^ trace;

		[Parameter(/*Position = 0, ParameterSetName = "ByObject",*/
			/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "specify disk")]
		property UINT64 index;


	public:
		virtual void InternalProcessRecord() override;
	};

	public enum class DECODE_LEVEL
	{
		LEVEL_PACKET = 0,		//Decoding Level Packet
		LEVEL_LINKTRA = 1,		//Decoding Level Link Transaction
		LEVEL_SPLITTRA = 2,	//Decoding Level Split	Transaction
		LEVEL_NVMHCI = 3,		//Decoding Level NVM	Transaction
		LEVEL_PQI = 4,			//Decoding Level PQI
		LEVEL_AHCI = 5,		//Decoding Level AHCI
		LEVEL_ATA = 6,			//Decoding Level ATA
		LEVEL_SOP = 7,			//Decoding Level SOP
		LEVEL_SCSI = 8,		//Decoding Level SCSI
		LEVEL_NVMC = 9,		//Decoding Level NVM Command
		LEVEL_RRAP = 10,		//Decoding Level RRAP
		LEVEL_MCTP_MSG = 11,	//Decoding Level MCTP MSG
		LEVEL_MCTP_CMD = 12,	//Decoding Level MCTP CMD
		LEVEL_LM = 13,			//Decoding Level LM
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Export Trace

	[CmdletAttribute(VerbsData::Export, "Trace")]
	public ref class ExportTrace : public JcCmdLet::JcCmdletBase
	{
	public:
		ExportTrace(void) {};
		~ExportTrace(void) {};

	public:
		[Parameter(/*Position = 0, */ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "input trace data")]
		property PATrace^ trace;

		[Parameter(Position = 0, /*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "output file name")]
		property String ^ fn;

		[Parameter(Position = 1, /*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "decode level")]
		property DECODE_LEVEL level;

	public:
		virtual void InternalProcessRecord() override;
	};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Get Payload of a command

	[CmdletAttribute(VerbsCommon::Get, "Payload")]
	public ref class GetPalyload : public JcCmdLet::JcCmdletBase
	{
	public:
		GetPalyload(void) {};
		~GetPalyload(void) {};

	public:
		[Parameter(Position = 0,/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "input trace")]
		property PATrace^ trace;

		[Parameter(Position = 1,/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "start timestame of the payload")]
		property double start_time;

		[Parameter(Position = 2,/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "start address of the payload")]
		property UINT64 start_addr;

		[Parameter(Position = 3,/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "data length in byte")]
		property UINT data_size;

		[Parameter(Position = 4,/*ValueFromPipelineByPropertyName = true,*/ValueFromPipeline = true, Mandatory = true,
			HelpMessage = "send or receive")]
		property bool receive;

	public:
		virtual void InternalProcessRecord() override;

	};

}

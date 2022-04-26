///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "palet.h"

LOCAL_LOGGER_ENABLE(L"vss.app", LOGGER_LEVEL_DEBUGINFO);

void palet::OpenTrace::InternalProcessRecord()
{
	std::wstring str_fn;
	ToStdString(str_fn, fn);
	jcvos::auto_interface<pa::IAnalyzer> analyzer;
	pa::CreateAnalyzer(analyzer, true);
	if (analyzer == nullptr) throw gcnew System::ApplicationException(L"failed on creating pe analyzer");
	jcvos::auto_interface<pa::ITrace> _trace;
	analyzer->OpenTrace(_trace, str_fn);

	palet::PATrace^ trace = gcnew palet::PATrace(_trace);
	WriteObject(trace);
}

void palet::GetPacket::InternalProcessRecord()
{
	jcvos::auto_interface<pa::ITrace> _trace;
	trace->GetTracePtr(_trace);
	if (_trace == nullptr) throw gcnew System::ApplicationException(L"input trace is empty");
	jcvos::auto_interface<pa::IPacket> _packet;
	_trace->GetPacket(_packet, index);
	if (_packet == nullptr) throw gcnew System::ApplicationException(L"failed on getting packet");

	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	_packet->GetPacketData(buf);
	JcCmdLet::BinaryType^ binary = gcnew JcCmdLet::BinaryType(buf);
	WriteObject(binary);
	//PAPacket^ packet = gcnew PAPacket(_packet);
	//WriteObject(packet);
}

void palet::ExportTrace::InternalProcessRecord()
{
	jcvos::auto_interface<pa::ITrace> _trace;
	trace->GetTracePtr(_trace);
	if (_trace == nullptr) throw gcnew System::ApplicationException(L"input trace is empty");

	std::wstring str_fn;
	ToStdString(str_fn, fn);
	//_trace->ExportTraceToCsv(str_fn, level.ToInt32(nullptr));
}

void palet::GetPalyload::InternalProcessRecord()
{
	jcvos::auto_interface<pa::ITrace> _trace;
	trace->GetTracePtr(_trace);
	if (_trace == nullptr) throw gcnew System::ApplicationException(L"input trace is empty");
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	_trace->GetCommandPayload(buf, start_time, start_addr, data_size, receive);

	JcCmdLet::BinaryType^ binary = gcnew JcCmdLet::BinaryType(buf);
	WriteObject(binary);
}

void palet::CloseTrace::InternalProcessRecord()
{
	trace->CloseTrace();
}

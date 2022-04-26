///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "security-parser.h"
#include "global.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == 

void SecurityParser::ParseSecurity::InternalProcessRecord()
{
	//$protocol = ($cdw10 - band 0xFF000000) / 0x1000000;
	//$comid = ($cdw10 - band 0xFFFF00) / 0x100;
	DWORD protocol = cdw10 >> 24;
	DWORD comid = (cdw10 >> 8) & 0xFFFF;

	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	payload->GetData(buf);

	//std::vector<tcg::ISecurityObject*> security_objects;
	jcvos::auto_interface<tcg::ISecurityObject> security_object;
	global.m_parser->ParseSecurityCommand(security_object, buf, protocol, comid, receive);
	if (security_object)
	{
		SecurityObj^ cmd = gcnew SecurityObj(security_object);
		cmd->SetNvmeId(nvme_id);
		//cmd->AddObjects(security_object);
		WriteObject(cmd);
	}
}

void SecurityParser::ExportSecurity::BeginProcessing()
{
	std::wstring fn;
	ToStdString(fn, filename);

	m_output = new std::wofstream(fn);
	if (!m_output) throw gcnew System::ApplicationException(L"failed on open file " + filename);
	*m_output << std::showbase;
}

void SecurityParser::ExportSecurity::EndProcessing()
{
	if (m_output)
	{
		*m_output << std::noshowbase;
		m_output->close();
		delete m_output;
		m_output = NULL;
	}
}

void SecurityParser::ExportSecurity::InternalProcessRecord()
{
	if (!m_output) throw gcnew System::ApplicationException(L"failed on open file");
	if (payload == nullptr) return;
	jcvos::auto_interface<tcg::ISecurityObject> obj;
	(*m_output) << L"NVMe: " << payload->GetNvmeId() << std::endl;
	payload->GetObject(obj);
	if (!obj) return;
	obj->ToString(*m_output, level, option);
	*m_output << std::endl;
}

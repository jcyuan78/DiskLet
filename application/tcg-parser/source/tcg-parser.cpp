#include "pch.h"

#include "tcg-parser.h"
#include "l0discovery.h"
#include <io.h>
#include <fcntl.h>
#include <boost/cast.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace prop_tree = boost::property_tree;


LOCAL_LOGGER_ENABLE(L"tcg_parser", LOGGER_LEVEL_NOTICE);
//
//void EndianSubPacket(SUBPACKET& ss)
//{
//	endian_reverse_inplace(ss.kind);
//	endian_reverse_inplace(ss.length);
//}



///////////////////////////////////////////////////////////////////////////////
// ==== Parce TCG Package




///////////////////////////////////////////////////////////////////////////////
// ==== Parce TCG Token

tcg_parser::ParseTcgToken::ParseTcgToken(void)
{
	Payload = nullptr;
}

tcg_parser::ParseTcgToken::~ParseTcgToken(void)
{
	//delete Payload;
}

void tcg_parser::ParseTcgToken::InternalProcessRecord()
{
//	PSObject^ token_list = gcnew PSObject;
	size_t len = Payload->Length;

//	TcgToken^ token = gcnew TcgToken;
//	CTcgTokenBase* top = token->GetToken();

	BYTE* buf = Payload->LockData();
	BYTE* ptr = buf;
	BYTE* end_buf = buf + len;
//	top->ParseToken(ptr, end_buf);
	LOG_DEBUG(L"receive = %d", receive);
	CTcgTokenBase* tt = CTcgTokenBase::SyntaxParse(ptr, end_buf, buf, /*receive.ToBool()*/ receive);
	Payload->Unlock();
	TcgToken^ token = gcnew TcgToken(tt);
	WriteObject(token);
}

tcg_parser::TcgToken::TcgToken(void) : m_token(nullptr)
{
//	m_token = new CTcgTokenBase(CTcgTokenBase::TopToken);
	m_token = static_cast<CTcgTokenBase*>(new ListToken(CTcgTokenBase::TopToken));
}

tcg_parser::TcgToken::~TcgToken(void)
{
	delete m_token;
	m_token = nullptr;
}

tcg_parser::TcgToken::!TcgToken(void)
{
	delete m_token;
	m_token = nullptr;
}

System::String ^ tcg_parser::TcgToken::Print(void)
{
#if 0
	wchar_t temp_path[MAX_PATH];
	GetTempPath(MAX_PATH - 1, temp_path);
	wchar_t temp_fn[MAX_PATH];
	GetTempFileName(temp_path, L"TCG", 0, temp_fn);
	HANDLE hfile = CreateFile(temp_fn, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, CREATE_ALWAYS, /*FILE_ATTRIBUTE_TEMPORARY|*/ FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hfile == INVALID_HANDLE_VALUE) throw gcnew System::ApplicationException(
		System::String::Format(gcnew System::String(L"failed on opening temp file: {0}"), gcnew System::String(temp_fn)) 
	);
	FILE* ff = _wfdopen(_open_osfhandle((int)hfile, _O_APPEND | _O_WTEXT), L"w+");
	m_token->Print(ff, 0);
	fflush(ff);
	HANDLE mapping = CreateFileMapping(hfile, NULL, PAGE_READWRITE, 0, 64 * 1024, NULL);
	if (mapping == INVALID_HANDLE_VALUE) throw gcnew System::ApplicationException(L"failed on creating file mapping");
	wchar_t* str = (wchar_t*)MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 64 * 1024);
	if (str == nullptr) throw gcnew System::ApplicationException(L"failed on allocating mapping memory");

	System::String^ ss = gcnew System::String(str);
	UnmapViewOfFile(str);
	CloseHandle(mapping);

	CloseHandle(hfile);
	return ss;
#else
	m_token->Print(stdout, 0);
	return gcnew System::String(L"\n");
#endif
}

//bool AnalyzeTcgProtocol(CTcgTokenBase* tt)
//{
//	tt->Begin();
//	CTcgTokenBase* cur_token = tt->Font();
//	switch (cur_token->GetType())
//	{
//		case CTcgTokenBase::
//	}
//}

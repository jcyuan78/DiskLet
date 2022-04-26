///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "secure-let.h"
#include "global.h"

LOCAL_LOGGER_ENABLE(L"security_let", LOGGER_LEVEL_NOTICE);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == global ==
StaticInit global;

StaticInit::StaticInit(void)
	: m_parser(NULL)
{
	std::wstring config = L"security.cfg";
	//Init for logs
	LOG_STACK_TRACE();
	// for test
	wchar_t dllpath[MAX_PATH] = { 0 };
//#ifdef _DEBUG
//	GetCurrentDirectory(MAX_PATH, dllpath);
//	wprintf_s(L"current path=%s\n", dllpath);
//	memset(dllpath, 0, sizeof(wchar_t) * MAX_PATH);
//#endif
	// 获取DLL路径
	MEMORY_BASIC_INFORMATION mbi;
	static int dummy;
	VirtualQuery(&dummy, &mbi, sizeof(mbi));
	HMODULE this_dll = reinterpret_cast<HMODULE>(mbi.AllocationBase);
	::GetModuleFileName(this_dll, dllpath, MAX_PATH);
	// Load log config
	std::wstring fullpath, fn;
	jcvos::ParseFileName(dllpath, fullpath, fn);
//#ifdef _DEBUG
//	wprintf_s(L"module path=%s\n", fullpath.c_str());
//#endif
	LOGGER_CONFIG(config.c_str(), fullpath.c_str());
	LOG_DEBUG(_T("log config..."));

	//g_init = true;

	tcg::CreateSecurityParser(m_parser);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// == Security Let ==

void SecureLet::InitSecurityParser::InternalProcessRecord()
{
	std::wstring uuid_fn;
	std::wstring l0_fn;

	ToStdString(uuid_fn, uuid);
	ToStdString(l0_fn, l0discovery);

	global.m_parser->Initialize(uuid_fn, l0_fn);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <LibTcg.h>


using namespace System;
//using namespace System::Runtime::InteropServices;

class StaticInit
{
public:
	StaticInit(void);
	~StaticInit(void) { RELEASE(m_parser); };

public:
	void InitialTcg(const std::wstring& uid_config, const std::wstring& l0discovery_config)
	{

	}

public:
	tcg::ISecurityParser* m_parser;

protected:

};

extern StaticInit global;
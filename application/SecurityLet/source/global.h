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

inline char* HexDecoder(const std::string& data)
{
	size_t out_size = data.size() / 2;
	char* out_buf = new char[out_size];
	size_t ii = 0;
	memset(out_buf, 0, out_size);
//	std::stringstream ss(data);
	
	const char* src = data.c_str();
	while (1)
	{
		if (ii >= out_size) THROW_ERROR(ERR_APP, L"wrong format, index overflow");
		while ((*src == ' ') || (*src == '\t') || (*src == '\n') || (*src == '\r')) src++;
		if (*src == 0) break;
		size_t ss = 2;
		out_buf[ii] = (BYTE)std::stoi(src, &ss, 16);
		src += ss; ii++;
	}
	return out_buf;
}
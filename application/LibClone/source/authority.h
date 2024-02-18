#pragma once

#include <lib_authorizer.h>

using namespace System;

namespace authority
{
	public ref class Authority : public Object
	{
	public:
		Authority(void) {}
		~Authority(void) {}
	public:
		static bool AddLicense(String^ license);
	};
}
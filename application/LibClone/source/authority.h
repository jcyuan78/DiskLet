#pragma once

#include <lib_authorizer.h>

using namespace System;

namespace Clone
{
	public enum class LICENSE_TYPE
	{
		LICENSE_NONE = 0,
		LICENSE_VALIDATE = (1<<Authority::LT_VALIDATE),
		LICENSE_INTERNAL = (1 << Authority::LT_INTERNAL),
		LICENSE_DELIVERY = (1<< Authority::LT_DELIVERY),
		LICENSE_DRICE = (1<<Authority::LT_DRIVE),
	};

	public ref class CloneAuthority : public Object
	{
	public:
		CloneAuthority(void) {}
		~CloneAuthority(void) {}
	public:
		static void AddLicense(String^ license);
//		static void RemoveLicense(String^ license);
		static UINT64 GetLicenseList(void);
	};
}
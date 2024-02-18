#pragma once

#include <LibAuthorGen.h>

class CAuthorizeServiceApp
	: public jcvos::CJCAppSupport<jcvos::AppArguSupport>
{
protected:
	typedef jcvos::CJCAppSupport<jcvos::AppArguSupport> BaseAppClass;

public:
	static const TCHAR LOG_CONFIG_FN[];
	CAuthorizeServiceApp(void);
	virtual ~CAuthorizeServiceApp(void);

public:
	virtual int Initialize(void);
	virtual int Run(void);
	virtual void CleanUp(void);
	virtual LPCTSTR AppDescription(void) const
	{
		return L"Scheduler, by Jingcheng Yuan\n";
	};

protected:
	void LoadConfig(const std::wstring& config);



public:
	std::wstring m_config_fn;		// Դvolume

public:
	CApplicationInfo m_app_info;
};

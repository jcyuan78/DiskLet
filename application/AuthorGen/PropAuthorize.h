#pragma once

#include <LibAuthorGen.h>

// CPropAuthorize 对话框

class CPropAuthorize : public CMFCPropertyPage
{
	DECLARE_DYNAMIC(CPropAuthorize)

public:
	CPropAuthorize(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPropAuthorize();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PROP_AUTHORIZE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	std::wstring m_pc_name;
	std::wstring m_user_name;
	std::string m_delivery;
	std::string m_license;
	afx_msg void OnMakeAuthorize();
	afx_msg void OnReauthorize();
	afx_msg void OnRebound();

public:
	CApplicationInfo* m_app_info;
	afx_msg void OnReboundCount();
};

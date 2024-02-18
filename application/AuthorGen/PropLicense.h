#pragma once

#include <LibAuthorGen.h>
#include <vector>

// CPropLicense 对话框

class CPropLicense : public CMFCPropertyPage
{
	DECLARE_DYNAMIC(CPropLicense)

public:
	CPropLicense(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPropLicense();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PROP_LICENSE };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	std::wstring m_computer_name;
//	std::wstring m_deviceid;
	std::wstring m_register_date;
	UINT32 m_period;
	std::string m_delivery_key;
	afx_msg void OnAuthorize();

	CApplicationInfo* m_app_info;
	std::string m_str_license;
	std::wstring m_verify_result;
	afx_msg void OnBnClickedVerifyAuthor();
	BOOL m_internal_license;
	CListCtrl m_drivelist_ctrl;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedLoadDrives();

protected:
	void RefreshDriveList();
protected:
	std::vector<std::wstring> m_drive_list;
public:
	BOOL m_include_drive_list;
	BOOL m_include_delivery;
};

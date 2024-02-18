#pragma once

#include <LibAuthorGen.h>

// CPropAppManager 对话框

class CPropAppManager : public CMFCPropertyPage
{
	DECLARE_DYNAMIC(CPropAppManager)

public:
	CPropAppManager(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPropAppManager();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PROP_APPMANAGER };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
	// Software Id
	std::string m_string_appid;			// 控件显示
	std::string m_str_uuid_appid;			// 控件
	CString m_str_id_appid;					// IDC_SWID
	// Master Id
	std::string m_string_masterid;
	std::string m_str_uuid_masterid;
	CString m_str_id_masterid;
	std::string m_master_key_finger;
	//
	std::string m_password;
	std::string m_password_finger;
	//
	DWORD m_start_sn;
	int m_delivery_count;
	//
	CListCtrl m_delivery_list;

	friend class CAuthorGenDlg;
protected:
	CApplicationInfo * m_app_info;
	void RefreshDeliveryList(void);

public:
	afx_msg void OnChangeAppId();
	afx_msg void OnChangeMasterId();
	afx_msg void OnGenerateMasterKey();
	afx_msg void OnSaveMasterKey();
	afx_msg void OnLoadMasterKey();
	afx_msg void OnUpdateSnPassword();
	afx_msg void OnSaveAppInfo();
	afx_msg void OnLoadAppInfo();
	afx_msg void OnBnClickedGenDelivery();

	virtual BOOL OnInitDialog();
	afx_msg void OnExporDelivery();
	afx_msg void OnBnClickedRefreshDelivery();
	afx_msg void OnBnClickedExportPublickey();
	afx_msg void OnSaveDelivery();
	afx_msg void OnLoadDelivery();
};



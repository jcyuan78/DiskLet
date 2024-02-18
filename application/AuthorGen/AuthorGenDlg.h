
// AuthorGenDlg.h: 头文件
//

#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include <cryptopp/sha.h>
#include <cryptopp/rsa.h>

#include "PropAppManager.h"
#include "PropLicense.h"
#include "PropAuthorize.h"

#define MAX_PROPERTY_PAGE 5

const boost::uuids::uuid server_id;

// Configuration 
//	Master Key 长度，字节单位
#define MASTER_KEY_LEN	128

class CLicense
{
public:
//	boost::uuids::uuid m_server_id;
	DWORD	m_master_id;
	DWORD	m_software_id;
	DWORD	m_serial_number;
	UINT64	m_system_id;
	UINT64	m_device_id;
	boost::gregorian::date m_start_date;
	UINT	m_period;
	UINT	m_crc;
};

// CAuthorGenDlg 对话框
class CAuthorGenDlg : public CDialogEx
{
// 构造
public:
	CAuthorGenDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUTHORGEN_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持



// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:

	// 软件序列号
	DWORD m_start_sn;
//	BYTE m_serial_number_out[16];		// 输出的软件序列号
	std::string m_string_sn;			// IDC_SERIAL_NO


//	CryptoPP::RSA m_server_key;
	CryptoPP::RSA::PrivateKey m_server_private_key;
	CryptoPP::RSA::PublicKey m_server_public_key;
	// private key 指纹
	std::string m_rsakey_finger;

	// BASE64格式保存的private key
	afx_msg void OnClickedSerialNoGen();
	// 注册软件序列号，以获取证书
	afx_msg void OnClickedAuthorize();
	// 生成RSA Key
	afx_msg void OnClickedRsaKeyGen();
	afx_msg void OnClickedRsaKeyLoad();

	std::string m_authorization_value;
	afx_msg void OnClickedSaveConfig();
	afx_msg void OnBnClickedLoadConfig();



	// 用于加密序列号的key
	std::string m_string_keysn;
	std::string m_str_sha_keysn;
	BYTE m_keysn[CryptoPP::SHA1::DIGESTSIZE];
	afx_msg void OnClickedSetKeySn();


	std::string m_string_masterid;
	boost::uuids::uuid m_uuid_masterid;
	DWORD m_id_masterid;						// 软件ID

	// App Id
	std::string m_string_softid;
	boost::uuids::uuid m_uuid_softid;
	DWORD m_id_softid;						// 软件ID



protected:
	void SetStringId(const std::string& string_id, boost::uuids::uuid& uuid, DWORD& id, std::string & str_uuid, CString & str_dword);
	void SetSnKey(const std::string& string_snkey);
	void SetPrivateKey(const CryptoPP::RSA::PrivateKey& key);

// property tags
public:
	afx_msg void OnClickedVerifyAuthor();
//	CMFCPropertySheet m_tabs;
	CTabCtrl m_tabs;
	afx_msg void OnTcnSelchangeTabs(NMHDR* pNMHDR, LRESULT* pResult);

	CMFCPropertyPage* m_property_pages[MAX_PROPERTY_PAGE];
	
	CPropAppManager		m_page1;
	CPropLicense		m_page2;
	CPropAuthorize		m_page3;

	CApplicationInfo m_app_info;

	afx_msg void OnUpdateMasteridStr();


};


void _DDX_Text(CDataExchange* pDX, int id, std::string& str);
void _DDX_Text(CDataExchange* pDX, int id, std::wstring& str);

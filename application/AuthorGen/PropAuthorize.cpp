// CPropAuthorize.cpp: 实现文件
//

#include "pch.h"
#include "AuthorGen.h"
#include "PropAuthorize.h"
#include "afxdialogex.h"
#include "AuthorGenDlg.h"


// CPropAuthorize 对话框

IMPLEMENT_DYNAMIC(CPropAuthorize, CMFCPropertyPage)

CPropAuthorize::CPropAuthorize(CWnd* pParent /*=nullptr*/)
	: CMFCPropertyPage(IDD_PROP_AUTHORIZE), m_delivery("")	, m_license("")
{

}

CPropAuthorize::~CPropAuthorize()
{
}

void CPropAuthorize::DoDataExchange(CDataExchange* pDX)
{
	CMFCPropertyPage::DoDataExchange(pDX);
	_DDX_Text(pDX, IDC_PCNAME, m_pc_name);
	_DDX_Text(pDX, IDC_USERNAME, m_user_name);
	_DDX_Text(pDX, IDC_DELIVERY_KEY, m_delivery);
	_DDX_Text(pDX, IDC_LICENSE, m_license);
}


BEGIN_MESSAGE_MAP(CPropAuthorize, CMFCPropertyPage)
	ON_BN_CLICKED(IDC_MAKE_AUTHORIZE, &CPropAuthorize::OnMakeAuthorize)
	ON_BN_CLICKED(IDC_REAUTHORIZE, &CPropAuthorize::OnReauthorize)
	ON_BN_CLICKED(IDC_REBOUND, &CPropAuthorize::OnRebound)
	ON_BN_CLICKED(IDC_REBOUND_COUNT, &CPropAuthorize::OnReboundCount)
END_MESSAGE_MAP()


// CPropAuthorize 消息处理程序


void CPropAuthorize::OnMakeAuthorize()
{
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	m_license.clear();
	bool br = false;
//	br = m_app_info->GenerateLicense(m_license, m_delivery,	m_pc_name, 0, false);
	br = m_app_info->AuthorizeRegister(m_license, m_delivery, m_pc_name, m_user_name);

	if (!br) m_license = "invalide delivery key";
	UpdateData(FALSE);
}


void CPropAuthorize::OnReauthorize()
{
	UpdateData(TRUE);
	m_license.clear();
	bool br = false;
	br = m_app_info->Reauthorize(m_license, m_user_name);

	if (!br) m_license = "invalide user";
	UpdateData(FALSE);
}


void CPropAuthorize::OnRebound()
{
	// TODO: 在此添加控件通知处理程序代码
}


void CPropAuthorize::OnReboundCount()
{
	UpdateData(TRUE);
	m_license.clear();
	int count = m_app_info->GetReboundCount(m_user_name);
	if (count < 0)
	{
		m_license = "invalide user";
	}
	else
	{
		m_license = "rebound count = ";
		m_license += std::to_string(count);
	}
	UpdateData(FALSE);
	// TODO: 在此添加控件通知处理程序代码
}

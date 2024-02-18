// CPropLicense.cpp: 实现文件
//

#include "pch.h"
#include "AuthorGen.h"
#include "PropLicense.h"
#include "afxdialogex.h"
#include "AuthorGenDlg.h"

#include <boost/property_tree/xml_parser.hpp>


// CPropLicense 对话框

IMPLEMENT_DYNAMIC(CPropLicense, CMFCPropertyPage)

CPropLicense::CPropLicense(CWnd* pParent /*=nullptr*/)
	: CMFCPropertyPage(IDD_PROP_LICENSE)
	, m_period(0)
	, m_internal_license(FALSE)
	, m_include_drive_list(FALSE)
	, m_include_delivery(FALSE)
{

}

CPropLicense::~CPropLicense()
{
}

void CPropLicense::DoDataExchange(CDataExchange* pDX)
{
	CMFCPropertyPage::DoDataExchange(pDX);
	_DDX_Text(pDX, IDC_SYSTEMID, m_computer_name);
	//	_DDX_Text(pDX, IDC_DEVICEID, m_deviceid);
	_DDX_Text(pDX, IDC_REGISTER_DATE, m_register_date);
	DDX_Text(pDX, IDC_AUTHOR_PERIOD, m_period);
	_DDX_Text(pDX, IDC_DELIVERY_KEY, m_delivery_key);
	_DDX_Text(pDX, IDC_AUTHORIZATION, m_str_license);
	_DDX_Text(pDX, IDC_VERIFY_RESULT, m_verify_result);
	DDX_Check(pDX, IDC_INTERNAL_LICENSE, m_internal_license);
	DDX_Control(pDX, IDC_DRIVE_LIST, m_drivelist_ctrl);
	DDX_Check(pDX, IDC_INCLUDE_DRIVES, m_include_drive_list);
	DDX_Check(pDX, IDC_INCLUDE_DELIVERY, m_include_delivery);
}


BEGIN_MESSAGE_MAP(CPropLicense, CMFCPropertyPage)
	ON_BN_CLICKED(IDC_AUTHORIZE, &CPropLicense::OnAuthorize)
	ON_BN_CLICKED(IDC_VERIFY_AUTHOR, &CPropLicense::OnBnClickedVerifyAuthor)
	ON_BN_CLICKED(IDC_LOAD_DRIVES, &CPropLicense::OnBnClickedLoadDrives)
END_MESSAGE_MAP()


// CPropLicense 消息处理程序


void CPropLicense::OnAuthorize()
{
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	m_str_license.clear();
	bool br = false;
	if (m_include_delivery)
	{
		br = m_app_info->GenerateLicense(m_str_license, m_delivery_key,
			m_computer_name, m_period, m_internal_license != 0);
	}
	else if (m_include_drive_list)
	{
		br = m_app_info->GenerateDriveLicense(m_str_license, m_delivery_key, m_drive_list);
	}
	
	if (!br) m_str_license = "invalide delivery key";
	UpdateData(FALSE);

}


void CPropLicense::OnBnClickedVerifyAuthor()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	boost::property_tree::wptree result_pt;
	bool br = m_app_info->VerifyLicense(m_str_license, result_pt);
	if (!br) m_verify_result = L"invalid license";
	else
	{
		std::wstringstream str_stream;
		boost::property_tree::xml_writer_settings<std::wstring> setting('\n', 1);
		boost::property_tree::write_xml(str_stream, result_pt, setting);
		m_verify_result = str_stream.str();
	}
	UpdateData(FALSE);
}


BOOL CPropLicense::OnInitDialog()
{
	CMFCPropertyPage::OnInitDialog();

	// 初始化 list
	LONG lStyle = GetWindowLong(m_drivelist_ctrl.GetSafeHwnd(), GWL_STYLE);//获取当前窗口style
	lStyle &= ~LVS_TYPEMASK; //清除显示方式位
	lStyle |= LVS_REPORT; //设置style
	SetWindowLong(m_drivelist_ctrl.GetSafeHwnd(), GWL_STYLE, lStyle);//设置style

	DWORD dwStyle = m_drivelist_ctrl.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;//选中某行使整行高亮（只适用与report风格的listctrl）
	dwStyle |= LVS_EX_GRIDLINES;//网格线（只适用与report风格的listctrl）
	m_drivelist_ctrl.SetExtendedStyle(dwStyle); //设置扩展风格

	m_drivelist_ctrl.InsertColumn(0, L"Drive Name", 0, 250);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void CPropLicense::OnBnClickedLoadDrives()
{
	JCASSERT(m_app_info);
	CFileDialog file_dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"config file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;

	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, (const wchar_t*)(file_dlg.GetPathName()));
	boost::property_tree::wptree root_pt;
	boost::property_tree::read_xml(str_fn, root_pt);

	m_drive_list.clear();
	const boost::property_tree::wptree& drive_list_pt = root_pt.get_child(L"drives");
	for (auto it = drive_list_pt.begin(); it != drive_list_pt.end(); ++it)
	{
		if (it->first == L"drive")
		{
			const std::wstring drive = it->second.get_value<std::wstring>();
			m_drive_list.push_back(drive);
		}
	}

	RefreshDriveList();
}

void CPropLicense::RefreshDriveList()
{
	m_drivelist_ctrl.DeleteAllItems();
	int index = 0;
	for (auto it = m_drive_list.begin(); it != m_drive_list.end(); ++it)
	{
		m_drivelist_ctrl.InsertItem(LVIF_TEXT, index, it->c_str(), 0, 0, 0, 0);
		index++;
	}
}

// CPropAppManager.cpp: 实现文件
//

#include "pch.h"


#include "AuthorGen.h"
#include "PropAppManager.h"
#include "afxdialogex.h"

#include <boost/uuid/uuid_io.hpp>



#include "AuthorGenDlg.h"
LOCAL_LOGGER_ENABLE(L"authorgen.appmanager", LOGGER_LEVEL_DEBUGINFO);

// CPropAppManager 对话框

IMPLEMENT_DYNAMIC(CPropAppManager, CMFCPropertyPage)

CPropAppManager::CPropAppManager(CWnd* pParent /*=nullptr*/)
	: CMFCPropertyPage(IDD_PROPPAGE_APPMANAGER)
	, m_delivery_count(0)
{
	//m_app_info = new CApplicationInfo;
}

CPropAppManager::~CPropAppManager()
{
	//delete m_app_info;
}

void CPropAppManager::DoDataExchange(CDataExchange* pDX)
{
	CMFCPropertyPage::DoDataExchange(pDX);
	_DDX_Text(pDX, IDC_APPID_STR, m_string_appid);
	_DDX_Text(pDX, IDC_APPID_UUID, m_str_uuid_appid);
	DDX_Text(pDX, IDC_APPID_ID, m_str_id_appid);
	_DDX_Text(pDX, IDC_MASTERID_STR, m_string_masterid);
	_DDX_Text(pDX, IDC_MASTERID_UUID, m_str_uuid_masterid);
	DDX_Text(pDX, IDC_MASTERID_ID, m_str_id_masterid);

	_DDX_Text(pDX, IDC_RSAKEY_FINGER, m_master_key_finger);
	_DDX_Text(pDX, IDC_KEYSN_STR, m_password);
	_DDX_Text(pDX, IDC_KEYSN_SHA, m_password_finger);
	DDX_Text(pDX, IDC_START_SN, m_start_sn);
	DDX_Text(pDX, IDC_DELIVERY_COUNT, m_delivery_count);
	DDX_Control(pDX, IDC_DELIVERY_LIST, m_delivery_list);
}


BEGIN_MESSAGE_MAP(CPropAppManager, CMFCPropertyPage)
	ON_EN_CHANGE(IDC_MASTERID_STR, &CPropAppManager::OnChangeMasterId)
	//ON_EN_UPDATE(IDC_MASTERID_STR, &CPropAppManager::OnUpdateMasteridStr)

	ON_EN_CHANGE(IDC_APPID_STR, &CPropAppManager::OnChangeAppId)
	ON_BN_CLICKED(IDC_RSAKEY_GEN, &CPropAppManager::OnGenerateMasterKey)
	ON_BN_CLICKED(IDC_RSAKEY_SAVE, &CPropAppManager::OnSaveMasterKey)
	ON_BN_CLICKED(IDC_RSAKEY_LOAD, &CPropAppManager::OnLoadMasterKey)
	ON_EN_KILLFOCUS(IDC_KEYSN_STR, &CPropAppManager::OnUpdateSnPassword)
	ON_BN_CLICKED(IDC_SAVE_APPINFO, &CPropAppManager::OnSaveAppInfo)
	ON_BN_CLICKED(IDC_LOAD_APPINFO, &CPropAppManager::OnLoadAppInfo)
	ON_BN_CLICKED(IDC_GEN_DELIVERY, &CPropAppManager::OnBnClickedGenDelivery)
	ON_BN_CLICKED(IDC_EXPOR_DELIVERY, &CPropAppManager::OnExporDelivery)
	ON_BN_CLICKED(IDC_REFRESH_DELIVERY, &CPropAppManager::OnBnClickedRefreshDelivery)
	ON_BN_CLICKED(IDC_EXPORT_PUBLICKEY, &CPropAppManager::OnBnClickedExportPublickey)
	ON_BN_CLICKED(IDC_SAVE_DELIVERY, &CPropAppManager::OnSaveDelivery)
	ON_BN_CLICKED(IDC_LOAD_DELIVERY, &CPropAppManager::OnLoadDelivery)
END_MESSAGE_MAP()


// CPropAppManager 消息处理程序


void CPropAppManager::OnChangeMasterId()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	m_app_info->SetMasterId(m_string_masterid);
	m_str_uuid_masterid = boost::uuids::to_string(m_app_info->GetMasterUuid());
	m_str_id_masterid.Format(L"%08X", m_app_info->GetMasterId());
	UpdateData(FALSE);
}



void CPropAppManager::OnChangeAppId()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	m_app_info->SetAppId(m_string_appid);
	m_str_uuid_appid = boost::uuids::to_string(m_app_info->GetAppUuid());
	m_str_id_appid.Format(L"%08X", m_app_info->GetAppId());
	UpdateData(FALSE);
}


void CPropAppManager::OnGenerateMasterKey()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	m_app_info->GenerateMasterKey(MASTER_KEY_LEN);
	m_master_key_finger = m_app_info->GetMasterFinger();
	UpdateData(FALSE);
}


void CPropAppManager::OnSaveMasterKey()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"Key Files (*.key)|*.key|Key Files (*.txt)|*.txt||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring fn = (const wchar_t*)(file_dlg.GetPathName()); 

	CApplicationInfo::KEY_FILE_FORMAT format;
	if (file_dlg.GetFileExt() == L"txt") format = CApplicationInfo::KEY_FILE_TEXT;
	else if (file_dlg.GetFileExt() == L"key") format = CApplicationInfo::KEY_FILE_BINARY;
	else {
		fn += L".key";
		format = CApplicationInfo::KEY_FILE_BINARY;
	}

	m_app_info->SaveMasterKey(fn, format);
	UpdateData(FALSE);
}


void CPropAppManager::OnLoadMasterKey()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	CFileDialog file_dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"Key Files (*.key)|*.key|Key Files (*.txt)|*.txt||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring fn = (const wchar_t*)(file_dlg.GetPathName()); 

	CApplicationInfo::KEY_FILE_FORMAT format;
	if (file_dlg.GetFileExt() == L"txt") format = CApplicationInfo::KEY_FILE_TEXT;
	else if (file_dlg.GetFileExt() == L"key") format = CApplicationInfo::KEY_FILE_BINARY;
	else {		format = CApplicationInfo::KEY_FILE_BINARY;	}
	m_app_info->LoadMasterKey(fn, format);
	m_master_key_finger = m_app_info->GetMasterFinger();

	UpdateData(FALSE);
}


void CPropAppManager::OnUpdateSnPassword()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	m_app_info->SetSnPassword(m_password);
	m_password_finger = m_app_info->GetSnPasswordFinger();
	UpdateData(FALSE);
}


void CPropAppManager::OnSaveAppInfo()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"config file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring wstr_fn = (const wchar_t*)(file_dlg.GetPathName());
	if (file_dlg.GetFileExt().IsEmpty()) wstr_fn += L".xml";
	m_app_info->SaveAppInfo(wstr_fn);
}


void CPropAppManager::OnLoadAppInfo()
{
	JCASSERT(m_app_info);
	CFileDialog file_dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"config file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;

	m_app_info->LoadAppInfo((const wchar_t*)(file_dlg.GetPathName()));

	m_string_masterid = m_app_info->GetMasterName();
	m_str_uuid_masterid = boost::uuids::to_string(m_app_info->GetMasterUuid());
	m_str_id_masterid.Format(L"%08X", m_app_info->GetMasterId());
	m_string_appid = m_app_info->GetAppName();
	m_str_uuid_appid = boost::uuids::to_string(m_app_info->GetAppUuid());
	m_str_id_appid.Format(L"%08X", m_app_info->GetAppId());

	m_master_key_finger = m_app_info->GetMasterFinger();
	m_password = m_app_info->GetPassword();
	m_password_finger = m_app_info->GetSnPasswordFinger();

	UpdateData(FALSE);
}


void CPropAppManager::OnBnClickedGenDelivery()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	m_app_info->GenerateDelivery(m_start_sn, m_delivery_count);
	RefreshDeliveryList();
}


BOOL CPropAppManager::OnInitDialog()
{
	CMFCPropertyPage::OnInitDialog();

	// TODO:  在此添加额外的初始化
	// 初始化 list
	LONG lStyle = GetWindowLong(m_delivery_list.GetSafeHwnd(), GWL_STYLE);//获取当前窗口style
	lStyle &= ~LVS_TYPEMASK; //清除显示方式位
	lStyle |= LVS_REPORT; //设置style
	SetWindowLong(m_delivery_list.GetSafeHwnd(), GWL_STYLE, lStyle);//设置style

	DWORD dwStyle = m_delivery_list.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;//选中某行使整行高亮（只适用与report风格的listctrl）
	dwStyle |= LVS_EX_GRIDLINES;//网格线（只适用与report风格的listctrl）
//	dwStyle |= LVS_EX_CHECKBOXES;//item前生成checkbox控件
	m_delivery_list.SetExtendedStyle(dwStyle); //设置扩展风格

	m_delivery_list.InsertColumn(0, L"Serial No", 0, 60);
	m_delivery_list.InsertColumn(1, L"App Id", 0, 100);
	m_delivery_list.InsertColumn(2, L"Delivery Key", 0, 380);
	m_delivery_list.InsertColumn(3, L"Registered", 0, 40);
	m_delivery_list.InsertColumn(4, L"Register Date", 0, 150);
	m_delivery_list.InsertColumn(5, L"Computer Name", 0, 200);
	m_delivery_list.InsertColumn(6, L"User Name", 0, 200);
	m_delivery_list.InsertColumn(7, L"Rebount count", 0, 20);
//	m_delivery_list.InsertColumn(5, L"System Id");


	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

void CPropAppManager::RefreshDeliveryList(void)
{
	JCASSERT(m_app_info);
	m_delivery_list.DeleteAllItems();
	m_app_info->ResetCursor();
	int index = 0;
	UINT mask = LVIF_TEXT | LVIF_PARAM /*| LVIF_COLUMNS*/;
	while (1)
	{
		CString str;
		CDeliveryInfo delivery;
		bool br =m_app_info->NextDeliveryItem(delivery);
		if (!br || !delivery.IsAvailable()) break;

//		if (delivery == NULL) break;
		DWORD sn = delivery.GetSerialNumber();
		str.Format(L"%04d", sn);
		int row = m_delivery_list.InsertItem(mask, index, str, 0, 0, 0, sn);
		DWORD app_id = delivery.GetAppId();
		str.Format(L"%08X", app_id);

		m_delivery_list.SetItemText(row, 1, str);
		std::string str_delivery = delivery.GetDeliveryKey();
		str.Format(L"%S", str_delivery.c_str());
		m_delivery_list.SetItemText(row, 2, str);
		bool reg = delivery.IsRegistered();
		str = reg ? L"R" : L"N";
		m_delivery_list.SetItemText(row, 3, str);

		if (reg)
		{
			const boost::gregorian::date& dd = delivery.GetRegisterDate();
			str = boost::gregorian::to_iso_extended_string(dd).c_str();
			m_delivery_list.SetItemText(row, 4, str);
		}
		m_delivery_list.SetItemText(row, 5, delivery.GetComputerName().c_str());

		m_delivery_list.SetItemText(row, 6, delivery.GetUserName().c_str());
		m_delivery_list.SetItemText(row, 7, std::to_wstring(delivery.GetReboundCount()).c_str());
		index++;
	}
}

void CPropAppManager::OnExporDelivery()
{
	JCASSERT(m_app_info);
	for (size_t ii = 0; ii < 5; ++ii)
	{
		int width = m_delivery_list.GetColumnWidth(ii);
		LOG_DEBUG(L"column[%d], width = %d", ii, width);
	}

	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"csv file (*.csv)|*.csv||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring wstr_fn = (const wchar_t*)(file_dlg.GetPathName());
	if (file_dlg.GetFileExt().IsEmpty()) wstr_fn += L".csv";
	m_app_info->ExportDelivery(wstr_fn);
}


void CPropAppManager::OnBnClickedRefreshDelivery()
{
	RefreshDeliveryList();
}


void CPropAppManager::OnBnClickedExportPublickey()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"Key Files (*.key)|*.key|Key Files (*.txt)|*.txt||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring fn = (const wchar_t*)(file_dlg.GetPathName());

	CApplicationInfo::KEY_FILE_FORMAT format;
	if (file_dlg.GetFileExt() == L"txt") format = CApplicationInfo::KEY_FILE_TEXT;
	else if (file_dlg.GetFileExt() == L"key") format = CApplicationInfo::KEY_FILE_BINARY;
	else {
		fn += L".key";
		format = CApplicationInfo::KEY_FILE_BINARY;
	}

	m_app_info->ExportPublicKey(fn, format);
	UpdateData(FALSE);
}


void CPropAppManager::OnSaveDelivery()
{
	JCASSERT(m_app_info);
	UpdateData(TRUE);
	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"delivery file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;
	std::wstring wstr_fn = (const wchar_t*)(file_dlg.GetPathName());
	if (file_dlg.GetFileExt().IsEmpty()) wstr_fn += L".xml";
	m_app_info->SaveDelivery(wstr_fn);
}


void CPropAppManager::OnLoadDelivery()
{
	JCASSERT(m_app_info);
	CFileDialog file_dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"delivery file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;
	m_app_info->LoadDelivery((const wchar_t*)(file_dlg.GetPathName()));

	m_string_masterid = m_app_info->GetMasterName();
	m_str_uuid_masterid = boost::uuids::to_string(m_app_info->GetMasterUuid());
	m_str_id_masterid.Format(L"%08X", m_app_info->GetMasterId());
	m_string_appid = m_app_info->GetAppName();
	m_str_uuid_appid = boost::uuids::to_string(m_app_info->GetAppUuid());
	m_str_id_appid.Format(L"%08X", m_app_info->GetAppId());

	m_master_key_finger = m_app_info->GetMasterFinger();
	m_password = m_app_info->GetPassword();
	m_password_finger = m_app_info->GetSnPasswordFinger();

	RefreshDeliveryList();

	UpdateData(FALSE);
}


// AuthorGenDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"

#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>


#include "AuthorGen.h"
#include "AuthorGenDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif



#include <cryptopp/hex.h>
#include <cryptopp/crc.h>
#include <cryptopp/base32.h>
#include <cryptopp/base64.h>
#include <cryptopp/aes.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>
#include <cryptopp/files.h>

#include <sqlite3.h>

LOCAL_LOGGER_ENABLE(L"authorgen", LOGGER_LEVEL_DEBUGINFO);



using namespace CryptoPP;


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAuthorGenDlg 对话框



CAuthorGenDlg::CAuthorGenDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AUTHORGEN_DIALOG, pParent)
	//, m_str_id_appid(_T(""))
	, m_start_sn(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void _DDX_Text(CDataExchange* pDX, int id, std::string& str)
{
	std::wstring wstr;
	CString cstr;
	if (pDX->m_bSaveAndValidate)
	{
		DDX_Text(pDX, id, cstr);
		wstr = (const wchar_t*)cstr;
		jcvos::UnicodeToUtf8(str, wstr);
	}
	else
	{
		jcvos::Utf8ToUnicode(wstr, str);
		cstr = wstr.c_str();
		DDX_Text(pDX, id, cstr);
	}
}

void _DDX_Text(CDataExchange* pDX, int id, std::wstring& wstr)
{
	CString cstr;
	if (pDX->m_bSaveAndValidate)
	{
		DDX_Text(pDX, id, cstr);
		wstr = (const wchar_t*)cstr;
	}
	else
	{
		cstr = wstr.c_str();
		DDX_Text(pDX, id, cstr);
	}
}
void CAuthorGenDlg::DoDataExchange(CDataExchange* pDX)
{
	CString str;
	std::wstring wstr;
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TABS, m_tabs);
}

BEGIN_MESSAGE_MAP(CAuthorGenDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_AUTHORIZE, &CAuthorGenDlg::OnClickedAuthorize)
	ON_BN_CLICKED(IDC_RSAKEY_GEN, &CAuthorGenDlg::OnClickedRsaKeyGen)
	ON_BN_CLICKED(IDC_RSAKEY_LOAD, &CAuthorGenDlg::OnClickedRsaKeyLoad)
	ON_BN_CLICKED(IDC_VERIFY_AUTHOR, &CAuthorGenDlg::OnClickedVerifyAuthor)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TABS, &CAuthorGenDlg::OnTcnSelchangeTabs)
END_MESSAGE_MAP()


// CAuthorGenDlg 消息处理程序

BOOL CAuthorGenDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	// 初始化属性页
	memset(m_property_pages, 0, sizeof(CMFCPropertyPage*) * MAX_PROPERTY_PAGE);


	m_tabs.InsertItem(0, L"AppManager");
	//m_page1.Create(MAKEINTRESOURCE(IDD_PROPPAGE_APPMANAGER), &m_tabs);
	m_page1.m_app_info = &m_app_info;
	m_property_pages[0] = &m_page1;
	m_property_pages[0]->Create(MAKEINTRESOURCE(IDD_PROPPAGE_APPMANAGER), &m_tabs);


	m_tabs.InsertItem(1, L"LicenseManager");
	m_page2.m_app_info = &m_app_info;
	m_property_pages[1] = &m_page2;
	m_property_pages[1]->Create(MAKEINTRESOURCE(IDD_PROP_LICENSE), &m_tabs);

	m_tabs.InsertItem(2, L"Authorize");
	m_page3.m_app_info = &m_app_info;
	m_property_pages[2] = &m_page3;
	m_property_pages[2]->Create(MAKEINTRESOURCE(IDD_PROP_AUTHORIZE), &m_tabs);

//	m_tabs.InsertItem(3, L"Verify");


	CRect rect = { 0 };
	CRect r1 = { 0 };
	CRect rw = { 0 };
	m_tabs.GetClientRect(&rect);
	m_tabs.GetItemRect(0, &r1);
	rect.top += r1.Height();
	for (size_t ii = 0; ii < MAX_PROPERTY_PAGE; ++ii)
	{
		if (!m_property_pages[ii]) continue;
//		m_property_pages[ii]->Create(MAKEINTRESOURCE(IDD_PROPPAGE_APPMANAGER), &m_tabs);
		m_property_pages[ii]->MoveWindow(&rect);
//		m_tabs.AddPage(m_property_pages[ii]);
	}

	m_property_pages[0]->ShowWindow(SW_SHOW);
	m_property_pages[0]->EnableWindow(TRUE);

//	size_t ss = sizeof(Authority::DELIVERY_LICENSE);


	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CAuthorGenDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CAuthorGenDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CAuthorGenDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CAuthorGenDlg::OnClickedSerialNoGen()
{
#if 0
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	BYTE sw_serial_number[16];
	memset(sw_serial_number, 0, 16);

	BYTE encrypt_sn[16];
	memset(encrypt_sn, 0, 16);

	BYTE* buf = sw_serial_number;
	memcpy_s(buf, 16, &m_id_softid, sizeof(m_id_softid));
	buf += sizeof(m_id_softid);
	memcpy_s(buf, 12, &m_start_sn, sizeof(m_start_sn));
	buf += sizeof(m_start_sn);

	CryptoPP::CRC32 crc;
	crc.Update(sw_serial_number, 8);
	crc.TruncatedFinal(buf, 4);
	buf += 4;

//	srand(time(0));
//	for (size_t ii = 0; ii < 4; ++ii) buf[ii] = (BYTE)(rand() & 0xFF);


	BYTE iv[CryptoPP::AES::BLOCKSIZE];
	memset(iv, 0, CryptoPP::AES::BLOCKSIZE);
	CryptoPP::AES::Encryption encryption(m_keysn, CryptoPP::AES::DEFAULT_KEYLENGTH);
	CryptoPP::CBC_Mode_ExternalCipher::Encryption cbc_encryption(encryption, iv);
	CryptoPP::StreamTransformationFilter encryptor(cbc_encryption, new CryptoPP::ArraySink(encrypt_sn, 16));
	encryptor.Put(sw_serial_number, 16);
	encryptor.MessageEnd();

//	std::string output;
	m_string_sn.clear();
	CryptoPP::BufferedTransformation* sink = new StringSink(m_string_sn);
	CryptoPP::Base32Encoder encoder(sink, true, 5, "-");
	encoder.Put(encrypt_sn, 16);
	encoder.MessageEnd();

	UpdateData(FALSE);
#endif
}


void CAuthorGenDlg::OnClickedSetKeySn()
{
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	SetSnKey(m_string_keysn);
	UpdateData(FALSE);
}


void CAuthorGenDlg::OnClickedAuthorize()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);

	CLicense license;
//	license.m_server_id = server_id;
	license.m_master_id = m_id_masterid;
	license.m_software_id = m_id_softid;
	license.m_serial_number = m_start_sn;
	license.m_system_id = 0;
	license.m_device_id = 0;
	license.m_period = 7 * 24 * 3600;	
	license.m_crc = 0;

	LOG_DEBUG(L"sizeof CLicense = %zd", sizeof(CLicense));

	DWORD local_crc;
	CryptoPP::CRC32 crc;
	crc.Update((BYTE*)(&license), sizeof(license));
	crc.TruncatedFinal((BYTE*)(&local_crc), sizeof(local_crc));
	license.m_crc = local_crc;


//	m_server_private_key.
	AutoSeededRandomPool rng;
	jcvos::auto_array<BYTE> out_buf(4096, 0);
	jcvos::auto_array<BYTE> in_buf(4096, 0);

	// 编码 = Decryptor
	//RSAES_OAEP_SHA_Decryptor decryptor(m_server_private_key);
	//PK_DecryptorFilter* df = new PK_DecryptorFilter(rng, decryptor, new ArraySink(out_buf, 4096));
	//RSASSA_PKCS1v15_SHA_Signer signer(m_server_private_key);
	//SignerFilter* df = new SignerFilter(rng, signer, new ArraySink(out_buf, 4096));


//	size_t fixed_len = decryptor.FixedCiphertextLength();
	size_t fixed_len = 128;
	LOG_DEBUG(L"size of decryption = %zd", fixed_len);


	// signe
	memcpy_s((BYTE*)in_buf, 4096, &license, sizeof(license));
	BYTE* buf = in_buf;


	Integer x = m_server_private_key.CalculateInverse(rng, Integer(in_buf, fixed_len));
	x.Encode(out_buf, fixed_len);



//	size_t written = 0;
////	size_t remain = sizeof(license);
//	while (written < sizeof(license) )
//	{
////		size_t len = min(remain, fixed_len);
//		ArraySource source(buf, fixed_len, true, df);
//		buf += fixed_len;
//		written += fixed_len;
////		remain -= len;
//	}


	// 转换成BASE 64;
	m_authorization_value.clear();
	StringSink* sink = new StringSink(m_authorization_value);
	Base64Encoder* encoder = new Base64Encoder(sink);
	encoder->Put(out_buf, fixed_len);
	encoder->MessageEnd();
	delete encoder;
	//	delete finger_sink;
	//delete sink;

	UpdateData(FALSE);

}


void CAuthorGenDlg::OnClickedVerifyAuthor()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);

	jcvos::auto_array<BYTE> cipher_buf(1024, 0);
	// Decode Base64
	Base64Decoder decoder(new ArraySink(cipher_buf, 1024));
	decoder.Put((BYTE*)m_authorization_value.c_str(), m_authorization_value.size());
	decoder.MessageEnd();

	// 解码
	jcvos::auto_array<BYTE> plain_buf(1024, 0);

	AutoSeededRandomPool rng;
	Integer x = m_server_public_key.ApplyRandomizedFunction(rng, Integer(cipher_buf, 128));
	x.Encode(plain_buf, 128);

	// 解析
	CLicense license;
	memcpy_s(&license, sizeof(license), plain_buf, sizeof(license));
	// check crc
	DWORD crc0 = license.m_crc;
	license.m_crc = 0;
	DWORD crc1;
	CryptoPP::CRC32 crc;
	crc.Update((BYTE*)(&license), sizeof(license));
	crc.TruncatedFinal((BYTE*)(&crc1), sizeof(crc1));
	LOG_DEBUG(L"orignal CRC=%08X, calculated CRC=%08X", crc0, crc1);
	if (crc0 != crc1)
	{
		LOG_ERROR(L"[err] worng crc");
	}


	m_start_sn = license.m_serial_number;
	//m_str_id_masterid = 
	//m_str_id_appid.Format(L"%08X", license.m_software_id);
	//m_str_id_masterid.Format(L"%08X", license.m_master_id);
	UpdateData(FALSE);
}


void SaveData(const std::wstring& fn, const BufferedTransformation& bt)
{
	CryptoPP::FileSink file_sink(fn.c_str());
	bt.CopyTo(file_sink);
	file_sink.MessageEnd();
}

void RsaKeyToFinger(std::string& finger, const RSA::PrivateKey& key)
{
	finger.clear();
	jcvos::auto_array<BYTE> key_buf(1024, 0);
	CryptoPP::ArraySink * sink = new CryptoPP::ArraySink(key_buf, 1024);
	key.Save(*sink);
	SHA1 hash;
	byte digest[SHA1::DIGESTSIZE];
	hash.Update(key_buf, 1024);
	hash.Final(digest);

	StringSink * finger_sink = new StringSink(finger);
	HexEncoder * encoder = new HexEncoder(finger_sink, true, 4);
	encoder->Put(digest, SHA1::DIGESTSIZE);
	encoder->MessageEnd();
	delete encoder;
//	delete finger_sink;
	delete sink;
}

void PublicKeyToFinger(std::string& finger, const RSA::PublicKey& key)
{
	finger.clear();
	jcvos::auto_array<BYTE> key_buf(1024, 0);
	CryptoPP::ArraySink* sink = new CryptoPP::ArraySink(key_buf, 1024);
	key.Save(*sink);
	SHA1 hash;
	byte digest[SHA1::DIGESTSIZE];
	hash.Update(key_buf, 1024);
	hash.Final(digest);

	StringSink* finger_sink = new StringSink(finger);
	HexEncoder* encoder = new HexEncoder(finger_sink, true, 4);
	encoder->Put(digest, SHA1::DIGESTSIZE);
	encoder->MessageEnd();
	delete encoder;
	//	delete finger_sink;
	delete sink;
}


void PrivateKeyToString(std::string& str, const RSA::PrivateKey& key)
{
	jcvos::auto_array<BYTE> key_buf(1024, 00);
	ArraySink sink(key_buf, 1024);
	key.Save(sink);
	size_t key_size = 1024 - sink.AvailableSize();
	LOG_DEBUG(L"key size = %zd", key_size);

	//ByteQueue que;
	//key.Save(que);
	
//	StringSink str_sink(str);
	CryptoPP::Base64Encoder encoder(new StringSink(str));
	key_size = encoder.Put(key_buf, key_size);
	//que.CopyTo(encoder);
	encoder.MessageEnd();

	//.CopyTo(str_sink);
	//str_sink.MessageEnd();
}


void CAuthorGenDlg::OnClickedRsaKeyGen()
{
	/*
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	CryptoPP::AutoSeededRandomPool rng;
	size_t key_size = sizeof(CLicense) * 8;

	m_server_private_key.GenerateRandomWithKeySize(rng, 1024);

	SetPrivateKey(m_server_private_key);

	CFileDialog file(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"Key Files (*.key)|*.key;*.txt||");
	if (file.DoModal() != IDOK) return;
	std::wstring fn = (const wchar_t*)(file.GetFolderPath());
	fn += L"\\";
	fn += ((const wchar_t*)(file.GetFileTitle()));
	std::wstring public_fn = fn + L".public.key";
	std::wstring private_fn = fn + L".private.txt";

	// 二进制保存
	//ByteQueue public_queue;
	//m_server_public_key.Save(public_queue);
	//SaveData(public_fn, public_queue);

	// 保存私钥到文本文件
	ByteQueue private_queue;
	m_server_private_key.Save(private_queue);
	CryptoPP::Base64Encoder private_encoder;
	private_queue.CopyTo(private_encoder);
	private_encoder.MessageEnd();
	SaveData(private_fn, private_encoder);

	// 保存私钥到二进制文件
	private_fn = fn + L"private.key";
	ByteQueue private_queue1;
	m_server_private_key.Save(private_queue1);
	SaveData(private_fn, private_queue1);

	// for test
	jcvos::auto_array<BYTE> key_buf(1024, 00);
	ArraySink sink(key_buf, 1024);
	m_server_private_key.Save(sink);

	// 文本保存
	//PrivateKeyToString(m_str_private_key, m_server_private_key);

	// 比较key


	//FILE* pff = NULL;
	//_wfopen_s(&pff, public_fn.c_str(), L"w+");
	//fwrite(public_string.c_str(), public_string.size(), 1, pff);
	//fclose(pff);
	UpdateData(FALSE);
*/
}


void CAuthorGenDlg::OnClickedRsaKeyLoad()
{
/*
	UpdateData(TRUE);
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog file(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"Key Files (*.key)|*.key;*.txt||");
	if (file.DoModal() != IDOK) return;

	std::wstring fn = (const wchar_t*)(file.GetPathName());
	if (file.GetFileExt() == L"txt")
	{
		FileSource file(fn.c_str(), true);
		CryptoPP::Base64Decoder decoder;
		//		ByteQueue que;
		file.TransferTo(decoder);
		decoder.MessageEnd();
		ByteQueue que;
		decoder.CopyTo(que);
		m_server_private_key.Load(que);
	}
	else if (file.GetFileExt() == L"key")
	{
		FileSource file(fn.c_str(), true);
		ByteQueue que;
		file.TransferTo(que);
		que.MessageEnd();
		m_server_private_key.Load(que);
	}

	// for test
	jcvos::auto_array<BYTE> key_buf(1024, 00);
	ArraySink sink(key_buf, 1024);
	m_server_private_key.Save(sink);

	SetPrivateKey(m_server_private_key);
	//m_server_public_key = CryptoPP::RSA::PublicKey(m_server_private_key);
	//RsaKeyToFinger(m_rsakey_finger, m_server_private_key);
	//PrivateKeyToString(m_str_private_key, m_server_private_key);

	UpdateData(FALSE);
*/
}


void CAuthorGenDlg::OnClickedSaveConfig()
{
	UpdateData(FALSE);
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog file_dlg(FALSE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"config file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;

	boost::property_tree::ptree pt;

	pt.put("master_id", m_string_masterid);
	pt.put<std::string>("software_name", m_string_softid);
	pt.put<std::string>("serial_num_key", m_string_keysn);
	std::string str_private;
	PrivateKeyToString(str_private, m_server_private_key);
	pt.put<std::string>("server_key", str_private);

	boost::property_tree::ptree root_pt;
	root_pt.put_child("software_info", pt);

	std::wstring wstr_fn = (const wchar_t*)(file_dlg.GetPathName());
	if (file_dlg.GetFileExt().IsEmpty()) wstr_fn += L".xml";
	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, wstr_fn);
	boost::property_tree::write_xml(str_fn, root_pt);
}


void CAuthorGenDlg::OnBnClickedLoadConfig()
{
	UpdateData(FALSE);
	// TODO: 在此添加控件通知处理程序代码
	CFileDialog file_dlg(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		L"config file (*.xml)|*.xml||");
	if (file_dlg.DoModal() != IDOK) return;


	std::wstring wstr_fn = (const wchar_t*)(file_dlg.GetPathName());
	std::string str_fn;
	jcvos::UnicodeToUtf8(str_fn, wstr_fn);

	boost::property_tree::ptree root_pt;
	boost::property_tree::read_xml(str_fn, root_pt);

	auto p0 = root_pt.get_optional<std::string>("software_info.master_id");
	if (p0)
	{
		m_string_masterid = *p0;
		//SetStringId(m_string_masterid, m_uuid_masterid, m_id_masterid, m_str_uuid_masterid, m_str_id_masterid);
	}
	auto p1 = root_pt.get_optional<std::string>("software_info.software_name");
	if (p1)
	{
		m_string_softid = *p1;
		//SetStringId(m_string_softid, m_uuid_appid, m_id_softid, m_str_uuid_appid, m_str_id_appid);
	}

	auto p2 = root_pt.get_optional<std::string>("software_info.serial_num_key");
	if (p2)
	{
		m_string_keysn = *p2;
		SetSnKey(m_string_keysn);
	}

	auto p3 = root_pt.get_optional<std::string>("software_info.server_key");
	if (p3)
	{
		std::string str_key = *p3;
		// string to 
		//jcvos::auto_array<BYTE> key_buf(1024, 0);
		//// Decode Base64
		//size_t key_size;
		//ArraySink* sink = new ArraySink(key_buf, 1024);
		//Base64Decoder decoder(sink);
		//key_size = decoder.Put((BYTE*)str_key.c_str(), str_key.size());
		//decoder.MessageEnd();
		//LOG_DEBUG(L"decoded size = %zd", key_size);

		//key_size = 1024 - sink->AvailableSize();
		//LOG_DEBUG(L"loaded size = %zd", key_size);

		//ArraySink sink2(key_buf, 1024);
		//m_server_private_key.Load(sink2);

		StringSource source(str_key, true);
		CryptoPP::Base64Decoder decoder;
		source.TransferTo(decoder);
		decoder.MessageEnd();

		ByteQueue que;
		decoder.CopyTo(que);
		m_server_private_key.Load(que);
		SetPrivateKey(m_server_private_key);

		// for test
		//jcvos::auto_array<BYTE> key_buf(1024, 00);
		//ArraySink sink(key_buf, 1024);
		//m_server_private_key.Save(sink);
		//sink.AvailableSize();
	}
	UpdateData(FALSE);
}



void CAuthorGenDlg::SetStringId(const std::string& string_id, boost::uuids::uuid& uuid, DWORD& id, std::string& str_uuid, CString& str_dword)
{
	boost::uuids::string_generator str_gen;

	boost::uuids::uuid dns_namespace_uuid = str_gen("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}");
	boost::uuids::name_generator gen(dns_namespace_uuid);
	uuid = gen(string_id);
	id = LODWORD(boost::uuids::hash_value(uuid));

	//	HexEncoder encoder;
	str_uuid = boost::uuids::to_string(uuid);
	str_dword.Format(L"%08X", id);
}

void CAuthorGenDlg::SetSnKey(const std::string& string_snkey)
{
	SHA1 hash;
	hash.Update((byte*)string_snkey.c_str(), string_snkey.size());
	hash.Final(m_keysn);

	m_str_sha_keysn.clear();
	CryptoPP::BufferedTransformation* sink = new StringSink(m_str_sha_keysn);
	CryptoPP::HexEncoder encoder(sink, true, 8, "-");
	encoder.Put(m_keysn, 16);
	encoder.MessageEnd();
}

void CAuthorGenDlg::SetPrivateKey(const RSA::PrivateKey& key)
{
	// 生成Public Key
	m_server_public_key = CryptoPP::RSA::PublicKey(key);
	PublicKeyToFinger(m_rsakey_finger, m_server_public_key);

}



void CAuthorGenDlg::OnTcnSelchangeTabs(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	int sel = m_tabs.GetCurSel();
	for (size_t ii = 0; ii < MAX_PROPERTY_PAGE; ++ii)
	{
		if (m_property_pages[ii])
		{
			if (ii == sel)
			{
				m_property_pages[ii]->ShowWindow(SW_SHOW);
				m_property_pages[ii]->EnableWindow(TRUE);
			}
			else
			{
				m_property_pages[ii]->ShowWindow(SW_HIDE);
				m_property_pages[ii]->EnableWindow(FALSE);
			}
		}
	}
}



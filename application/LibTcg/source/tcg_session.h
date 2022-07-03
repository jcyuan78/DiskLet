///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../include/itcg.h"

#include "dta_command.h"
#include "dta_response.h"
#include "../include/l0discovery.h"

class CTcgSession : public tcg::ITcgSession
{
public:
    CTcgSession(void);
    ~CTcgSession(void);

public:
    bool ConnectDevice(IStorageDevice * dev);
// essential methods
	virtual UINT64 GetState(bool reload);
	virtual bool GetProtocol(BYTE* buf, size_t buf_len);

	virtual bool GetFeatures(tcg::ISecurityObject*& feature, bool force);

    virtual bool L0Discovery(BYTE* buf);
	virtual BYTE Properties(boost::property_tree::wptree& res, const std::vector<std::wstring>& req);

    virtual BYTE StartSession(const TCG_UID sp, const char* host_challenge, const TCG_UID sign_authority, 
		bool write= true);
	virtual BYTE EndSession(void);
	virtual bool IsOpen(void) { return m_is_open; }


	BYTE GetTable(tcg::ISecurityObject*& res, const TCG_UID table, WORD start_col, WORD end_col);
    BYTE GetTable(DtaResponse & response, const TCG_UID table, WORD start_col, WORD end_col);
	virtual void GetTable(boost::property_tree::wptree& res, const TCG_UID table, WORD start_col, WORD end_col);
	//	virtual BYTE SetTable(const TCG_UID table, OPAL_TOKEN name, vector<BYTE>& value);
	virtual BYTE SetTable(tcg::ISecurityObject*& res, const TCG_UID table, int name, const char* value);
	/// <summary> 用于设置Table，忽略where参数，以column </summary>
	/// <param name="res"></param>
	/// <param name="table"></param>
	/// <param name="name">column id</param>
	/// <param name="val"></param>
	/// <returns></returns>
	virtual BYTE SetTable(tcg::ISecurityObject*& res, const TCG_UID table, int name, int val);
	/// <summary> SetTable的一般形式 </summary>
	/// <param name="res">[OUT]返回结果</param>
	/// <param name="table">[IN]需要设置的table id</param>
	/// <param name="_where">[IN, OPT]根据TCG定义的Where参数</param>
	/// <param name="val">[IN, OPT]根据TCG定义的val参数</param>
	/// <returns></returns>
	virtual BYTE SetTableBase(tcg::ISecurityObject*& res, const TCG_UID table, CTcgTokenBase* _where, 
		CTcgTokenBase* value);
//	BYTE SetTable(const TCG_UID table, int name, const char * value);

	virtual BYTE Activate(tcg::ISecurityObject*& res, const TCG_UID obj);
	virtual BYTE Revert(tcg::ISecurityObject*& res, const TCG_UID sp);
	virtual BYTE GenKey(tcg::ISecurityObject*& res, const TCG_UID obj, UINT public_exp, UINT pin_length);

	virtual BYTE TperReset(void);

	virtual void Reset(void);
	virtual BYTE BlockSID(BYTE clear_event, BYTE freeze);

// others: non TPer related function
	virtual void GetLockingRangeUid(TCG_UID uuid, UINT id);


// hi-level features (with session open)
    virtual BYTE GetDefaultPassword(std::string& password);

	BYTE SetSIDPassword(const char* old_pw, const char* new_pw);
	//void SetPassword(UINT user_id, bool admin, const char* new_pw);

	/// <summary> 设置authority的密码 </summary>
	/// <param name="authority">如果是authority的UID，自动转化为C_PIN的UID </param>
	/// <param name="new_pwd">需要设置的password</param>
	virtual void SetPassword(const TCG_UID authority, const char* new_pwd);

	virtual BYTE RevertTPer(const char* password, const TCG_UID authority, const TCG_UID sp);


	virtual BYTE SetLockingRange(UINT range_id, UINT64 start, UINT64 length);
	virtual BYTE WriteShadowMBR(jcvos::IBinaryBuffer * buf);

	virtual void WriteDataTable(const TCG_UID tab_id, const BYTE * buf, size_t buf_len);
	virtual size_t ReadDataTable(const TCG_UID, BYTE* buf, size_t offset, size_t len);

	virtual void SetACE(const TCG_UID obj, const std::vector<const BYTE *> & authorities);

	virtual void AssignRangeToUser(UINT range_id, UINT user_id, bool keep_admin);
	virtual void GetLockingInfo(tcg::LOCKING_INFO & info);

protected:
	// 返回TCG receive的state code
    int InvokeMethod(DtaCommand& cmd, DtaResponse& response);
    bool IsEnterprise(void) { return false; }
	// 返回TCG receive 的state code，小于0，表示Scsi command执行的错误代码，>0表示TCG receive的state code
    int ExecSecureCommand(const DtaCommand& cmd, DtaResponse& response, BYTE protocol);
	virtual WORD GetComId(void) { return m_base_comid; }

    // 把passwoad打包成一个token
    void PackagePasswordToken(vector<BYTE>& hash, const char* password);
    BYTE Authenticate(vector<uint8_t> Authority, const char* Challenge);
	void Abort(void);
	/// <summary> 生成LockingSP的Admin或者User的UID </summary>
	/// <param name="uid">[OUT]生成的UID</param>
	/// <param name="admin_user">[IN]true: admin, false: user</param>
	/// <param name="id"></param>
	void GetAuthorityUid(TCG_UID & uid, UINT tab,  bool admin_user, UINT id);
	void UpdateFeatureState(void);

protected:
	CTcgFeatureSet * m_feature_set = nullptr;

    IStorageDevice * m_dev;
    bool m_hash_password;
    UINT32 m_tsn, m_hsn;    // TPer Session Number and Host Session Number
    BYTE m_session_auth;
	WORD m_base_comid;

	bool m_datastore_support;
	UINT32 m_datastore_alignment;

	bool m_will_abort;

	// session 是否打开
	bool m_is_open;

	/// <summary>更具定义，00表示未设置SID密码，FF表示已经设置SID密码 </summary>
	BYTE m_sid_pin_init = 0;
	bool m_lock_enabled = false;
	bool m_locked = false;

	boost::property_tree::wptree m_tper_property;
	UINT32 m_tperMaxPacket=0, m_tperMaxToken=0;

	tcg::ISecurityParser* m_parser=nullptr;

	tcg::SSC_TYPE m_ssc_type=tcg::SSC_UNKNOWN;

//#ifdef LOG_BINARY
//public:
//protected:
//#endif
protected:
	//命令计数，用于保存调试数据
	UINT32 m_invoking_id;
	std::wostream* m_log_out = nullptr;
public:
	virtual void SetLogFile(const std::wstring& fn);
};

//void DtaHashPwd(vector<uint8_t>& hash, char* password, DtaDev* d);
void CreateTcgSession(tcg::ITcgSession*& session, IStorageDevice* dev);


class CTestDevice : public IStorageDevice
{
public:
	virtual BYTE Inquiry(BYTE* buf, size_t buf_len, BYTE evpd, BYTE page_code) { return 0; }

	virtual bool Inquiry(IDENTIFY_DEVICE& id) { return false; }
//	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO& info, boost::property_tree::wptree* ext_info) { return false; }
	virtual STORAGE_HEALTH_STATUS GetHealthInfo(DEVICE_HEALTH_INFO& info, std::vector<STORAGE_HEALTH_INFO_ITEM>& ext_info) { return STATUS_ERROR; }

	// rd_wr: Read (true) or Write (false)
	// Sector Read / Write通过Interface转用方法实现
	virtual bool SectorRead(BYTE* buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE* buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }

	// Read/Write方法通过系统Read/Write函数实现，
	virtual bool Read(void* buf, FILESIZE lba, size_t secs) { return false; }

	virtual bool Write(const void* buf, FILESIZE lba, size_t secs) { return false; }

	virtual void FlushCache() {}

	// 用于性能测试，单位us。
	virtual UINT GetLastInvokeTime(void) { return 0; }
	// Device容量，单位:sector
	virtual FILESIZE GetCapacity(void) { return 0; }
	virtual bool DeviceLock(void) { return false; }
	virtual bool DeviceUnlock(void) { return false; }

	// time out in second
	virtual BYTE ScsiCommand(READWRITE rd_wr, BYTE* buf, size_t length,
		_In_ const BYTE* cb, size_t cb_length,		// input of command block
		_Out_ BYTE* sense, size_t sense_len,		// output of sense buffer
		UINT timeout) {
		return false;
	}

	virtual BYTE SecurityReceive(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid);
	virtual BYTE SecuritySend(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid);

	virtual BYTE DownloadFirmware(BYTE* buf, size_t buf_len, size_t block_size, DWORD slot, bool activate) { return 0; }


public:
	std::wstring m_path;
};


#pragma once
#include "../include/itcg.h"

#include "dta_command.h"
#include "dta_response.h"
#include "../include/l0discovery.h"


#define PROTOCOL_ID_TCG (0x01)
#define PROTOCOL_ID_TPER (0x02)

class CTcgSession : public ITcgSession
{
public:
    CTcgSession(void);
    ~CTcgSession(void);

public:
    bool ConnectDevice(IStorageDevice * dev);
// essential methods
    virtual BYTE L0Discovery(BYTE* buf);
    virtual BYTE StartSession(const TCG_UID sp, const char* host_challenge, const TCG_UID sign_authority, bool write= true);
	virtual BYTE EndSession(void);
	virtual bool IsOpen(void) { return m_is_open; }
    virtual BYTE GetTable(DtaResponse & response, const TCG_UID table, WORD start_col, WORD end_col);
//	virtual BYTE SetTable(const TCG_UID table, OPAL_TOKEN name, vector<BYTE>& value);
	virtual BYTE SetTable(const TCG_UID table, OPAL_TOKEN name, const char * value);
	virtual BYTE Revert(const TCG_UID sp);
	virtual void Reset(void);

// packed features
    virtual BYTE GetDefaultPassword(std::string& password);
	virtual BYTE SetSIDPassword(const char* old_pw, const char* new_pw);


protected:
    BYTE InvokeMethod(DtaCommand& cmd, DtaResponse& response);
    bool IsEnterprise(void) { return false; }
    BYTE ExecSecureCommand(const DtaCommand& cmd, DtaResponse& response, BYTE protocol);
	virtual WORD GetComId(void) { return m_base_comid; }

    // 把passwoad打包成一个token
    void PackagePasswordToken(vector<BYTE>& hash, const char* password);
    BYTE Authenticate(vector<uint8_t> Authority, const char* Challenge);

protected:
	CTcgFeatureSet m_feature_set;

    IStorageDevice * m_dev;
    bool m_hash_password;
    UINT32 m_tsn, m_hsn;    // TPer Session Number and Host Session Number
    BYTE m_session_auth;
	WORD m_base_comid;

	bool m_will_abort;

	// session 是否打开
	bool m_is_open;

#ifdef _DEBUG
	//命令计数，用于保存调试数据
	UINT32 m_invoking_id;
#endif
};

//void DtaHashPwd(vector<uint8_t>& hash, char* password, DtaDev* d);
void CreateTcgSession(ITcgSession*& session, IStorageDevice* dev);


class CTestDevice : public IStorageDevice
{
public:
	virtual bool Inquiry(IDENTIFY_DEVICE& id) { return false; }
	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO& info, boost::property_tree::wptree* ext_info) { return false; }
	// rd_wr: Read (true) or Write (false)
	// Sector Read / Write通过Interface转用方法实现
	virtual bool SectorRead(BYTE* buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE* buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }

	// Read/Write方法通过系统Read/Write函数实现，
	virtual bool Read(BYTE* buf, FILESIZE lba, size_t secs) { return false; }

	virtual bool Write(BYTE* buf, FILESIZE lba, size_t secs) { return false; }

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

public:
	std::wstring m_path;
};


#pragma once

#include "storage_device.h"
#pragma pack (1)	// 解除对齐
class CSmartAttribute
{
public:
	BYTE	m_id;
	WORD	m_flags;
	BYTE	m_val;
	BYTE	m_th;
	union
	{
		UINT64	m_raw_val;
		BYTE	m_raw[7];
	};
};
#pragma pack ()	



class CAtaDeviceComm : public CStorageDeviceComm
{
protected:
	CAtaDeviceComm(void) : m_max_lba(0), m_feature_lba48(false)
	{}
	virtual ~CAtaDeviceComm(void);

public:
	//	static const size_t	ID_TABLE_LEN;
	bool Detect(void);

public:
	virtual bool Inquiry(jcvos::IBinaryBuffer * & data);
	virtual bool Inquiry(IDENTIFY_DEVICE & id);

	bool GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree * ext_info);


	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t secs, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t secs, UINT timeout) { return false; }
	virtual void FlushCache() {}

	//virtual BYTE ReadPIO(FILESIZE lba, BYTE secs, BYTE &error, BYTE * buf);
	//virtual BYTE WritePIO(FILESIZE lba, size_t secs, BYTE &error, BYTE * buf);


	virtual WORD StandBy(WORD time) { return false; }
	//	virtual bool DownloadMicrocode(BYTE sub_cmd, bool dma, LPVOID buf, size_t len, size_t offset);
	virtual bool DeviceLock(void) { return false; }
	virtual bool DeviceUnlock(void) { return false; }
	//	virtual bool Dismount(void);
	virtual bool Read(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual bool Write(BYTE * buf, FILESIZE lba, size_t secs) { return false; }

	// Device容量，单位:sector
	virtual UINT GetLastInvokeTime(void) { return false; }
	virtual FILESIZE GetCapacity(void) { return m_max_lba; }


	//virtual BYTE ScsiCommand(IStorageDevice::READWRITE rd_wr, BYTE *buf, size_t length, 
	//	_In_ const BYTE *cb, size_t cb_length, // input of command block
	//	_Out_ BYTE *sense, size_t sense_len,		// output of sense buffer
	//	UINT timeout);


// ATA specific command
public:
	virtual bool AtaCommand(ATA_REGISTER &reg, ATA_PROTOCOL prot, BYTE * buf, size_t secs);
	virtual bool AtaCommand48(ATA_REGISTER &previous, ATA_REGISTER &current,
		ATA_PROTOCOL prot, BYTE * buf, size_t secs);
	virtual bool ReadSmart(BYTE * buf, size_t length);
	size_t SmartParser(CSmartAttribute * attri,	// [output] 调用者确保空间
		size_t attri_size, BYTE * buf, size_t length);

protected:
	bool AtaCommandNoData(ATA_REGISTER &reg);

protected:
	FILESIZE	m_max_lba;
	bool		m_feature_lba48;
	FILESIZE	m_capacity;		// capacitance in sector
	// 用于性能测试，单位us。
	LONGLONG	m_last_invoke_time;
	bool		m_locked;
};

class CAtaPassThroughDevice : public CAtaDeviceComm
{
protected:
	CAtaPassThroughDevice(void) {}
	virtual ~CAtaPassThroughDevice(void) {}

public:
	// 用于检测连接的device是否支持ata pass through
//	bool Detect(void);

public:
	virtual bool AtaCommand(ATA_REGISTER &reg, ATA_PROTOCOL prot, BYTE * buf, size_t secs);
	virtual bool AtaCommand48(ATA_REGISTER &previous, ATA_REGISTER &current,
		ATA_PROTOCOL prot, BYTE * buf, size_t secs);

	virtual LPCTSTR GetBusName(void) { return L"ATA_PASSTHROUGH"; };

	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t secs, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t secs, UINT timeout) { return false; }

};


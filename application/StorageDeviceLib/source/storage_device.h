#pragma once

#include "../include/istorage_device.h"

#define CDB6GENERIC_LENGTH	(6)
#define CDB10GENERIC_LENGTH	(16)


#pragma pack(push, sensedata, 1)
typedef struct _SENSE_DATA {
	UCHAR ErrorCode : 7;
	UCHAR Valid : 1;
	UCHAR SegmentNumber;
	UCHAR SenseKey : 4;
	UCHAR Reserved : 1;
	UCHAR IncorrectLength : 1;
	UCHAR EndOfMedia : 1;
	UCHAR FileMark : 1;
	UCHAR Information[4];
	UCHAR AdditionalSenseLength;
	UCHAR CommandSpecificInformation[4];
	UCHAR AdditionalSenseCode;
	UCHAR AdditionalSenseCodeQualifier;
	UCHAR FieldReplaceableUnitCode;
	UCHAR SenseKeySpecific[3];
} SENSE_DATA, * PSENSE_DATA;
#pragma pack(pop, sensedata)


class CStorageDeviceComm : virtual public IStorageDevice/*, virtual public ITcgDevice*/
{
protected:
	CStorageDeviceComm(void);
	virtual ~CStorageDeviceComm(void);

public:
	bool Connect(HANDLE dev, bool own);
//	virtual bool Inquiry(jcvos::IBinaryBuffer * & data) { return false; }

//	virtual bool Identify(boost::property_tree::wptree & prop);
	virtual bool Inquiry(IDENTIFY_DEVICE & id);
//	virtual bool ReadSmart(jcvos::IBinaryBuffer * & data) { return false; }

	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree * ext_info)
	{
		return false;
	}

	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }

	virtual bool Read(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual bool Write(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual void FlushCache() { return; }

	//virtual bool StartStopUnit(bool stop) = 0;

	// 用于性能测试，单位us。
	virtual UINT GetLastInvokeTime(void) { return 0; }
	// Device容量，单位:sector
	virtual FILESIZE GetCapacity(void) { return 0; }
	virtual bool DeviceLock(void) { return false; }
	virtual bool DeviceUnlock(void) { return false; }
	//virtual bool Dismount(void) = 0;

	//virtual void SetDeviceName(LPCTSTR name) = 0;
	//virtual LPCTSTR GetBusName(void) = 0;

	//virtual void UnmountAllLogical(void) = 0;
	// time out in second
	virtual BYTE ScsiCommand(READWRITE rd_wr, BYTE *buf, size_t length,
		_In_ const BYTE *cb, size_t cb_length,		// input of command block
		_Out_ BYTE *sense, size_t sense_len,		// output of sense buffer
		UINT timeout);

	virtual BYTE SecurityReceive(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid);
	virtual BYTE SecuritySend(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid);

	//virtual bool L0Discovery(BYTE* buf);


protected:
	HANDLE		m_hdev;


};


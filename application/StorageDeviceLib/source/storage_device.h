#pragma once

#include "../include/istorage_device.h"
#include "../include/idiskinfo.h"

#define CDB6GENERIC_LENGTH	(6)
#define CDB10GENERIC_LENGTH	(16)

enum HEALTH_ITEM_ID
{
	HEALTH_ITEM_UNKNOWN = 0,
	POWER_CYCLE = 1, POWER_ON_HOURS = 2, UNSAFE_SHUTDOWN = 3,
	HOST_READ_MB = 4, HOST_WRITE_MB = 5,  MEDIA_READ_MB = 6, MEDIA_WRITE_MB = 7,
	ERROR_COUNT = 8, INITIAL_BAD_BLOCK = 9, RUNTIME_BAD_BLOCK = 10, VALID_SPARE_BLOCK = 11,
	AVERAGE_ERASE_COUNT = 12, TOTAL_ERASE_COUNT = 13, MAX_ERASE_COUNT = 14, MIN_ERASE_COUNT = 15,
	CURRENT_TEMPERATURE = 16, MAX_TEMPERATURE = 17, REMAIN_LIFE = 18,
};

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
	bool Connect(HANDLE dev, bool own, const std::wstring & dev_id, DISK_PROPERTY::BusType bus_type);
	//virtual bool Detect(void) { return false; }
	virtual bool Inquiry(IDENTIFY_DEVICE & id);
	virtual BYTE Inquiry(BYTE* buf, size_t buf_len, BYTE evpd, BYTE page_code);

//	virtual bool ReadSmart(jcvos::IBinaryBuffer * & data) { return false; }

	virtual STORAGE_HEALTH_STATUS GetHealthInfo(DEVICE_HEALTH_INFO& info, HEALTH_INFO_LIST& ext_info);

	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }

	virtual bool Read(void* buf, FILESIZE lba, size_t secs);
	virtual bool Write(const void* buf, FILESIZE lba, size_t secs);
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

	virtual BYTE DownloadFirmware(BYTE* buf, size_t buf_len, size_t block_size, DWORD slot, bool activate);
	//virtual BYTE GetFeature(void) = 0;
	//virtual BYTE SetFeature() = 0;
	//virtual BYTE GetLogPage(BYTE * buf, size_t buf_len, BYTE LID, DWORD NUMD, UINT64 offset, DWORD param);


protected:
	// 用于派生类检测链接的device是否合适
	virtual bool OnConnectDevice(void);

	//virtual bool L0Discovery(BYTE* buf);


protected:
	HANDLE		m_hdev;
	std::wstring m_dev_id;
	DISK_PROPERTY::BusType m_bus_type;
};


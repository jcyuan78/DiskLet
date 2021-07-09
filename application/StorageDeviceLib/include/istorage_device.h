#pragma once

#include <stdext.h>
#include <jcparam.h>
#include <nvme.h>
#include <boost/property_tree/ptree.hpp>

static const UINT32 SPT_SENSE_LENGT = 24;
//#define SPT_SENSE_LENGT (32)

// 绝对0度。部分设备的温度用开尔文表示，用于转换成摄氏度
#define ABSOLUTE_ZERO	(-273)


typedef struct _ATA_REGISTER
{
	union
	{
		BYTE feature;
		BYTE error;
	};
	BYTE sec_count;
	BYTE lba_low;
	BYTE lba_mid;
	BYTE lba_hi;
	BYTE device;
	union
	{
		BYTE command;
		BYTE status;
	};
	BYTE dummy;

	void set_lba(DWORD LBA)
	{
		lba_low = LOBYTE(LOWORD(LBA));
		lba_mid = HIBYTE(LOWORD(LBA));
		lba_hi = LOBYTE(HIWORD(LBA));
		device = 0x40 | (HIBYTE(HIWORD(LBA)) & 0x0F);
	}
	DWORD get_lba(void)
	{
		DWORD lba = MAKELONG(MAKEWORD(lba_low, lba_mid), MAKEWORD(lba_hi, device & 0x0F));
		return lba;
	}

} ATA_REGISTER;

class IDENTIFY_DEVICE
{
public:
	std::wstring m_model_name;
	std::wstring m_serial_num;
	std::wstring m_firmware;
	std::wstring m_vendor;
	BYTE m_dev_type;
	STORAGE_BUS_TYPE m_bus_type;
};


class DEVICE_HEALTH_INFO
{
public:
	UINT64 m_power_cycle;
	UINT64 m_power_on_hours;
	UINT64 m_unsafe_shutdowns;
	UINT64 m_host_read;			// in MB unit
	UINT64 m_host_write;		// in MB unit
	UINT64 m_media_write;		// in MB unit
	UINT64 m_error_count;
	UINT m_bad_block_num;
	short m_temperature_cur;	// 单位：摄氏度
	short m_temperature_max;	// 单位：摄氏度
	BYTE m_percentage_used;
};

// define SCSI status code

const BYTE SCSISTAT_GOOD = 0x00;
const BYTE SCSISTAT_CHECK_CONDITION = 0x02;
const BYTE SCSISTAT_CONDITION_MET = 0x04;
const BYTE SCSISTAT_BUSY = 0x08;
const BYTE SCSISTAT_INTERMEDIATE = 0x10;
const BYTE SCSISTAT_INTERMEDIATE_COND_MET = 0x14;
const BYTE SCSISTAT_RESERVATION_CONFLICT = 0x18;
const BYTE SCSISTAT_COMMAND_TERMINATED = 0x22;
const BYTE SCSISTAT_QUEUE_FULL = 0x28;
const BYTE SCSISTAT_ACA_ACTIVE = 0x30;
const BYTE SCSISTAT_TASK_ABORTED = 0x40;


class IStorageDevice : virtual public IJCInterface
{
public:
	enum READWRITE { write, read, };
	enum ATA_PROTOCOL
	{
		HARD_RESET = 0, SRST = 1, NON_DATA = 3, PIO_DATA_IN = 4,
		PIO_DATA_OUT = 5, DMA = 6, DMA_QUEUED = 7, DEVICE_DIAGNOSTIC = 8,
		DEVICE_RESET = 9, UDMA_DATA_IN = 10, UDMA_DATA_OUT = 11, FPDMA_12, RETURN_RESPONSE = 15,
	};

public:
	virtual bool Inquiry(IDENTIFY_DEVICE & id) = 0;
	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree * ext_info) = 0;
	// rd_wr: Read (true) or Write (false)
	// Sector Read / Write通过Interface转用方法实现
	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) = 0;
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) = 0;

	// Read/Write方法通过系统Read/Write函数实现，
	virtual bool Read(BYTE * buf, FILESIZE lba, size_t secs) = 0;
	virtual bool Write(BYTE * buf, FILESIZE lba, size_t secs) = 0;
	virtual void FlushCache() = 0;

	//virtual bool StartStopUnit(bool stop) = 0;

	// 用于性能测试，单位us。
	virtual UINT GetLastInvokeTime(void) = 0;
	// Device容量，单位:sector
	virtual FILESIZE GetCapacity(void) = 0;
	virtual bool DeviceLock(void) = 0;
	virtual bool DeviceUnlock(void) = 0;
	//virtual bool Dismount(void) = 0;

	//virtual void SetDeviceName(LPCTSTR name) = 0;
	//virtual LPCTSTR GetBusName(void) = 0;

	//virtual void UnmountAllLogical(void) = 0;
	// time out in second
	virtual BYTE ScsiCommand(READWRITE rd_wr, BYTE *buf, size_t length, 
		_In_ const BYTE *cb, size_t cb_length,		// input of command block
		_Out_ BYTE *sense, size_t sense_len,		// output of sense buffer
		UINT timeout) = 0;
	//virtual UINT GetTimeOut(void) = 0; 
	//virtual void SetTimeOut(UINT) = 0;

	//virtual bool Recognize() = 0;
	//virtual void Detach(HANDLE & dev) = 0;

	virtual BYTE SecurityReceive(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid) = 0;
	virtual BYTE SecuritySend(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid) = 0;

#ifdef _DEBUG
	//virtual HANDLE GetHandle() = 0;
#endif
};

class INVMeDevice : virtual public IStorageDevice
{
public:
	virtual bool ReadIdentifyDevice(BYTE cns, WORD nvmsetid, BYTE * data, size_t data_len) = 0;
	virtual bool GetLogPage(BYTE lid, WORD numld, BYTE * data, size_t data_len) = 0;
	virtual bool NVMeCommand(BYTE protocol, BYTE opcode, NVME_COMMAND * cmd, BYTE * buf, size_t length) = 0;
};


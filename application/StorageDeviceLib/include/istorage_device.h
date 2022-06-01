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
	enum VENDOR_ID
	{
		VENDOR_UNKNOWN = 0,

		VENDOR_MICRON =		0x8800,	VENDOR_MICRON_CRUCIAL=0x8801, VENDOR_MICRON_MU02=0x8802,
		VENDOR_SMI=			0x6600,
		VENDOR_MTRON =		0x0200,
		VENDOR_INDILINX =	0x0300,
		VENDOR_JMICRON =	0x0400,
		VENDOR_INTEL =		0x0500,
		VENDOR_SAMSUNG =	0x0600,
		VENDOR_SANDFORCE =	0x0700,
		VENDOR_OCZ =		0x0900,
		VENDOR_SEAGATE =	0x0A00,
		VENDOR_WDC =		0x1100,
		VENDOR_PLEXTOR =	0x1200,
		VENDOR_SANDISK =	0x1300,
//		VENDOR_OCZ_VECTOR = 14,
		VENDOR_TOSHIBA =	0x1500,
		VENDOR_CORSAIR =	0x1600,
		VENDOR_KINGSTON =	0x1700,
//		VENDOR_MICRON_MU02 = 18,
		VENDOR_NVME = 19,
		VENDOR_REALTEK =	0x2000,
		VENDOR_SKHYNIX =	0x2100,
		VENDOR_KIOXIA =		0x2200,
		VENDOR_SSSTC =		0x2300,
//		VENDOR_INTEL_DC = 24,
		VENDOR_APACER =		0x2500,
		VENDOR_PHISON =		0x2700,
		VENDOR_MARVELL =	0x2800,
		VENDOR_MAXIOTEK =	0x2900,
		VENDOR_YMTC =		0x3000,
//		VENDOR_MAX = 99,

		//VENDOR_UNKNOWN = 0x0000,
		VENDOR_BUFFALO = 0x0411,
		VENDOR_IO_DATA = 0x04BB,
		VENDOR_LOGITEC = 0x0789,
		VENDOR_INITIO = 0x13FD,
		VENDOR_SUNPLUS = 0x04FC,
//		VENDOR_JMICRON = 0x152D,
		VENDOR_CYPRESS = 0x04B4,
		VENDOR_OXFORD = 0x0928,
		VENDOR_PROLIFIC = 0x067B,
//		VENDOR_REALTEK = 0x0BDA,
		VENDOR_ALL = 0xFFFF,
	};
	std::wstring m_model_name;
	std::wstring m_serial_num;
	std::wstring m_firmware;
	std::wstring m_vendor;
	std::wstring m_cur_transfer;
	std::wstring m_max_transfer;
	std::wstring m_interface;
	BYTE m_dev_type;
	STORAGE_BUS_TYPE m_bus_type;
};

enum STORAGE_HEALTH_STATUS
{
	STATUS_ERROR = 0, STATUS_GOOD = 1, STATUS_WARNING = 2, STATUS_BAD = 3,
};

class STORAGE_HEALTH_INFO_ITEM
{
public:
	STORAGE_HEALTH_INFO_ITEM(void) {}
	STORAGE_HEALTH_INFO_ITEM(BYTE attr, DWORD id, INT64 value, const std::wstring& unit)
		: m_attr_id(attr), m_item_id(id), m_item_value(value), m_unit(unit) {}

public:
	INT64 m_item_value;
	DWORD m_item_id;
	BYTE  m_attr_id;
	STORAGE_HEALTH_STATUS m_status = STATUS_ERROR;
	std::wstring m_unit;
//	std::wstring m_item_name;
//	std::wstring m_str_value;
};
typedef std::vector<STORAGE_HEALTH_INFO_ITEM> HEALTH_INFO_LIST;

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
	static void CreateDeviceByIndex(IStorageDevice*& dev, int index);

public:
	virtual bool Inquiry(IDENTIFY_DEVICE & id) = 0;
//	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree & ext_info) = 0;
	virtual STORAGE_HEALTH_STATUS GetHealthInfo(DEVICE_HEALTH_INFO & info, std::vector<STORAGE_HEALTH_INFO_ITEM> & ext_info) = 0;
	// rd_wr: Read (true) or Write (false)
	// 
	// 
	// 


	/// <summary> Sector Read / Write通过Device的Interface专用方法读写磁盘 </summary>
	/// <param name="buf">[OUT] 读取到的data</param>
	/// <param name="lba">[IN] 起始LBA地址</param>
	/// <param name="sectors">[IN] 数据长度，以Sector为单位</param>
	/// <param name="timeout"></param>
	/// <returns></returns>
	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) = 0;
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) = 0;

	// 

	/// <summary> Read/Write方法通过系统调用Read/Write读写磁盘 </summary>
	/// <param name="buf">[OUT] 读取到的data</param>
	/// <param name="lba">[IN] 起始LBA地址</param>
	/// <param name="secs">[IN] 数据长度，以Sector为单位</param>
	/// <returns></returns>
	virtual bool Read(void * buf, FILESIZE lba, size_t secs) = 0;
	virtual bool Write(const void * buf, FILESIZE lba, size_t secs) = 0;
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

	virtual BYTE DownloadFirmware(BYTE* buf, size_t buf_len, size_t block_size, DWORD slot, bool activate) = 0;

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


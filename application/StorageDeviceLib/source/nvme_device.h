#pragma once

#include "storage_device.h"
//#include <nvme.h>

class CNVMeDevice : virtual public INVMeDevice, public CStorageDeviceComm
{
public:
	CNVMeDevice(void);
	~CNVMeDevice(void);

public:
//	virtual bool GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree & ext_info);
	virtual STORAGE_HEALTH_STATUS GetHealthInfo(DEVICE_HEALTH_INFO& info, std::vector<STORAGE_HEALTH_INFO_ITEM>& ext_info);
	virtual bool Inquiry(IDENTIFY_DEVICE& id);

	// Device容量，单位:sector
	virtual size_t GetCapacity(void) { return 0; }

	// rd_wr: Read (true) or Write (false)
	// Sector Read / Write通过Interface转用方法实现
	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false;}

	// Read/Write方法通过系统Read/Write函数实现，
	virtual bool Read(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual bool Write(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual void FlushCache() {}

	virtual bool StartStopUnit(bool stop) { return false; }

	// 用于性能测试，单位us。
	virtual UINT GetLastInvokeTime(void) { return false; }
	virtual bool DeviceLock(void) { return false; }
	virtual bool DeviceUnlock(void) { return false; }
	//virtual bool Dismount(void) { return false; }

	//virtual void SetDeviceName(LPCTSTR name) { }
	//virtual LPCTSTR GetBusName(void) { return NULL; }

	//virtual void UnmountAllLogical(void) { return false; }
	// time out in second
	//virtual UINT GetTimeOut(void) = 0; 
	//virtual void SetTimeOut(UINT) = 0;

//	virtual bool Recognize() = 0;
	//virtual void Detach(HANDLE & dev) = 0;
	bool NVMeTest(jcvos::IBinaryBuffer * & data);

public:
	virtual bool ReadIdentifyDevice(BYTE cns, WORD nvmsetid, BYTE * data, size_t data_len)
	{
		return false;
	}
	virtual bool GetLogPage(BYTE lid, WORD numld, BYTE * data, size_t data_len);

protected:
	virtual bool NVMeCommand(BYTE protocol, BYTE opcode, NVME_COMMAND * cmd, BYTE * buf, size_t length)
	{
		return false;
	}

	void GetDeviceIDFromPhysicalDriveID(std::wstring& dev_id);
//	bool GetSlotMaxCurrSpeedFromDeviceID(const std::wstring& DeviceID);

};


class CNVMePassthroughDevice : public CNVMeDevice
{
public:
	CNVMePassthroughDevice(void);
	~CNVMePassthroughDevice(void);

public:
	virtual bool Inquiry(IDENTIFY_DEVICE & id);

	// Device容量，单位:sector
	virtual size_t GetCapacity(void) { return 0; }

	// rd_wr: Read (true) or Write (false)
	// Sector Read / Write通过Interface转用方法实现
	virtual bool SectorRead(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }
	virtual bool SectorWrite(BYTE * buf, FILESIZE lba, size_t sectors, UINT timeout) { return false; }

	// Read/Write方法通过系统Read/Write函数实现，
	virtual bool Read(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual bool Write(BYTE * buf, FILESIZE lba, size_t secs) { return false; }
	virtual void FlushCache() {}

	virtual bool StartStopUnit(bool stop) { return false; }

	// 用于性能测试，单位us。
	virtual UINT GetLastInvokeTime(void) { return false; }
	virtual bool DeviceLock(void) { return false; }
	virtual bool DeviceUnlock(void) { return false; }
	//virtual bool Dismount(void) { return false; }

	//virtual void SetDeviceName(LPCTSTR name) { }
	//virtual LPCTSTR GetBusName(void) { return NULL; }

	//virtual void UnmountAllLogical(void) { return false; }
	// time out in second
	//virtual BYTE ScsiCommand(READWRITE rd_wr, BYTE *buf, size_t length,
	//	_In_ const BYTE *cb, size_t cb_length, // input of command block
	//	_Out_ BYTE *sense, size_t sense_len,		// output of sense buffer
	//	UINT timeout)
	//{
	//	return -1;
	//}
	//virtual UINT GetTimeOut(void) = 0; 
	//virtual void SetTimeOut(UINT) = 0;
	bool NVMETest(jcvos::IBinaryBuffer *& data);


// Ata NVMe interface
public:
	virtual bool ReadIdentifyDevice(BYTE cns, WORD nvmsetid, BYTE * data, size_t data_len);
	virtual bool GetLogPage(BYTE lid, WORD numld, BYTE * data, size_t data_len);

// NVMe Interface
protected:
	virtual bool NVMeCommand(BYTE protocol, BYTE opcode, NVME_COMMAND * cmd, BYTE * buf, size_t length);

	// 用于派生类检测链接的device是否合适
	virtual bool OnConnectDevice(void);


};
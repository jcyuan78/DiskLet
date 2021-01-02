#pragma once

//#include "storage_device.h"
#include "../../StorageDeviceLib/storage_device_lib.h"

using namespace System;
//using namespace System::Runtime::InteropServices;


class StaticInit
{
public:
	StaticInit(const std::wstring & config);
	~StaticInit(void);

public:
	// ���浱ǰdevice
	void SelectDevice(IStorageDevice * dev);
	void GetDevice(IStorageDevice * & dev);
	//Clone::StorageDevice ^ GetDevice(void);

	void SelectDisk(IDiskInfo * disk);
	void GetDisk(IDiskInfo * & disk);

	//void GetStorageManager(IStorageManager * manager);

public:
	IStorageManager * m_manager;

protected:
	IStorageDevice * m_cur_dev;
	IDiskInfo * m_cur_disk;
	IPartitionInfo * m_cur_partition;
//	Clone::StorageManager m_manager;
};

#include "pch.h"
#include "../include/storage_device.h"
#include <boost/cast.hpp>

using namespace Clone;
using namespace System;
using namespace System::Runtime::InteropServices;

LOCAL_LOGGER_ENABLE(L"storage", LOGGER_LEVEL_NOTICE);

//-----------------------------------------------------------------------------
// -- storage device
Clone::StorageDevice::StorageDevice(IStorageDevice * storage)
{
	JCASSERT(storage);
	m_storage = storage;
	m_storage->AddRef();
}

JcCmdLet::BinaryType ^ Clone::StorageDevice::GetLogPage(BYTE page_id, size_t secs)
{
	INVMeDevice * nvme = dynamic_cast<INVMeDevice*>(m_storage);
	if (!nvme) throw gcnew System::ApplicationException(L"The device is not a NVMe device");
	
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(secs));
	BYTE *_buf = buf->Lock();
	bool br = nvme->GetLogPage(page_id, boost::numeric_cast<WORD>(secs * 0x80 - 1), 
		_buf, SECTOR_TO_BYTE(secs));
	buf->Unlock(_buf);
	if (!br) throw gcnew System::ApplicationException(L"failed on getting log page");
	return gcnew JcCmdLet::BinaryType(buf);
}

HEALTH_INFO ^ Clone::StorageDevice::GetHealthInfo(void)
{
	JCASSERT(m_storage);
	DEVICE_HEALTH_INFO info;
	boost::property_tree::wptree prop;
	m_storage->GetHealthInfo(info, &prop);

	HEALTH_INFO ^ health = gcnew HEALTH_INFO;

	health->power_cycle = info.m_power_cycle;
	health->power_on_hours = info.m_power_on_hours;
	health->unsafe_shutdowns = info.m_unsafe_shutdowns;
	health->host_read.MB = info.m_host_read;
	health->host_write.MB = info.m_host_write;
	health->media_write.MB = info.m_media_write;
	health->error_count = info.m_error_count;
	health->bad_block_num = info.m_bad_block_num;
	health->temperature_cur= info.m_temperature_cur;
	health->temperature_max = info.m_temperature_max;
	health->percentage_used = info.m_percentage_used;

#ifdef _DEBUG
	//wprintf_s(L"percentage used = %d %%\n", info.m_percentage_used);
	//wprintf_s(L"error count = %lld\n", info.m_error_count);
	//wprintf_s(L"temperature cur=%dC, max=%dC\n", info.m_temperature_cur, info.m_temperature_max);
	//wprintf_s(L"Power cycle = %lld\n", info.m_power_cycle);
	//wprintf_s(L"Power on hours = %lld\n", info.m_power_on_hours);
	//wprintf_s(L"Unsafe shutdown = %lld\n", info.m_unsafe_shutdowns);
	//wprintf_s(L"host read = %lld MB\n", info.m_host_read);
	//wprintf_s(L"host write = %lld MB\n", info.m_host_write);
	//wprintf_s(L"medial write = %lld MB\n", info.m_media_write);
	//wprintf_s(L"bad block num = %d\n", info.m_bad_block_num);
	//wprintf_s(L"\n");
#endif
	return health;
}

INQUIRY ^ Clone::StorageDevice::Inquiry(void)
{
	JCASSERT(m_storage);
	IDENTIFY_DEVICE id;
	m_storage->Inquiry(id);

	INQUIRY ^ inquiry = gcnew INQUIRY;
	inquiry->model_name = gcnew String(id.m_model_name.c_str());
	inquiry->serial_num = gcnew String(id.m_serial_num.c_str());
	inquiry->firmware = gcnew String(id.m_firmware.c_str());
	inquiry->vendor = gcnew String(id.m_vendor.c_str());
#ifdef _DEBUG
	//wprintf_s(L"Vendor=%s\n", id.m_vendor.c_str());
	//wprintf_s(L"Model=%s\n", id.m_model_name.c_str());
	//wprintf_s(L"Serial No=%s\n", id.m_serial_num.c_str());
	//wprintf_s(L"Firmware=%s\n", id.m_firmware.c_str());
	//wprintf_s(L"\n");
#endif
	return inquiry;
}





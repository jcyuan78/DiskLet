#include "pch.h"
#include "../include/storage_device.h"
#include "../include/storage_manager.h"
#include "static_init.h"

using namespace Clone;
using namespace System;
using namespace System::Runtime::InteropServices;

LOCAL_LOGGER_ENABLE(L"libclone.storage_device", LOGGER_LEVEL_NOTICE);

//-----------------------------------------------------------------------------
// -- storage device
Clone::StorageDevice::StorageDevice(IStorageDevice * storage)
{
	JCASSERT(storage);
	m_storage = storage;
	m_storage->AddRef();
}

#ifdef INTERNAL_USE
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
#endif

//HEALTH_INFO ^ Clone::StorageDevice::GetHealthInfo(void)
Clone::HEALTH_STATUS Clone::StorageDevice::GetHealthInfo(Clone::HEALTH_INFO^ health, System::Collections::ArrayList^ ext_info)
{
	JCASSERT(m_storage);
	DEVICE_HEALTH_INFO info;
	std::vector<STORAGE_HEALTH_INFO_ITEM> _ext_info;
	STORAGE_HEALTH_STATUS status = STATUS_ERROR;
	try
	{
		status = m_storage->GetHealthInfo(info, _ext_info);

		health->power_cycle = info.m_power_cycle;
		health->power_on_hours = info.m_power_on_hours;
		health->unsafe_shutdowns = info.m_unsafe_shutdowns;
		health->host_read.MB = info.m_host_read;
		health->host_write.MB = info.m_host_write;
		health->media_write.MB = info.m_media_write;
		health->error_count = info.m_error_count;
		health->bad_block_num = info.m_bad_block_num;
		health->temperature_cur = info.m_temperature_cur;
		health->temperature_max = info.m_temperature_max;
		health->percentage_used = info.m_percentage_used;

		for (auto it = _ext_info.begin(); it != _ext_info.end(); ++it)
		{
			const STORAGE_HEALTH_INFO_ITEM& item = *it;
			HEALTH_INFO_ITEM^ hh = gcnew HEALTH_INFO_ITEM;
			hh->item_id = item.m_item_id;
			hh->attribute_id = item.m_attr_id;
			//		hh->item_name = gcnew System::String(item.m_item_name.c_str());
			hh->item_value = item.m_item_value;
			hh->status = (HEALTH_STATUS)((int)item.m_status);
			hh->unit = gcnew String(item.m_unit.c_str());
			ext_info->Add(hh);
		}
	}
	catch (jcvos::CJCException& err)
	{
		String^ msg = gcnew String(err.WhatT());
		throw gcnew Clone::CloneException(msg, err.GetErrorID());
	}
	return (HEALTH_STATUS)((int)status);
}

HEALTH_INFO ^ Clone::StorageDevice::GetHealthInfo(void)
{
	JCASSERT(m_storage);
	DEVICE_HEALTH_INFO info;
	std::vector<STORAGE_HEALTH_INFO_ITEM> _ext_info;
	STORAGE_HEALTH_STATUS status = STATUS_ERROR;
	HEALTH_INFO^ health = nullptr;
	try
	{
		status = m_storage->GetHealthInfo(info, _ext_info);
		health = gcnew HEALTH_INFO;
		health->power_cycle = info.m_power_cycle;
		health->power_on_hours = info.m_power_on_hours;
		health->unsafe_shutdowns = info.m_unsafe_shutdowns;
		health->host_read.MB = info.m_host_read;
		health->host_write.MB = info.m_host_write;
		health->media_write.MB = info.m_media_write;
		health->error_count = info.m_error_count;
		health->bad_block_num = info.m_bad_block_num;
		health->temperature_cur = info.m_temperature_cur;
		health->temperature_max = info.m_temperature_max;
		health->percentage_used = info.m_percentage_used;
	}
	catch (jcvos::CJCException& err)
	{
		String^ msg = gcnew String(err.WhatT());
		throw gcnew Clone::CloneException(msg, err.GetErrorID());
	}
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
	switch (id.m_bus_type)
	{
	case BusTypeSata:	inquiry->bus_type = BUS_TYPE::SATA_DEVICE; break;
	case BusTypeUsb:	inquiry->bus_type = BUS_TYPE::USB_DEVICE; break;
	case BusTypeNvme:	inquiry->bus_type = BUS_TYPE::NVME_DEVICE; break;
	default:			inquiry->bus_type = BUS_TYPE::OTHER_DEVICE; break;

	}
	inquiry->cur_transfer_speed = gcnew String(id.m_cur_transfer.c_str());
	inquiry->max_transfer_speed = gcnew String(id.m_max_transfer.c_str());
	inquiry->interface_type = gcnew String(id.m_interface.c_str());

	return inquiry;
}





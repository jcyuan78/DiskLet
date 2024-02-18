#pragma once

using namespace System;
using namespace System::Management::Automation;

#include "disk_info.h"

namespace Clone
{
	public enum class HEALTH_STATUS
	{
		STATUS_ERROR=0, STATUS_GOOD=1, STATUS_WARNING=2, STATUS_BAD=3,
	};
	public enum class BUS_TYPE
	{
		SATA_DEVICE, USB_DEVICE, NVME_DEVICE, OTHER_DEVICE,
	};
	public ref class INQUIRY : public Object
	{
	public:
		String ^ model_name;
		String ^ serial_num;
		String ^ firmware;
		String ^ vendor;
		BUS_TYPE bus_type;
		String ^ cur_transfer_speed;
		String ^ max_transfer_speed;
		String ^ interface_type;

	};

	public ref class HEALTH_INFO_ITEM : public Object
	{
	public:
		int item_id;
		BYTE attribute_id;
//		String^ item_name;
		INT64 item_value;
		HEALTH_STATUS status;
		String^ unit;
	};

	public ref class HEALTH_INFO : public Object
	{
	public:
		UINT64 power_cycle;
		UINT64 power_on_hours;
		UINT64 unsafe_shutdowns;
		DiskSize host_read;			// in MB unit
		DiskSize host_write;		// in MB unit
		DiskSize media_write;		// in MB unit
		UINT64 error_count;
		UINT   bad_block_num;
		short  temperature_cur;		// 单位：摄氏度
		short  temperature_max;		// 单位：摄氏度
		short  percentage_used;		// 负数表示不支持寿命信息
	};

	//-----------------------------------------------------------------------------
	// -- storage device
	public ref class StorageDevice : public Object
	{
	public:
		StorageDevice(IStorageDevice * storage);
		~StorageDevice(void) { RELEASE_INTERFACE(m_storage); }
		!StorageDevice(void) { RELEASE_INTERFACE(m_storage); }

	public:
		// for debug
		Clone::HEALTH_STATUS GetHealthInfo(Clone::HEALTH_INFO ^ info, System::Collections::ArrayList ^ ext_in
		JcCmdLet::BinaryType ^ GetLogPage(BYTE page_id, size_t secs);

		HEALTH_INFO ^ GetHealthInfo(void);
		//System::Collections::ArrayList^ GetDetailedHealthInfo(void);

		INQUIRY ^ Inquiry(void);

#ifdef INTERNAL_USE
		JcCmdLet::BinaryType ^ GetLogPage(BYTE page_id, size_t secs);
		JcCmdLet::BinaryType^ Read(UINT64 lba, UINT64 secs)
		{
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			jcvos::CreateBinaryBuffer(buf, SECTOR_TO_BYTE(secs));
			BYTE* _buf = buf->Lock();
			m_storage->Read(_buf, lba, secs);
			buf->Unlock(_buf);
			return gcnew JcCmdLet::BinaryType(buf);
		}
#endif

	protected:
		IStorageDevice *m_storage;
	};

};
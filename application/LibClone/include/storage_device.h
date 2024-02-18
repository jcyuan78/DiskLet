#pragma once

using namespace System;
using namespace System::Management::Automation;

#include "disk_info.h"

namespace Clone
{
	public ref class INQUIRY : public Object
	{
	public:
		String ^ model_name;
		String ^ serial_num;
		String ^ firmware;
		String ^ vendor;
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
		short  temperature_cur;	// 单位：摄氏度
		short  temperature_max;	// 单位：摄氏度
		BYTE   percentage_used;
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
		JcCmdLet::BinaryType ^ GetLogPage(BYTE page_id, size_t secs);

		HEALTH_INFO ^ GetHealthInfo(void);
		INQUIRY ^ Inquiry(void);

	protected:
		IStorageDevice *m_storage;
	};

};
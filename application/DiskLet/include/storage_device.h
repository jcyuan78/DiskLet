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

#define NVME_CMD_GET_LOG	(0x02)
#define NVME_CMD_IDENTIFY	(0x06)


	public ref class StorageDevice : public Object
	{
	public:
		StorageDevice(IStorageDevice * storage);
		~StorageDevice(void) { RELEASE_INTERFACE(m_storage); }
		!StorageDevice(void) { RELEASE_INTERFACE(m_storage); }

	public:
		// for debug
		JcCmdLet::BinaryType ^ GetLogPage(BYTE page_id, size_t secs);
		JcCmdLet::BinaryType^ ReadIdentifyControl(void);

		HEALTH_INFO ^ GetHealthInfo(void);
		INQUIRY ^ Inquiry(void);

		/// <summary> 调用StorageDevice的Read()方法，通过Windows File Read函数实现 </summary>
		/// <param name="lba"></param>
		/// <param name="secs"></param>
		/// <returns></returns>
		JcCmdLet::BinaryType^ ReadSectors(UINT64 lba, size_t secs)
		{
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			bool br = jcvos::CreateBinaryBuffer(buf, secs * SECTOR_SIZE);
			if (!br || !buf) THROW_ERROR(ERR_MEM, L"failed on creating buffer, size=%d", secs * SECTOR_SIZE);
			BYTE* _buf = buf->Lock();
			m_storage->Read(_buf, lba, secs);
			buf->Unlock(_buf);
			return gcnew JcCmdLet::BinaryType(buf);
		}

		void WriteSectors(UINT64 lba, size_t secs, JcCmdLet::BinaryType^ data)
		{

		}

		void GetStorageDevice(IStorageDevice*& dev)
		{
			JCASSERT(dev == nullptr);
			dev = m_storage;
			if (dev) dev->AddRef();
		}

		JcCmdLet::BinaryType^ GetFeature(BYTE fid, BYTE sel, size_t out_len)
		{
			INVMeDevice* dev = dynamic_cast<INVMeDevice*>(m_storage);
			if (dev == nullptr) throw gcnew System::ApplicationException(L"non NVMe device");

			BYTE* buf = nullptr;
			jcvos::auto_interface<jcvos::IBinaryBuffer> _buf;
			DWORD comp = 0;
			if (out_len > 0)
			{
				jcvos::CreateBinaryBuffer(_buf, out_len);
				buf = _buf->Lock();
			}
			dev->GetFeature(buf, out_len, comp, fid, sel);
			if (out_len > 0)
			{
				_buf->Unlock(buf);
				return gcnew JcCmdLet::BinaryType(_buf);
			}
			else return nullptr;

		}

		JcCmdLet::BinaryType^ Inquire(BYTE page_code, size_t data_len)
		{
			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			bool br = jcvos::CreateBinaryBuffer(buf, data_len);
			if (!br || !buf) THROW_ERROR(ERR_MEM, L"failed on creating buffer, size=%d", data_len);
			BYTE* _buf = buf->Lock();
			BYTE res = m_storage->Inquiry(_buf, data_len, page_code == 0xFF ? 0 : 1, page_code);
			wprintf_s(L"device returns: %02X\n", res);
			buf->Unlock(_buf);
			return gcnew JcCmdLet::BinaryType(buf);
		}

/*
		JcCmdLet::BinaryType ^ Test1(void)
		{
			INVMeDevice * nvme = dynamic_cast<INVMeDevice*>(m_storage);
			if (!nvme) throw gcnew System::ApplicationException(L"The device is not a NVMe device");

			NVME_COMMAND nvme_cmd;
			memset(&nvme_cmd, 0, sizeof(NVME_COMMAND));
			// read identify
			//nvme_cmd.CDW0.OPC = NVME_CMD_IDENTIFY;
			//nvme_cmd.u.IDENTIFY.CDW10.CNS = 1;
			//nvme_cmd.u.IDENTIFY.CDW11 = 0;

			// read smart
			nvme_cmd.CDW0.OPC = NVME_CMD_GET_LOG;
			nvme_cmd.NSID = 0xFFFFFFFF;
			nvme_cmd.u.GETLOGPAGE.CDW10.LID = NVME_LOG_PAGE_HEALTH_INFO;
			nvme_cmd.u.GETLOGPAGE.CDW10_V13.NUMDL = 0x7F;

			jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
			jcvos::CreateBinaryBuffer(buf, 4096);
			BYTE * _buf = buf->Lock();

	//		bool br = nvme->NVMeCommand(IStorageDevice::UDMA_DATA_IN, NVME_CMD_IDENTIFY, &nvme_cmd, _buf, 4096);
			bool br = nvme->NVMeCommand(IStorageDevice::UDMA_DATA_IN, NVME_CMD_GET_LOG, &nvme_cmd, _buf, sizeof(NVME_HEALTH_INFO_LOG));
			buf->Unlock(_buf);
			if (!br)
			{
				System::Console::WriteLine(L"failed on reading nvme");
				return nullptr;
			}
//			if (!br) throw gcnew System::ApplicationException(L"failed on getting log page");
			return gcnew JcCmdLet::BinaryType(buf);
		}
*/
	protected:
		IStorageDevice *m_storage;
	};


	//public ref class NVMeDevice : public StorageDevice
	//{
	//public:
	//	NVMeDevice(IStorageDevice* storage) : StorageDevice(storage) {};
	//	~NVMeDevice(void) {};

	//public:

	//};

};
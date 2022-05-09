#include "pch.h"
#include "disk_info.h"
#include "../include/utility.h"
#include "storage_manager.h"
#include "nvme_device.h"
#include "ata_device.h"

LOCAL_LOGGER_ENABLE(L"disk_info", LOGGER_LEVEL_NOTICE);
namespace prop_tree = boost::property_tree;

CDiskInfo::CDiskInfo(void) 
	: m_header(NULL), m_pentry(NULL), /*m_hdisk(NULL),*/ m_secondy_pentry(NULL), m_secondy_header(NULL), m_sdev(NULL)
	, m_device_class(UNKNOWN)
{
	m_class_name = L"MSFT_Disk";
	m_property.m_index = -1;
	m_property.m_size = 0;
};

CDiskInfo::~CDiskInfo(void) 
{
	CloseCache();
}

UINT CDiskInfo::Initialize(IWbemClassObject * obj, CStorageManager * manager)
{
	CWmiObjectBase::Initialize(obj, manager);
	return m_property.m_index;
}

void CDiskInfo::SetWmiProperty(IWbemClassObject * obj)
{
	prop_tree::wptree prop;
	ReadWmiObject(prop, obj);

	m_property.m_index = prop.get<UINT32>(L"Number");

	m_property.m_name = prop.get<std::wstring>(L"FriendlyName");
	m_property.m_size = prop.get<UINT64>(L"Size");
	m_offline = prop.get<bool>(L"IsOffline");
	m_readonly = prop.get<bool>(L"IsReadOnly");
	m_operation_status = 0;
	auto status = prop.get_child_optional(L"OperationalStatus");
	if (status)
	{
		for (auto it = status->begin(); it != status->end(); ++it)
		{
			UINT16 ss = it->second.get_value<UINT16>();
			if (ss > 0xD000) {m_operation_status = ss; break;}
		}
	}

	std::wstring str_guid = prop.get<std::wstring>(L"Guid", L"");
	if (!str_guid.empty())
	{
		CLSIDFromString(str_guid.c_str(), &(m_property.m_guid));
	}

	UINT16 style = prop.get<UINT16>(L"PartitionStyle", 0);
	m_property.m_style = (DISK_PROPERTY::PartitionStyle)(style);

	m_property.m_bus_type = (DISK_PROPERTY::BusType)(prop.get<UINT16>(L"BusType", 0));

	// for physical device informaiton.
	jcvos::auto_interface<IStorageDevice> dev;
	bool br = GetStorageDevice(dev);
	if (br && (dev))
	{
		IDENTIFY_DEVICE id;
		dev->Inquiry(id);
		m_property.m_model_name = id.m_model_name;
		m_property.m_serial_num = id.m_serial_num;
		m_property.m_vendor_name = id.m_vendor;
		m_property.m_firmware = id.m_firmware;
	}
	// Get Partition List

}

void CDiskInfo::SetReadOnly(bool read_only)
{
	UINT32 res = InvokeMethod(L"SetAttributes", L"IsReadOnly", read_only);
	Refresh();
}

bool CDiskInfo::GetProperty(DISK_PROPERTY & prop)
{
	//prop.m_index = m_index;
	//prop.m_size = m_size;
	//prop.m_name = m_disk_name;
	//prop.m_guid = m_gpt_id;
	//prop.m_style = m_style;
	prop = m_property;
	return true;
}

HANDLE CDiskInfo::OpenDisk(DWORD access)
{
	wchar_t path[32];
	swprintf_s(path, L"\\\\.\\PhysicalDrive%d", m_property.m_index);
	return CreateFile(path, access, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}

void CDiskInfo::AddPartition(CPartitionInfo * part)
{
	UINT index = part->GetIndex();
	if (index >= m_partitions.size())	m_partitions.resize(index+1);
	JCASSERT(m_partitions.at(index) == nullptr);
	m_partitions.at(index) = part;
	part->AddRef();

}


void CDiskInfo::AddPartition(const std::wstring & path)
{
	jcvos::auto_interface<CPartitionInfo> part;
	bool br = CPartitionInfo::CreatePartitionInfo(part, path, this, m_manager);
	if (!br || !part ) { LOG_ERROR(L"failed on create partition %s", path.c_str()); }
	AddPartition(part);
}

void CDiskInfo::RefreshPartitionList(void)
{
	m_partitions.ClearObj();
	HRESULT hres;
	auto_unknown<IEnumWbemClassObject> penum;
	hres = m_manager->m_services->CreateInstanceEnum(BSTR(L"MSFT_DiskToPartition"), WBEM_FLAG_FORWARD_ONLY, NULL, &penum);
	if (FAILED(hres) || !penum) THROW_COM_ERROR(hres, L"failed on quering disk-partition, error=0x%X", hres);

	while (1)
	{
		auto_unknown<IWbemClassObject> obj;
		ULONG count = 0;
		hres = penum->Next(WBEM_INFINITE, 1, &obj, &count);
		if (FAILED(hres)) LOG_COM_ERROR(hres, L"failed on getting object");
		if (hres == S_FALSE || obj == nullptr) break;
		prop_tree::wptree prop;
		if (!(obj == nullptr)) ReadWmiObject(prop, obj);
		std::wstring & disk_path = prop.get<std::wstring>(L"Disk");
		std::wstring & partition_path = prop.get<std::wstring>(L"Partition");
		// find disk
		if (IsObject(disk_path))
		{
			AddPartition(partition_path);
		}
	}

}

void CDiskInfo::RemovePartition(CPartitionInfo * part)
{
	JCASSERT(part);
	UINT32 index = part->m_prop.m_index;
	if (index == 0) return;
	index--;
	JCASSERT(m_partitions.at(index) == part);
	part->Release();
	m_partitions.at(index) = NULL;
}


bool CDiskInfo::GetPartition(IPartitionInfo *& part, UINT32 index)
{
	JCASSERT(!part);
	size_t num = m_partitions.size();
	if (index >= num)
	{
		LOG_ERROR(L"[err] index (%d) is larger than partition num(%d)", index, num);
		return false;
	}
	part = m_partitions.at(index);
	if (part)
	{
		part->AddRef();
		return true;
	}
	else return false;
}

int CDiskInfo::GetDiskStatus(void)
{
	switch (m_operation_status)
	{
	case WMI_DISK_OPERATION_STATUS::UNKNOWN:	return (int)IDiskInfo::UNKNOWN;
	case WMI_DISK_OPERATION_STATUS::ONLINE:		return (int)IDiskInfo::ONLINE;
	case WMI_DISK_OPERATION_STATUS::NOT_READY:	return (int)IDiskInfo::NOT_READY;
	case WMI_DISK_OPERATION_STATUS::NO_MEDIA:	return (int)IDiskInfo::NO_MEDIA;
	case WMI_DISK_OPERATION_STATUS::OFFLINE:	return (int)IDiskInfo::OFFLINE;
	case WMI_DISK_OPERATION_STATUS::FAILED:		return (int)IDiskInfo::FAILED;
	default:									return (int)IDiskInfo::UNKNOWN;
	}
}


int CDiskInfo::Online(bool on_off)
{
	auto_unknown<IWbemClassObject> in_param;
	auto_unknown<IWbemClassObject> out_param;
	auto_unknown<IWbemClassObject> out_param_2;
	auto_unknown<IWbemCallResult> result;

	//HRESULT hres = m_obj->GetMethod(L"Online", 0, &in_param, &out_param);
	//if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on getting method (Online)");
	HRESULT hres;
	if (on_off)
	{
		hres = m_manager->m_services->ExecMethod(BSTR(m_obj_path.c_str()),
			L"Online", WBEM_FLAG_RETURN_IMMEDIATELY, NULL, NULL, &out_param_2, &result);
	}
	else
	{
		hres = m_manager->m_services->ExecMethod(BSTR(m_obj_path.c_str()),
			L"Offline", WBEM_FLAG_RETURN_IMMEDIATELY, NULL, NULL, &out_param_2, &result);
	}
	if (FAILED(hres)) THROW_COM_ERROR(hres, L"failed on calling (Online)");
//	result->GetCallStatus()
	BSTR str_res=nullptr;
	if (!(result == nullptr)) hres = result->GetResultString(WBEM_INFINITE, &str_res);
	if (FAILED(hres))
	{
		LOG_COM_ERROR(hres, L"failed on getting result");
	}
	else
	{
		wprintf_s(L"Call Online, result=%s", str_res);
		SysFreeString(str_res);
	}
	Refresh();

	return GetDiskStatus();
}

bool CDiskInfo::ClearDisk(const GUID & guid)
{
	SetReadOnly(false);
	UINT32 res;
	res = InvokeMethod(L"Clear", L"RemoveData", true, L"RemoveOEM", true, L"ZeroOutEntireDisk", false);
//	if (res != 0) THROW_ERROR(ERR_APP, L"failed on clear disk, [%d]:%s", res, GetMSFTErrorMsg(res));
	if (res != 0) LOG_ERROR(L"failed on clear disk, [%d]:%s", res, GetMSFTErrorMsg(res));

	res = InvokeMethod(L"Initialize", L"PartitionStyle", UINT16(2));
//	if (res != 0) THROW_ERROR(ERR_APP, L"failed on initialzing disk, [%d]:%s", res, GetMSFTErrorMsg(res));
	if (res != 0) LOG_ERROR(L"failed on initialzing disk, [%d]:%s", res, GetMSFTErrorMsg(res));
	Refresh();
	RefreshPartitionList();
	return true;
}

bool CDiskInfo::CreatePartition(IPartitionInfo *& part, UINT64 offset, UINT64 size, const wchar_t * name, const GUID & type, const GUID & diskid, ULONGLONG attribute)
{
	auto_unknown<IWbemClassObject> out_param;

	const wchar_t * v_offset = nullptr;
	if (offset < m_property.m_size) v_offset = L"Offset";
	const wchar_t * v_type = nullptr;
	if (m_property.m_style == DISK_PROPERTY::PARTSTYLE_GPT && type != GUID({ 0 })) v_type = L"GptType";
	if (size == 0)
	{	// size为0，表示剩下的所有空间
//		size = m_property.m_size - offset;
//		size &= ~(1024L * 1024L - 1L);
		InvokeMethod(L"CreatePartition", out_param,  L"UseMaximumSize", true, v_type, type, v_offset, offset);
	}
	else
	{
		InvokeMethod(L"CreatePartition", out_param,  L"Size", size, v_type, type, v_offset, offset);
	}

	CJCVariant return_val;
	HRESULT hres = out_param->Get(BSTR(L"ReturnValue"), 0, &return_val, NULL, 0);
	UINT32 res = return_val.val<UINT32>();
	if (res != 0) THROW_ERROR(ERR_APP, L"failed on creating partion, [%d]:%s", res, GetMSFTErrorMsg(res) );

	CJCVariant partition_val;
	CIMTYPE ctype;
	hres = out_param->Get(BSTR(L"CreatedPartition"), 0, &partition_val, &ctype, 0);
	if (SUCCEEDED(hres) && ((VARIANT&)partition_val).vt == VT_UNKNOWN)
	{
		IUnknown * obj = ((VARIANT&)partition_val).punkVal;	JCASSERT(obj);
		auto_unknown<IWbemClassObject> part_obj;
		hres = obj->QueryInterface(&part_obj);
		if (SUCCEEDED(hres) && !(part_obj == nullptr))
		{
			jcvos::auto_interface<CPartitionInfo> part_info;
			bool br = CPartitionInfo::CreatePartitionInfo(part_info, part_obj, this, m_manager);
			if (br && part_info)
			{
				AddPartition(part_info);
				part_info.detach<IPartitionInfo>(part);
			}
		}
		else LOG_COM_ERROR(hres, L"[warning] failed on getting partition interface");
	}
	else LOG_COM_ERROR(hres, L"[warning] failed on getting partition object");
	if (!part)
	{
		RefreshPartitionList();
	}
	return true;
}

bool CDiskInfo::CacheGptHeader(void)
{
	LOG_STACK_TRACE();
	JCASSERT(sizeof(GptHeader) == 512);
	if (m_sdev != NULL) THROW_ERROR(ERR_APP, L"device has already cached");
	if (m_header || m_pentry) THROW_ERROR(ERR_APP, L"has already cached");

	CreateStorageDevice<CStorageDeviceComm>(m_sdev);

	m_header = new GptHeader;
	LOG_DEBUG(L"disk size = %lld , %lld secs", m_property.m_size, m_property.m_size / SECTOR_SIZE);

	LOG_DEBUG(L"read primaery header at 1");
	bool br = m_sdev->Read((BYTE*)m_header, 1, 1);
	if (!br) THROW_WIN32_ERROR(L"failed on reading primary GPT");

	LOG_DEBUG(L"read secondary header LBA=%lld", m_header->backup_lba);
	m_secondy_header = new GptHeader;
	br = m_sdev->Read((BYTE*)m_secondy_header, m_header->backup_lba, 1);
	if (!br) THROW_WIN32_ERROR(L"failed on reading secondary GPT");

	JCASSERT(m_pentry == NULL);
	m_pentry = new PartitionEntry[m_header->entry_num];

	size_t entry_secs = (m_header->entry_size * m_header->entry_num) / SECTOR_SIZE;
	LOG_DEBUG(L"read primary entry, LBA=%lld, secs=%lld", m_header->starting_lba, entry_secs);
	br = m_sdev->Read((BYTE*)m_pentry, m_header->starting_lba, entry_secs);
	if (!br) THROW_WIN32_ERROR(L"failed on reading primary entries");

	m_secondy_pentry = new PartitionEntry[m_secondy_header->entry_num];
	entry_secs = (m_secondy_header->entry_size * m_secondy_header->entry_num) / SECTOR_SIZE;
	LOG_DEBUG(L"read secondary entry, LBA=%lld, secs=%lld", m_secondy_header->starting_lba, entry_secs);
	br = m_sdev->Read((BYTE*)m_secondy_pentry, m_secondy_header->starting_lba, entry_secs);
	if (!br) THROW_WIN32_ERROR(L"failed on reading secondary entries");

	return true;
}

bool CDiskInfo::SetDiskId(const GUID & id)
{
	if (!m_header || !m_sdev || !m_pentry) THROW_ERROR(ERR_APP, L"gpt header does not cached");
	m_header->disk_gid = id;
	m_secondy_header->disk_gid = id;
	m_property.m_guid = id;
	return true;
}

bool CDiskInfo::SetPartitionId(const GUID & id, UINT index)
{
	if (!m_header || !m_sdev || !m_pentry || !m_secondy_pentry) THROW_ERROR(ERR_APP, L"gpt header does not cached");

	JCASSERT(index > 0 && index <= m_header->entry_num);
	UINT ii = index - 1;
	m_pentry[ii].part_id = id;
	m_secondy_pentry[ii].part_id = id;
	CPartitionInfo * pp = m_partitions.at(index);	JCASSERT(pp);
	pp->m_prop.m_guid = id;
	return true;
}

bool CDiskInfo::SetPartitionType(const GUID & type, UINT index)
{
	LOG_STACK_TRACE();
	if (!m_header || !m_sdev || !m_pentry) THROW_ERROR(ERR_APP, L"gpt header does not cached");
	JCASSERT(index > 0 && index <= m_header->entry_num);
	UINT ii = index - 1;
	m_pentry[ii].type = type;
	m_secondy_pentry[ii].type = type;
	CPartitionInfo * pp = m_partitions.at(index);	JCASSERT(pp);
	pp->m_prop.m_gpt_type = type;
	return true;
}

bool CDiskInfo::UpdateGptHeader(void)
{
	LOG_STACK_TRACE();
	if (!m_header || !m_sdev || !m_pentry) THROW_ERROR(ERR_APP, L"gpt header does not cached");

	// 计算分区表CRC	  
	size_t entry_size = m_header->entry_num * m_header->entry_size;
	size_t dword_size = entry_size / 4;
	DWORD crc_entry = CalculateCRC32((DWORD*)(m_pentry), dword_size);
	// 计算表头的CRC;
	m_header->entry_crc = crc_entry;
	m_header->header_crc = 0;
	m_header->header_crc = CalculateCRC32((DWORD*)(m_header), 23);

	// 计算secondary分区表CRC	  
	size_t entry_size2 = m_secondy_header->entry_num * m_secondy_header->entry_size;
	size_t dword_size2 = entry_size2 / 4;
	crc_entry = CalculateCRC32((DWORD*)(m_secondy_pentry), dword_size2);
	// 计算表头的CRC;
	m_secondy_header->entry_crc = crc_entry;
	m_secondy_header->header_crc = 0;
	m_secondy_header->header_crc = CalculateCRC32((DWORD*)(m_secondy_header), 23);

	// 回写header
	LOG_DEBUG(L"lba of partition header=%d", m_header->current_lba);
	bool br = m_sdev->Write(m_header, m_header->current_lba, 1);
	if (!br ) THROW_WIN32_ERROR(L"failed write back");

	br = m_sdev->Write(m_pentry, m_header->starting_lba, entry_size / SECTOR_SIZE);
	if (!br ) THROW_WIN32_ERROR(L"failed write back");

#ifdef WRITE_SECONDARY_PART_TAB
	br = m_sdev->Write(m_secondy_pentry, m_secondy_header->starting_lba, entry_size2 / SECTOR_SIZE);
	if (!br) THROW_WIN32_ERROR(L"failed write secondary entry");

	br = m_sdev->Write(m_secondy_header, m_header->backup_lba, 1);
	if (!br) THROW_WIN32_ERROR(L"failed on wrting secodary header");
#endif
	CloseCache();
	return true;
}

void CDiskInfo::CloseCache(void)
{
//	if (m_hdisk)		CloseHandle(m_hdisk); 
	RELEASE(m_sdev);
	//m_hdisk = NULL;
	delete m_header; 
	m_header = NULL;
	delete[] m_pentry; 
	m_pentry = NULL;
	delete[] m_secondy_pentry;
	m_secondy_pentry = NULL;
	delete[] m_secondy_header;
	m_secondy_header = NULL;
}



CDiskInfo::DEVICE_CLASS CDiskInfo::DetectDevice(IStorageDevice *& dev/*, HANDLE handle*/)
{
	LOG_STACK_TRACE();
	LOG_NOTICE(L"Detecting device for disk[%d], bus=%d", m_property.m_index, m_property.m_bus_type);
	bool br = false;
	switch (m_property.m_bus_type)
	{
	case DISK_PROPERTY::BUS_SCSI: return SCSI_DEVICE;
	case DISK_PROPERTY::BUS_ATA:
	case DISK_PROPERTY::BUS_SATA:
		return ATA_DEVICE;
	case DISK_PROPERTY::BUS_NVMe: {
//		jcvos::auto_interface<CNVMeDevice> sdev(jcvos::CDynamicInstance<CNVMeDevice>::Create());
//		if (sdev == NULL) THROW_ERROR(ERR_APP, L"failed on creating NVMe device");
//		if (sdev) sdev->Connect(handle, false);
//		jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
//#ifdef RUN_DEVICE_TEST
//		sdev->NVMeTest(buf);
//#endif
//		sdev.detach(dev);
		CreateStorageDevice<CNVMeDevice>(dev);
		return NVME_DEVICE; }
	case DISK_PROPERTY::BUS_USB: {	// 尝试不同连接
		// （1） ATA PASS THROW
		br = CreateStorageDevice<CAtaPassThroughDevice>(dev);
		if (br && dev) return ATA_PASSTHROUGH;
		RELEASE(dev);
		br = CreateStorageDevice<CNVMePassthroughDevice>(dev);
		if (br && dev) 	return NVME_PASSTHROUGH;
		RELEASE(dev);

		break;		}
	case DISK_PROPERTY::BUS_RAID: {
		// （1） ATA PASS THROW
		br = CreateStorageDevice<CAtaDeviceComm>(dev);
		if (br && dev) return ATA_DEVICE;
		RELEASE(dev);

		br = CreateStorageDevice<CAtaPassThroughDevice>(dev);
		if (br && dev) return ATA_PASSTHROUGH;
		RELEASE(dev);

		CreateStorageDevice<CStorageDeviceComm>(dev);
//		if (dev->Detect()) return ATA_PASSTHROUGH;
//		RELEASE(dev);
		return SCSI_DEVICE;
		break; }

	}
	return SCSI_DEVICE;
}


bool CDiskInfo::GetStorageDevice(IStorageDevice *& dev)
{
	LOG_STACK_TRACE();
	if (m_device_class == UNKNOWN)
	{
		m_device_class = DetectDevice(dev/*, handle*/);
		if (dev) return true;
	}

	bool br;
	//HANDLE handle = OpenDisk(GENERIC_READ | GENERIC_WRITE);
	//if (handle == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(L"[err] failed on opening device %s", m_property.m_name.c_str());
	switch (m_device_class)
	{
	case SCSI_DEVICE: {
		br = CreateStorageDevice<CStorageDeviceComm>(dev);
		break; } //THROW_ERROR(ERR_APP, L"does not support SCSI_DEVICE"); break;
	case ATA_DEVICE: {
		br = CreateStorageDevice<CAtaDeviceComm>(dev);
		break;		}
	case ATA_PASSTHROUGH: {
		br = CreateStorageDevice<CAtaPassThroughDevice>(dev);
		break;		}
	case NVME_DEVICE: {
		br = CreateStorageDevice<CNVMeDevice>(dev);
		break;	}
	case NVME_PASSTHROUGH: {
		br = CreateStorageDevice<CNVMePassthroughDevice>(dev);
		break;	}
	}
	return br;
}


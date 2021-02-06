#include "pch.h"

#include "nvme_device.h"
#include <ntddscsi.h>
#include <boost/cast.hpp>
#include <boost/algorithm/string.hpp>
#define _NVME 0
//#include "../include/utility.h"
//#include <nvme.h>

LOCAL_LOGGER_ENABLE(L"sata_device", LOGGER_LEVEL_NOTICE);

#define NVME_CMD_GET_LOG	(0x02)
#define NVME_CMD_IDENTIFY	(0x06)

typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS24 {
	SCSI_PASS_THROUGH Spt;
	UCHAR             SenseBuf[24];
	UCHAR             DataBuf[4096];
} SCSI_PASS_THROUGH_WITH_BUFFERS24, *PSCSI_PASS_THROUGH_WITH_BUFFERS24;


CNVMeDevice::CNVMeDevice(void)
{
}

CNVMeDevice::~CNVMeDevice(void)
{
}

#define DRIVE_HEAD_REG	0xA0

#define BUFFER_SIZE 8192

bool CNVMeDevice::NVMeTest(jcvos::IBinaryBuffer * & data)
{
	BYTE _query[BUFFER_SIZE];
	STORAGE_PROPERTY_QUERY *query = (STORAGE_PROPERTY_QUERY*)_query;
	BYTE _query_out[BUFFER_SIZE];

	memset(_query, 0, BUFFER_SIZE);
	
	//StorageDeviceProperty,
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageDeviceProperty;
	//	StorageDeviceIdProperty,
	//	StorageAdapterProtocolSpecificProperty,
	//	StorageAdapterTemperatureProperty,
	//	StorageDeviceTemperatureProperty,
	//	StorageAdapterPhysicalTopologyProperty,
	//	StorageDevicePhysicalTopologyProperty,
	//	StorageDeviceAttributesProperty,
	//	StorageAdapterSerialNumberProperty,
	//	StorageDeviceLocationProperty,
	//	StorageDeviceNumaProperty,


	memset(_query_out, 0, BUFFER_SIZE);

	DWORD read;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query_out, BUFFER_SIZE, &read, NULL);

	LOG_DEBUG(L"query StorageDeviceProperty, ir=%d, read=%d", br, read);
	STORAGE_DEVICE_DESCRIPTOR * desc = (STORAGE_DEVICE_DESCRIPTOR*)_query_out;

	//	StorageAdapterProperty,
	memset(_query, 0, BUFFER_SIZE);
	memset(_query_out, 0, BUFFER_SIZE);
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageAdapterProperty;
	br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query_out, 512, &read, NULL);
	LOG_DEBUG(L"query StorageAdaptorProperty, ir=%d, read=%d", br, read);
	STORAGE_ADAPTER_DESCRIPTOR * d2 = (STORAGE_ADAPTER_DESCRIPTOR*)_query_out;

	//	StorageDeviceIdProperty,
	memset(_query, 0, BUFFER_SIZE);
	memset(_query_out, 0, BUFFER_SIZE);
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageDeviceIdProperty;
	br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query_out, 512, &read, NULL);
	LOG_DEBUG(L"query StorageAdaptorProperty, ir=%d, read=%d", br, read);
	STORAGE_DEVICE_ID_DESCRIPTOR * d3 = (STORAGE_DEVICE_ID_DESCRIPTOR*)_query_out;

	//	StorageDeviceProtocolSpecificProperty,
	memset(_query, 0, BUFFER_SIZE);
	memset(_query_out, 0, BUFFER_SIZE);
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageDeviceProtocolSpecificProperty;
	br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query_out, 512, &read, NULL);
	LOG_DEBUG(L"query StorageAdaptorProperty, ir=%d, read=%d", br, read);
	STORAGE_PROTOCOL_DATA_DESCRIPTOR * d4 = (STORAGE_PROTOCOL_DATA_DESCRIPTOR*)_query_out;


// Query GetLogPage

	memset(_query, 0, BUFFER_SIZE);
	memset(_query_out, 0, BUFFER_SIZE);

	size_t bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters) 
		+ sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + NVME_MAX_LOG_SIZE;

	PSTORAGE_PROTOCOL_DATA_DESCRIPTOR protocolDataDescr = (PSTORAGE_PROTOCOL_DATA_DESCRIPTOR)_query_out;

	query->PropertyId = StorageDeviceProtocolSpecificProperty;
	query->QueryType = PropertyStandardQuery;

	STORAGE_PROTOCOL_SPECIFIC_DATA * protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;
	protocolData->ProtocolType = ProtocolTypeNvme;
	protocolData->DataType = NVMeDataTypeLogPage;
	protocolData->ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
	protocolData->ProtocolDataRequestSubValue = 0;
	protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
	protocolData->ProtocolDataLength = sizeof(NVME_HEALTH_INFO_LOG);
	//  
	// Send request down.  
	//  
	br = DeviceIoControl(m_hdev,
		IOCTL_STORAGE_QUERY_PROPERTY,
		_query,		boost::numeric_cast<DWORD>(bufferLength),
		_query_out, boost::numeric_cast<DWORD>(bufferLength),
		&read, NULL);

	if (!br) LOG_WIN32_ERROR(L"SMART/Health Information Log failed.");

	//
	// Validate the returned data.
	//
	if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
		(protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR))) 
	{
		LOG_ERROR(L"SMART/Health Information Log - data descriptor header not valid. ver=%d, size=%d, expected=%d",
			protocolDataDescr->Version, protocolDataDescr->Size, sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR));
		return false;
	}

	protocolData = &protocolDataDescr->ProtocolSpecificData;

	if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
		(protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_LOG))) {
		LOG_ERROR(L"SMART/Health Information Log - ProtocolData Offset(%d)/Length(%d) not valid. expected=%d, %d",
			protocolData->ProtocolDataOffset, sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA),
			protocolData->ProtocolDataLength, sizeof(NVME_HEALTH_INFO_LOG));
		return false;
	}

	//
	// SMART/Health Information Log Data 
	//
	{
		PNVME_HEALTH_INFO_LOG smartInfo = (PNVME_HEALTH_INFO_LOG)((PCHAR)protocolData + protocolData->ProtocolDataOffset);

		LOG_DEBUG(L"SMART/Health Information Log Data - Temperature %d.", ((ULONG)smartInfo->Temperature[1] << 8 | smartInfo->Temperature[0]) - 273);
		LOG_DEBUG(L"***SMART/Health Information Log succeeded***.\n");
	}

	bufferLength = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command)
		+ STORAGE_PROTOCOL_COMMAND_LENGTH_NVME + 4096 + sizeof(NVME_ERROR_INFO_LOG);

	//
	memset(_query, 0, BUFFER_SIZE);
	STORAGE_PROTOCOL_COMMAND * protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)_query;

	protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
	protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
	protocolCommand->ProtocolType = ProtocolTypeNvme;
	protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
	protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
	protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
	protocolCommand->DataFromDeviceTransferLength = 512;
	protocolCommand->TimeOutValue = 1000;
	protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) 
		+ STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
	protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset 
		+ protocolCommand->ErrorInfoLength;
	protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

	NVME_COMMAND * command = (PNVME_COMMAND)protocolCommand->Command;
	memset(command, 0, sizeof(NVME_COMMAND));

	command->CDW0.OPC = 0x02;	// get log page
	command->NSID = 0xFFFFFFFF;
	command->u.GETLOGPAGE.CDW10_V13.LID = 2;
	command->u.GETLOGPAGE.CDW10_V13.NUMDL = 0x7F;
//	command->u.GENERAL.CDW10 = 0x007F0002;
//	command->u.GENERAL.CDW13 = 0xto_fill_in;

	//  
	// Send request down.  
	//  

	br = DeviceIoControl(m_hdev,
		IOCTL_STORAGE_PROTOCOL_COMMAND,
		_query,		boost::numeric_cast<DWORD>(bufferLength),
		_query,		boost::numeric_cast<DWORD>(bufferLength),
		&read,	NULL);
	if (!br) LOG_WIN32_ERROR(L"failed on invoking nvme command");


	return false;
}

bool CNVMeDevice::GetLogPage(BYTE lid, WORD numld, BYTE * data, size_t data_len)
{
	JCASSERT(m_hdev!=INVALID_HANDLE_VALUE);
	size_t bufferLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters)
		+ sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + NVME_MAX_LOG_SIZE;
	jcvos::auto_array<BYTE> buffer(bufferLength);
	memset(buffer, 0, bufferLength);

	STORAGE_PROPERTY_QUERY *query = (STORAGE_PROPERTY_QUERY*)buffer.get_ptr();
	query->PropertyId = StorageDeviceProtocolSpecificProperty;
	query->QueryType = PropertyStandardQuery;

	STORAGE_PROTOCOL_SPECIFIC_DATA * protocolData = (PSTORAGE_PROTOCOL_SPECIFIC_DATA)query->AdditionalParameters;
	protocolData->ProtocolType = ProtocolTypeNvme;
	protocolData->DataType = NVMeDataTypeLogPage;
	protocolData->ProtocolDataRequestValue = lid;
	protocolData->ProtocolDataRequestSubValue = 0;
	protocolData->ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
	protocolData->ProtocolDataLength = boost::numeric_cast<DWORD>(data_len);
	// Send request down.  
	DWORD read;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		buffer, boost::numeric_cast<DWORD>(bufferLength),
		buffer, boost::numeric_cast<DWORD>(bufferLength),
		&read, NULL);

	if (!br)
	{
		LOG_WIN32_ERROR(L"failed on getting log page, id=%d", lid);
		return false;
	}

	STORAGE_PROTOCOL_DATA_DESCRIPTOR * protocolDataDescr = (STORAGE_PROTOCOL_DATA_DESCRIPTOR*)buffer.get_ptr();
	if ((protocolDataDescr->Version != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)) ||
		(protocolDataDescr->Size != sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR)))
	{
		LOG_ERROR(L"Log page: data descriptor header not valid. ver=%d, size=%d, expected=%d",
			protocolDataDescr->Version, protocolDataDescr->Size, sizeof(STORAGE_PROTOCOL_DATA_DESCRIPTOR));
		return false;
	}

	protocolData = &protocolDataDescr->ProtocolSpecificData;

	if ((protocolData->ProtocolDataOffset < sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA)) ||
		(protocolData->ProtocolDataLength < sizeof(NVME_HEALTH_INFO_LOG))) {
		LOG_ERROR(L"Log page: - ProtocolData Offset(%d)/Length(%d) not valid. expected=%d, %d",
			protocolData->ProtocolDataOffset, sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA),
			protocolData->ProtocolDataLength, sizeof(NVME_HEALTH_INFO_LOG));
		return false;
	}
	BYTE *log  = ((BYTE *)protocolData + protocolData->ProtocolDataOffset);
	memcpy_s(data, data_len, log, data_len);
	return true;
}


bool CNVMeDevice::GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree * ext_info)
{
	memset(&info, 0, sizeof(DEVICE_HEALTH_INFO));
	size_t buf_len = sizeof(NVME_HEALTH_INFO_LOG);
	jcvos::auto_array<BYTE> buf(buf_len);
	bool br = GetLogPage(NVME_LOG_PAGE_HEALTH_INFO, 0x7F, buf, buf_len);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on getting smart info");
		return false;
	}
	NVME_HEALTH_INFO_LOG *smartInfo = (NVME_HEALTH_INFO_LOG*)buf.get_ptr();

	info.m_power_cycle = *(UINT64*)(smartInfo->PowerCycle);
	info.m_power_on_hours = *(UINT64*)(smartInfo->PowerOnHours);
	info.m_unsafe_shutdowns = *(UINT64*)(smartInfo->UnsafeShutdowns);
	info.m_host_read = *(UINT64*)(smartInfo->DataUnitRead) * 1000 / 2048;
	info.m_host_write = *(UINT64*)(smartInfo->DataUnitWritten) * 1000 / 2048;
	info.m_error_count = *(UINT64*)(smartInfo->MediaErrors) + *(UINT64*)(smartInfo->ErrorInfoLogEntryCount);
	info.m_percentage_used = smartInfo->PercentageUsed;

	info.m_bad_block_num = 100- smartInfo->AvailableSpare;
	info.m_temperature_cur = *(short*)(smartInfo->Temperature) + ABSOLUTE_ZERO;
	info.m_temperature_max;

	br = GetLogPage(0x09, 0x7F, buf, buf_len);
	if (br)
	{
		UINT64 *endurance_info = (UINT64*)buf.get_ptr();
		info.m_media_write = endurance_info[10] * 1024;
	}
	else 
	{
		// windows property方式（NVMe device）不支持 endurance page
		LOG_ERROR(L"[err] failed on getting endurance info");
	}
	return true;
}

bool CNVMeDevice::NVMeCommand(BYTE protocol, BYTE opcode, NVME_COMMAND * cmd, BYTE * buf, size_t length)
{
	jcvos::auto_array<BYTE> query_buf(BUFFER_SIZE, 0);

//	STORAGE_PROTOCOL_COMMAND cmd_buf;
//	ZeroMemory(cmd_buf, bufferLength);
	
	PSTORAGE_PROTOCOL_COMMAND protocolCommand = (PSTORAGE_PROTOCOL_COMMAND)(query_buf.get_ptr());

	protocolCommand->Version = STORAGE_PROTOCOL_STRUCTURE_VERSION;
	protocolCommand->Length = sizeof(STORAGE_PROTOCOL_COMMAND);
	protocolCommand->ProtocolType = ProtocolTypeNvme;
	protocolCommand->Flags = STORAGE_PROTOCOL_COMMAND_FLAG_ADAPTER_REQUEST;
//	protocolCommand->Flags = 0;
	protocolCommand->CommandLength = STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
	protocolCommand->ErrorInfoLength = sizeof(NVME_ERROR_INFO_LOG);
	protocolCommand->ErrorInfoOffset = FIELD_OFFSET(STORAGE_PROTOCOL_COMMAND, Command) + STORAGE_PROTOCOL_COMMAND_LENGTH_NVME;
	protocolCommand->TimeOutValue = 100;

	if (protocol == IStorageDevice::PIO_DATA_IN || protocol == IStorageDevice::UDMA_DATA_IN)
	{	// read
		protocolCommand->DataFromDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
		protocolCommand->DataFromDeviceTransferLength = length;
	}
	else
	{
		protocolCommand->DataToDeviceBufferOffset = protocolCommand->ErrorInfoOffset + protocolCommand->ErrorInfoLength;
		protocolCommand->DataToDeviceTransferLength = length;
		BYTE * data_buf = query_buf + protocolCommand->DataToDeviceBufferOffset;
		memcpy_s(data_buf, query_buf.size()- protocolCommand->DataToDeviceBufferOffset, buf, length);
	}
	protocolCommand->CommandSpecific = STORAGE_PROTOCOL_SPECIFIC_NVME_ADMIN_COMMAND;

	PNVME_COMMAND command = (PNVME_COMMAND)protocolCommand->Command;
	memcpy_s(command, STORAGE_PROTOCOL_COMMAND_LENGTH_NVME, cmd, STORAGE_PROTOCOL_COMMAND_LENGTH_NVME);

	//command->CDW0.OPC = 0xFF;
	//command->u.GENERAL.CDW10 = 0xto_fill_in;
	//command->u.GENERAL.CDW12 = 0xto_fill_in;
	//command->u.GENERAL.CDW13 = 0xto_fill_in;

	//  
	// Send request down.  
	//  
	DWORD returnedLength = 0;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_STORAGE_PROTOCOL_COMMAND,
		query_buf, boost::numeric_cast<DWORD>(query_buf.size()),
		query_buf, boost::numeric_cast<DWORD>(query_buf.size()),
		&returnedLength, NULL);
	if (!br)
	{
		LOG_WIN32_ERROR(L"failed on invoking NVMe command");
		return false;
	}

	if (protocol == IStorageDevice::PIO_DATA_IN || protocol == IStorageDevice::UDMA_DATA_IN)
	{
		BYTE * data_buf = query_buf + protocolCommand->DataFromDeviceBufferOffset;
		memcpy_s(buf, length, data_buf, length);
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
// -- NVMe passthrough

CNVMePassthroughDevice::CNVMePassthroughDevice(void)
{
}

CNVMePassthroughDevice::~CNVMePassthroughDevice(void)
{
}

bool CNVMePassthroughDevice::NVMETest(jcvos::IBinaryBuffer *& data)
{
	BOOL	bRet;
	DWORD	dwReturned;
	DWORD	length;

	BYTE _query[BUFFER_SIZE];
	STORAGE_PROPERTY_QUERY *query = (STORAGE_PROPERTY_QUERY*)_query;
	memset(_query, 0, BUFFER_SIZE);

	//StorageDeviceProperty,
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageDeviceProperty;

	DWORD read;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query, BUFFER_SIZE, &read, NULL);

	LOG_DEBUG(L"query StorageDeviceProperty, ir=%d, read=%d", br, read);
	STORAGE_DEVICE_DESCRIPTOR * desc = (STORAGE_DEVICE_DESCRIPTOR*)_query;

	SCSI_PASS_THROUGH_WITH_BUFFERS24 sptwb;

	::ZeroMemory(&sptwb, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS24));

	sptwb.Spt.Length = sizeof(SCSI_PASS_THROUGH);
	sptwb.Spt.PathId = 0;
	sptwb.Spt.TargetId = 0;
	sptwb.Spt.Lun = 0;
	sptwb.Spt.SenseInfoLength = 24;
	sptwb.Spt.DataIn = SCSI_IOCTL_DATA_OUT;
	sptwb.Spt.DataTransferLength = IDENTIFY_BUFFER_SIZE;
	sptwb.Spt.TimeOutValue = 2;
	sptwb.Spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS24, DataBuf);
	sptwb.Spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS24, SenseBuf);

	sptwb.Spt.CdbLength = 12;
	sptwb.Spt.Cdb[0] = 0xA1; // NVME PASS THROUGH
	sptwb.Spt.Cdb[1] = 0x80; // ADMIN
	sptwb.Spt.Cdb[2] = 0;
	sptwb.Spt.Cdb[3] = 0;
	sptwb.Spt.Cdb[4] = 2;
	sptwb.Spt.Cdb[5] = 0;
	sptwb.Spt.Cdb[6] = 0;
	sptwb.Spt.Cdb[7] = 0;
	sptwb.Spt.Cdb[8] = 0;
	sptwb.Spt.Cdb[9] = 0;
	sptwb.Spt.Cdb[10] = 0;
	sptwb.Spt.Cdb[11] = 0;
	sptwb.DataBuf[0] = 'N';
	sptwb.DataBuf[1] = 'V';
	sptwb.DataBuf[2] = 'M';
	sptwb.DataBuf[3] = 'E';
	sptwb.DataBuf[8] = 0x02;  // GetLogPage, S.M.A.R.T.
	sptwb.DataBuf[10] = 0x56;
	sptwb.DataBuf[12] = 0xFF;
	sptwb.DataBuf[13] = 0xFF;
	sptwb.DataBuf[14] = 0xFF;
	sptwb.DataBuf[15] = 0xFF;
	sptwb.DataBuf[0x21] = 0x40;		
	sptwb.DataBuf[0x22] = 0x7A;
	sptwb.DataBuf[0x30] = 0x02;		// SMART
	sptwb.DataBuf[0x32] = 0x7F;

	length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS24, DataBuf) + sptwb.Spt.DataTransferLength;

	bRet = ::DeviceIoControl(m_hdev, IOCTL_SCSI_PASS_THROUGH,
		&sptwb, length,
		&sptwb, length, &dwReturned, NULL);

	if (bRet == FALSE)
	{
		::CloseHandle(m_hdev);
		return	FALSE;
	}

	::ZeroMemory(&sptwb, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS24));
	sptwb.Spt.Length = sizeof(SCSI_PASS_THROUGH);
	sptwb.Spt.PathId = 0;
	sptwb.Spt.TargetId = 0;
	sptwb.Spt.Lun = 0;
	sptwb.Spt.SenseInfoLength = 24;
	sptwb.Spt.DataIn = SCSI_IOCTL_DATA_IN;
	sptwb.Spt.DataTransferLength = IDENTIFY_BUFFER_SIZE;
	sptwb.Spt.TimeOutValue = 2;
	sptwb.Spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS24, DataBuf);
	sptwb.Spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS24, SenseBuf);

	sptwb.Spt.CdbLength = 12;
	sptwb.Spt.Cdb[0] = 0xA1; // NVME PASS THROUGH
	sptwb.Spt.Cdb[1] = 0x82; // ADMIN + DMA-IN
	sptwb.Spt.Cdb[2] = 0;
	sptwb.Spt.Cdb[3] = 0;
	sptwb.Spt.Cdb[4] = 2;
	sptwb.Spt.Cdb[5] = 0;
	sptwb.Spt.Cdb[6] = 0;
	sptwb.Spt.Cdb[7] = 0;
	sptwb.Spt.Cdb[8] = 0;
	sptwb.Spt.Cdb[9] = 0;
	sptwb.Spt.Cdb[10] = 0;
	sptwb.Spt.Cdb[11] = 0;

	bRet = ::DeviceIoControl(m_hdev, IOCTL_SCSI_PASS_THROUGH,
		&sptwb, length,
		&sptwb, length, &dwReturned, NULL);

	if (bRet == FALSE)		return	FALSE;

	DWORD count = 0;
	for (int i = 0; i < 512; i++)	count += sptwb.DataBuf[i];
	if (count == 0)		return	FALSE;

	jcvos::auto_array<BYTE> data_buf(4096);
	NVME_COMMAND nvme_cmd;
	memset(&nvme_cmd, 0, sizeof(NVME_COMMAND));
	nvme_cmd.CDW0.OPC = NVME_CMD_GET_LOG;
	nvme_cmd.NSID = 0xFFFFFFFF;
	nvme_cmd.u.GENERAL.CDW10 = 0x007F0002;

	br = NVMeCommand(0x82, NVME_CMD_GET_LOG, &nvme_cmd, data_buf, IDENTIFY_BUFFER_SIZE);
	if (!br) LOG_ERROR(L"[err] ")
	return true;
}

#define SET_ID_PROP(dst, offset, len)	{	\
	memset(str_buf, 0, STR_BUF_LEN);	\
	memcpy_s(str_buf, STR_BUF_LEN, data + offset, len);	\
	jcvos::Utf8ToUnicode(dst, str_buf);	}

bool CNVMePassthroughDevice::Inquiry(IDENTIFY_DEVICE & id)
{
	jcvos::auto_array<BYTE> data(4096);
	bool br=ReadIdentifyDevice(1, 0, data, 4096);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on reading Identify");
		return false;
	}
#define STR_BUF_LEN		(64)

	char str_buf[STR_BUF_LEN];
	SET_ID_PROP(id.m_serial_num, 4, 20);
	boost::algorithm::trim(id.m_serial_num);
	SET_ID_PROP(id.m_model_name, 24, 40);
	SET_ID_PROP(id.m_firmware, 64, 8);
	WORD * wbuf = (WORD*)((BYTE*)data);
	memset(str_buf, 0, STR_BUF_LEN);
	sprintf_s(str_buf, "%04X-%04X", wbuf[0], wbuf[1]);
	jcvos::Utf8ToUnicode(id.m_vendor, str_buf);
	return true;
}

/*
bool CNVMePassthroughDevice::GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree & ext_info)
{
	memset(&info, 0, sizeof(DEVICE_HEALTH_INFO));
	jcvos::auto_array<BYTE> data_buf(4096);
	// smart info
	bool br = GetLogPage(0x02, 0x7F, data_buf, IDENTIFY_BUFFER_SIZE);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on getting smart info");
		return false;
	}

	WORD * wbuf = (WORD*)((BYTE*)data_buf);
	UINT64 * llbuf = (UINT64*)((BYTE*)data_buf);
	info.m_host_read = llbuf[4]*1000 / 2048;
	info.m_host_write = llbuf[6]*1000 / 2048;
	info.m_power_cycle = llbuf[14];
	info.m_power_on_hours = llbuf[16];
	info.m_unsafe_shutdowns = llbuf[18];
	info.m_error_count = llbuf[20] + llbuf[22];
	info.m_temperature_cur = (short)(MAKEWORD(data_buf[1], data_buf[2])) + ABSOLUTE_ZERO;
	info.m_percentage_used = data_buf[5];
	info.m_bad_block_num = 100 - data_buf[3];

	br = GetLogPage(0x09, 0x7F, data_buf, IDENTIFY_BUFFER_SIZE);
	info.m_media_write = llbuf[10] * 1024;

	return true;
}
*/

bool CNVMePassthroughDevice::ReadIdentifyDevice(BYTE cns, WORD nvmsetid, BYTE * data, size_t data_len)
{
	NVME_COMMAND nvme_cmd;
	memset(&nvme_cmd, 0, sizeof(NVME_COMMAND));
	nvme_cmd.CDW0.OPC = NVME_CMD_IDENTIFY;

	nvme_cmd.u.IDENTIFY.CDW10.CNS = cns;
#if _NVME==1
	nvme_cmd.u.IDENTIFY.CDW11 = nvmsetid;
#else
	nvme_cmd.u.IDENTIFY.CDW11.NVMSETID = nvmsetid;
#endif
//	nvme_cmd.u.IDENTIFY.CDW14 = uuid;

	bool br = NVMeCommand(0x82, NVME_CMD_IDENTIFY, &nvme_cmd, data, data_len);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on invoking IDENTIFY, cns=0x%02X", cns);
		return false;
	}
	return true;
}

bool CNVMePassthroughDevice::GetLogPage(BYTE lid, WORD numld, BYTE * data, size_t data_len)
{
	NVME_COMMAND nvme_cmd;
	memset(&nvme_cmd, 0, sizeof(NVME_COMMAND));
	nvme_cmd.CDW0.OPC = NVME_CMD_GET_LOG;
	nvme_cmd.NSID = 0xFFFFFFFF;
	nvme_cmd.u.GETLOGPAGE.CDW10.LID = lid;
	nvme_cmd.u.GETLOGPAGE.CDW10_V13.NUMDL = numld;

	bool br = NVMeCommand(0x82, NVME_CMD_GET_LOG, &nvme_cmd, data, data_len);
	if (!br)
	{
		LOG_ERROR(L"[err] ");
		return false;
	}
	return true;
}

bool CNVMePassthroughDevice::NVMeCommand(BYTE protocol, BYTE opcode, NVME_COMMAND * cmd, BYTE * buf, size_t length)
{
	BYTE sense_buf[24];
	memset(sense_buf, 0, 24);

	BYTE cdb[CDB10GENERIC_LENGTH];
	memset(cdb, 0, CDB10GENERIC_LENGTH);
	cdb[0] = 0xA1;	// NVMe pass through
	cdb[1] = protocol & 0xFD;	// send command
	cdb[4] = 2;
 
	BYTE data_buf[IDENTIFY_BUFFER_SIZE];
	memset(data_buf, 0, IDENTIFY_BUFFER_SIZE);

	DWORD * cdw = (DWORD*)((BYTE*)data_buf);
	data_buf[0] = 'N'; data_buf[1] = 'V'; data_buf[2] = 'M'; data_buf[3] = 'E';
	memcpy_s(data_buf + 8, sizeof(NVME_COMMAND), cmd, sizeof(NVME_COMMAND));

	BYTE status = ScsiCommand(write, data_buf, IDENTIFY_BUFFER_SIZE, cdb, 12, sense_buf, 24, 5000);

	if (protocol & 0x02)
	{
		cdb[1] = protocol;
		status = ScsiCommand(read, buf, length, cdb, 12, sense_buf, 24, 50000);
	}

	return true;
}

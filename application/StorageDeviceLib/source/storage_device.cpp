#include "pch.h"

#include "nvme_device.h"
#include "ntddscsi.h"
#include <boost/cast.hpp>
#include <boost/algorithm/string.hpp>

LOCAL_LOGGER_ENABLE(L"storage_device", LOGGER_LEVEL_NOTICE);

#define SCSIOP_SECURITY_IN		0xA2
#define SCSIOP_SECURITY_OUT		0xB5

#define CDB12GENERIC_LENGTH                  12

#define Lo

//
// SCSI bus status codes.
//

//#define SCSISTAT_GOOD                  0x00
//#define SCSISTAT_CHECK_CONDITION       0x02
//#define SCSISTAT_CONDITION_MET         0x04
//#define SCSISTAT_BUSY                  0x08
//#define SCSISTAT_INTERMEDIATE          0x10
//#define SCSISTAT_INTERMEDIATE_COND_MET 0x14
//#define SCSISTAT_RESERVATION_CONFLICT  0x18
//#define SCSISTAT_COMMAND_TERMINATED    0x22
//#define SCSISTAT_QUEUE_FULL            0x28
//#define SCSISTAT_ACA_ACTIVE			   0x30
//#define SCSISTAT_TASK_ABORTED		   0x40


CStorageDeviceComm::CStorageDeviceComm(void)
	: m_hdev(INVALID_HANDLE_VALUE)
{

}

CStorageDeviceComm::~CStorageDeviceComm(void)
{
	if (m_hdev != INVALID_HANDLE_VALUE) CloseHandle(m_hdev);
}

bool CStorageDeviceComm::Connect(HANDLE dev, bool own)
{
	if (own) { m_hdev = dev; }
	else
	{
		BOOL br = DuplicateHandle(GetCurrentProcess(), dev, GetCurrentProcess(), &m_hdev,
			0, FALSE, DUPLICATE_SAME_ACCESS);
		if (!br)
		{
			LOG_WIN32_ERROR(L"[err], failed of duplicate handle (%d)", dev);
			return false;
		}
	}
	return true;
}
//
//bool CStorageDeviceComm::Identify(boost::property_tree::wptree & prop)
//{
//	return false;
//}

#define PROPERTY_TO_STRING(offset, str) {	\
	if (desc->##offset)		{		\
		jcvos::Utf8ToUnicode(str, (char*)desc + desc->##offset);	\
	}	}	


bool CStorageDeviceComm::Inquiry(IDENTIFY_DEVICE & id)
{
	BYTE _query[512];
	memset(_query, 0, 512);
	BYTE _query_out[512];

	STORAGE_PROPERTY_QUERY *query = (STORAGE_PROPERTY_QUERY*)_query;
	query->QueryType = PropertyStandardQuery;
	query->PropertyId = StorageDeviceProperty;


	memset(_query_out, 0, 512);

	DWORD read;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_STORAGE_QUERY_PROPERTY,
		query, sizeof(STORAGE_PROPERTY_QUERY),
		_query_out, 512, &read, NULL);
	if (!br) THROW_WIN32_ERROR(L"failed on calling IOCTL_STORAGE_QUERY_PROPERTY");
	STORAGE_DEVICE_DESCRIPTOR * desc = (STORAGE_DEVICE_DESCRIPTOR*)_query_out;

	id.m_dev_type = desc->DeviceType;
	id.m_bus_type = desc->BusType;
	char * str;
	PROPERTY_TO_STRING(ProductIdOffset, id.m_model_name);

	//if (desc->ProductIdOffset)
	//{
	//	str = (char*)desc + desc->ProductIdOffset;
	//	jcvos::Utf8ToUnicode(id.m_model_name, str);
	//}
	if (desc->SerialNumberOffset)
	{
		str = (char*)desc + desc->SerialNumberOffset;
		jcvos::Utf8ToUnicode(id.m_serial_num, str);
		boost::algorithm::trim(id.m_serial_num);
	}
	if (desc->VendorIdOffset)
	{
		str = (char*)desc + desc->VendorIdOffset;
		jcvos::Utf8ToUnicode(id.m_vendor, str);
	}
//	desc->ProductRevisionOffset
	PROPERTY_TO_STRING(ProductRevisionOffset, id.m_firmware);

	return true;
}


#ifndef SPT_SENSE_LENGTH
#define SPT_SENSE_LENGTH	(32)
#endif
//#define SPTWB_DATA_LENGTH	4*512
//#define CDB6GENERIC_LENGTH	6
//#define CDB10GENERIC_LENGTH	16

typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER
{
	SCSI_PASS_THROUGH_DIRECT sptd;
	ULONG             Filler;      // realign buffer to double word boundary
	UCHAR             ucSenseBuf[SPT_SENSE_LENGTH];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

//typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS
//{
//	SCSI_PASS_THROUGH spt;
//	ULONG             Filler;      // realign buffers to double word boundary
//	UCHAR             ucSenseBuf[SPT_SENSE_LENGTH];
//	UCHAR             ucDataBuf[SPTWB_DATA_LENGTH];
//} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;




BYTE CStorageDeviceComm::ScsiCommand(READWRITE rd_wr, BYTE * buf, size_t length, const BYTE * cb, size_t cb_length, BYTE * sense, size_t sense_len, UINT timeout)
{
	//JCASSERT(buf);
	JCASSERT(cb);
	LOG_STACK_TRACE_EX(L"cmd=0x%02X", cb[0]);
	JCASSERT(m_hdev);

	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;
	ZeroMemory(&sptdwb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));

	sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sptdwb.sptd.PathId = 0;
	sptdwb.sptd.TargetId = 1;
	sptdwb.sptd.Lun = 0;
	sptdwb.sptd.CdbLength = boost::numeric_cast<UCHAR>(cb_length);
	sptdwb.sptd.SenseInfoLength = SPT_SENSE_LENGTH;
	sptdwb.sptd.DataIn = (rd_wr == read) ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_OUT;
	sptdwb.sptd.DataTransferLength = boost::numeric_cast<ULONG>(length);
	sptdwb.sptd.TimeOutValue = timeout;
	sptdwb.sptd.DataBuffer = buf;
	// get offset
	sptdwb.sptd.SenseInfoOffset = (ULONG)offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	memcpy_s(sptdwb.sptd.Cdb, CDB10GENERIC_LENGTH, cb, min(cb_length, CDB10GENERIC_LENGTH));

	ULONG llength = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
	ULONG	returned = 0;
	BOOL	success;

	LARGE_INTEGER t0, t1;		// 性能计算
	QueryPerformanceCounter(&t0);
	success = DeviceIoControl(m_hdev, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptdwb, llength,
		&sptdwb, llength,
		&returned, FALSE);
	QueryPerformanceCounter(&t1);		// 性能计算
	//if (t0.QuadPart <= t1.QuadPart)		m_last_invoke_time = t1.QuadPart - t0.QuadPart;
	//else								m_last_invoke_time = 0;

	if (sense) memcpy_s(sense, sense_len, sptdwb.ucSenseBuf, min(SPT_SENSE_LENGT, sense_len));
	if (!success)	THROW_WIN32_ERROR(L"failed on calling IOCTL_SCSI_PASS_THROUGH_DIRECT ");
//	return sptdwb.ucSenseBuf[0xC];
	return sptdwb.sptd.ScsiStatus;
}

const wchar_t* ScsiStatusCodeToString(BYTE code)
{
	switch (code)
	{
	case SCSISTAT_GOOD:				return L"Good"; 
	case SCSISTAT_CHECK_CONDITION:	return L"Check condition";
	case SCSISTAT_CONDITION_MET:	return L"Busy";
	case SCSISTAT_BUSY:				return L"Busy";
	case SCSISTAT_INTERMEDIATE:		return L"Intermediate";
	case SCSISTAT_INTERMEDIATE_COND_MET:	return L"Intermediate cond met";
	case SCSISTAT_RESERVATION_CONFLICT:		return L"Reservation conflict";
	case SCSISTAT_COMMAND_TERMINATED:		return L"Command terminated";
	case SCSISTAT_QUEUE_FULL:				return L"Task set full";
	case SCSISTAT_ACA_ACTIVE:				return L"ACA active";
	case SCSISTAT_TASK_ABORTED:				return L"Task aborted";
	default: return L"Unknonw status";
	}
}

BYTE CStorageDeviceComm::SecurityReceive(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid)
{
	BYTE cdb[16];
	memset(cdb, 0, 16);
	cdb[0] = SCSIOP_SECURITY_IN;
	cdb[1] = (BYTE)(protocolid & 0xFF);
	cdb[2] = (BYTE)((comid >> 8) & 0xFF);
	cdb[3] = (BYTE)(comid & 0xFF);
	cdb[6] = HIBYTE(HIWORD(buf_len));
	cdb[7] = LOBYTE(HIWORD(buf_len));
	cdb[8] = HIBYTE(LOWORD(buf_len));
	cdb[9] = LOBYTE(LOWORD(buf_len));

	SENSE_DATA sense;

	BYTE res = ScsiCommand(IStorageDevice::read, buf, buf_len, cdb, CDB12GENERIC_LENGTH, 
		(BYTE*)&sense, sizeof(SENSE_DATA), 600);
	if (res != SCSISTAT_GOOD) LOG_ERROR(L"[err] device returns error (0x%02X): %s", res, ScsiStatusCodeToString(res));
	return res;
}

BYTE CStorageDeviceComm::SecuritySend(BYTE* buf, size_t buf_len, DWORD protocolid, DWORD comid)
{
	BYTE cdb[16];
	memset(cdb, 0, 16);
	cdb[0] = SCSIOP_SECURITY_OUT;
	cdb[1] = (BYTE)(protocolid & 0xFF);
	cdb[2] = (BYTE)((comid >> 8) & 0xFF);
	cdb[3] = (BYTE)(comid & 0xFF);
	cdb[6] = HIBYTE(HIWORD(buf_len));
	cdb[7] = LOBYTE(HIWORD(buf_len));
	cdb[8] = HIBYTE(LOWORD(buf_len));
	cdb[9] = LOBYTE(LOWORD(buf_len));

	SENSE_DATA sense;

	BYTE res = ScsiCommand(IStorageDevice::write, buf, buf_len, cdb, CDB12GENERIC_LENGTH,
		(BYTE*)&sense, sizeof(SENSE_DATA), 600);
	if (res != SCSISTAT_GOOD) LOG_ERROR(L"[err] device returns error (0x%02X): %s", res, ScsiStatusCodeToString(res));
	return res;
}



///////////////////////////////////////////////////////////////////////////////
// -- TCG features

//bool CStorageDeviceComm::L0Discovery(BYTE* buf)
//{
//	bool br = SecurityReceive(buf, 512, 0x01, 0x01);
//	if (!br) { LOG_ERROR(L"[err] failed on calling security receive command"); }
//	return br;
//}
//

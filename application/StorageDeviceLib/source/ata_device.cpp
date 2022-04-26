#include "pch.h"

#include "ata_device.h"
#include <boost/cast.hpp>
#include "ntddscsi.h"
#include <boost/algorithm/string.hpp>

LOCAL_LOGGER_ENABLE(L"ata_device", LOGGER_LEVEL_NOTICE);

#define ATACMD_SMART	(0xB0)
#define ATACMD_IDENTIFY	(0xEC)

//#ifndef SPT_SENSE_LENGTH
//#define SPT_SENSE_LENGTH	(32)
//#endif
//#define SPTWB_DATA_LENGTH	4*512

#define ATTRIBUTE_SIZE		(12)
#define MAX_SMART_ATTRI_NUM	(360 / ATTRIBUTE_SIZE)



//typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER
//{
//	SCSI_PASS_THROUGH_DIRECT sptd;
//	ULONG             Filler;      // realign buffer to double word boundary
//	UCHAR             ucSenseBuf[SPT_SENSE_LENGTH];
//} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

//typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS
//{
//	SCSI_PASS_THROUGH spt;
//	ULONG             Filler;      // realign buffers to double word boundary
//	UCHAR             ucSenseBuf[SPT_SENSE_LENGTH];
//	UCHAR             ucDataBuf[SPTWB_DATA_LENGTH];
//} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;





CAtaDeviceComm::~CAtaDeviceComm(void)
{
}

bool CAtaDeviceComm::Inquiry(jcvos::IBinaryBuffer *& data)
{	// identify device
	JCASSERT(data == NULL);
	JCASSERT(m_hdev && m_hdev!=INVALID_HANDLE_VALUE);

	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	bool res = jcvos::CreateBinaryBuffer(buf, READ_ATTRIBUTE_BUFFER_SIZE);
	if (!res || !buf) THROW_ERROR(ERR_APP, L"failed on creating buffer.");
	BYTE * _buf = buf->Lock();
	memset(_buf, 0, READ_ATTRIBUTE_BUFFER_SIZE);

	ATA_REGISTER reg;
	memset(&reg, 0, sizeof(ATA_REGISTER));

	reg.command = ATACMD_IDENTIFY;
	reg.sec_count = 1;
	bool br = AtaCommand(reg, PIO_DATA_IN, _buf, 1);

	buf->Unlock(_buf);
	buf.detach(data);
	return br;
}

void SwapFillBuffer(char * dst, size_t dst_len, BYTE * src, size_t len)
{
	memset(dst, 0, dst_len);
	for (size_t ii = 0; ii < len; ii += 2)	dst[ii] = src[ii + 1], dst[ii + 1] = src[ii];
}

//#define AddStringMember(name, src_offset, src_len)	{		\
//	SwapFillBuffer(str, 128, _buf+src_offset, src_len);		\
//	AddPropertyMember<String>(list, name, str);	}

bool CAtaDeviceComm::Inquiry(IDENTIFY_DEVICE & id)
{
	static const size_t BUF_LEN = 128;
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	bool br = Inquiry(buf);
	if (!br || !buf)
	{
		LOG_ERROR(L"[err] failed on reading identify device");
		return false;
	}

	// decode identify device
	char str[BUF_LEN];
	BYTE * _buf = buf->Lock();
	SwapFillBuffer(str, BUF_LEN, _buf + 20, 20);
	jcvos::Utf8ToUnicode(id.m_serial_num, str);
	boost::algorithm::trim(id.m_serial_num);

	SwapFillBuffer(str, BUF_LEN, _buf + 54, 40);
	jcvos::Utf8ToUnicode(id.m_model_name, str);

	SwapFillBuffer(str, BUF_LEN, _buf + 46, 8);
	jcvos::Utf8ToUnicode(id.m_firmware, str);

	buf->Unlock(_buf);
	return true;
}

bool CAtaDeviceComm::ReadSmart(BYTE * _buf, size_t length)
{
//	JCASSERT(data == NULL);
	JCASSERT(m_hdev && m_hdev!=INVALID_HANDLE_VALUE);

//	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
//	bool res = jcvos::CreateBinaryBuffer(buf, READ_ATTRIBUTE_BUFFER_SIZE);
//	if (!res || !buf) THROW_ERROR(ERR_APP, L"failed on creating buffer.");
//	BYTE * _buf = buf->Lock();

	ATA_REGISTER reg;
	memset(&reg, 0, sizeof(ATA_REGISTER));

	// enable smart operation
	reg.command = ATACMD_SMART;
	reg.set_lba(0xC24F00);
	reg.feature = 0xD8;			// enable SMART operation
	bool br = AtaCommand(reg, NON_DATA, _buf, 1);
	// check register
	LOG_NOTICE(L"Enable SMART operation, status=0x%02X, error=0x%02X", reg.status, reg.error);
	
	// read smart data
	memset(&reg, 0, sizeof(ATA_REGISTER));
	reg.command = ATACMD_SMART;
	reg.set_lba(0xC24F00);
	reg.feature = 0xD0;
	reg.sec_count = 1;
	br = AtaCommand(reg, PIO_DATA_IN, _buf, 1);
	// check register
	LOG_NOTICE(L"Read Smart data, status = 0x%02X, error=0x%02X", reg.status, reg.error);

	//buf->Unlock(_buf);
	//buf.detach(data);
	return br;

}

size_t CAtaDeviceComm::SmartParser(
	CSmartAttribute * attri,	// [output] 调用者确保空间
	size_t attri_size, BYTE * buf, size_t length)
{
	if (length < SECTOR_SIZE) ERROR_TERM_(return 0, L"data buffer is empty");

	size_t attri_index = 0, out_index = 0;
	size_t offset = 2;

	for (attri_index = 0;
		(attri_index < MAX_SMART_ATTRI_NUM) && (out_index < attri_size);
		++attri_index, offset += ATTRIBUTE_SIZE)
	{
		BYTE * attri_buf = buf + offset;
		if (attri_buf[0] != 0)
		{
			CSmartAttribute * _a = attri + out_index;
			memcpy_s(_a, sizeof(CSmartAttribute), attri_buf, ATTRIBUTE_SIZE);
			out_index++;
		}
	}
	return out_index;

}

bool CAtaDeviceComm::GetHealthInfo(DEVICE_HEALTH_INFO & info, boost::property_tree::wptree *)
{
	memset(&info, 0, sizeof(DEVICE_HEALTH_INFO));
	jcvos::auto_array<BYTE> smart_buf(SECTOR_SIZE, 0);
	bool br = ReadSmart(smart_buf, SECTOR_SIZE);
	if (!br)
	{
		LOG_ERROR(L"[err] failed on reading SMART");
		return false;
	}

	jcvos::auto_array<CSmartAttribute> attri(MAX_SMART_ATTRI_NUM, 0);

	size_t attri_num = SmartParser(attri, MAX_SMART_ATTRI_NUM, smart_buf, SECTOR_SIZE);

	for (size_t ii = 0; ii < attri_num; ++ii)
	{
		switch (attri[ii].m_id)
		{
		case 0x05: info.m_bad_block_num = boost::numeric_cast<UINT>(attri[ii].m_raw_val); break;
		case 0x09: info.m_power_on_hours = attri[ii].m_raw_val; break;
		case 0x0C: info.m_power_cycle = attri[ii].m_raw_val; break;
		case 0xC0: info.m_unsafe_shutdowns = attri[ii].m_raw_val; break;
		case 0xC2: info.m_temperature_cur = attri[ii].m_raw[0];
			info.m_temperature_max = attri[ii].m_raw[4]; break;
		case 0xC7: info.m_error_count = attri[ii].m_raw_val; break;
		case 0xF1: info.m_host_write = attri[ii].m_raw_val * 32; break;
		case 0xF2: info.m_host_read = attri[ii].m_raw_val * 32; break;
		}
	}

	return true;
}

bool CAtaDeviceComm::AtaCommand(ATA_REGISTER & reg, ATA_PROTOCOL prot, BYTE * buf, size_t secs)
{
	LOG_STACK_TRACE();
	LOG_TRACE(_T("Send ata cmd: %02X, %02X, %02X, %02X, %02X, %02X, %02X")
		, reg.feature, reg.sec_count, reg.lba_low, reg.lba_mid, reg.lba_hi, reg.device, reg.command);
	JCASSERT(m_hdev);

	if (prot == NON_DATA || (!buf && secs == 0)) return AtaCommandNoData(reg);

	size_t buf_len = secs * SECTOR_SIZE;
	// 申请一个4KB对齐的内存。基本思想：申请一块内存比原来多4KB的空间，
	// 然后将指针移到4KB对齐的位置。移动最多不会超过4KB。
	static const size_t ALIGNE_SIZE = 4096;		// Aligne to 4KB
	jcvos::auto_array<BYTE>	_buf(buf_len + 2 * ALIGNE_SIZE);	// 
#if 1	// aligne
	BYTE * __buf = (BYTE*)_buf;
	size_t align_mask = (~ALIGNE_SIZE) + 1;
	BYTE * align_buf = (BYTE*)((size_t)(__buf)& align_mask) + ALIGNE_SIZE;			// 4KBB aligne
#else
	BYTE * align_buf = buf;
#endif

	size_t hdr_size = sizeof(ATA_PASS_THROUGH_DIRECT);

	ATA_PASS_THROUGH_DIRECT	hdr_in;
	memset(&hdr_in, 0, hdr_size);

	hdr_in.DataTransferLength = boost::numeric_cast<ULONG>(buf_len);
	hdr_in.DataBuffer = align_buf;
	if (prot == PIO_DATA_OUT || prot == UDMA_DATA_OUT)
	{
		hdr_in.AtaFlags = ATA_FLAGS_DATA_OUT;
		if (buf_len > 0) memcpy_s(align_buf, buf_len, buf, buf_len);
	}
	else
	{
		hdr_in.AtaFlags = ATA_FLAGS_DATA_IN;
	}
	if (prot == UDMA_DATA_IN || prot == UDMA_DATA_OUT)	hdr_in.AtaFlags |= ATA_FLAGS_USE_DMA;
	else		hdr_in.AtaFlags |= ATA_FLAGS_NO_MULTIPLE;

	JCASSERT(hdr_size <= USHRT_MAX);
	hdr_in.Length = boost::numeric_cast<USHORT>(hdr_size);
	hdr_in.TimeOutValue = 5;
	memcpy_s(hdr_in.CurrentTaskFile, 8, &reg, 8);

	DWORD readsize = 0;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_ATA_PASS_THROUGH_DIRECT, 
		&hdr_in, boost::numeric_cast<DWORD>(hdr_size), &hdr_in, boost::numeric_cast<DWORD>(hdr_size), 
		&readsize, NULL);
	if (!br)
	{
		jcvos::CWin32Exception err(GetLastError(), _T("failure on ata command "));
		LOG_ERROR(err.WhatT());
	}
	memcpy_s(&reg, 8, hdr_in.CurrentTaskFile, 8);
	if ((prot == PIO_DATA_IN || prot == UDMA_DATA_IN) && (buf_len > 0))
		memcpy_s(buf, buf_len, align_buf, buf_len);

	LOG_DEBUG(_T("Returned ata cmd: %02X, %02X"), reg.error, reg.status);

	if (!br) THROW_WIN32_ERROR(_T("failure in invoking ATA command."));

	return true;
}

bool CAtaDeviceComm::AtaCommand48(ATA_REGISTER &reg_pre, ATA_REGISTER &reg_cur, 
		ATA_PROTOCOL prot, BYTE * buf, size_t secs)
{
	JCASSERT(m_hdev);

	size_t buf_len = secs * SECTOR_SIZE;
	size_t hdr_size = sizeof(ATA_PASS_THROUGH_DIRECT);
	ATA_PASS_THROUGH_DIRECT	hdr_in;
	memset(&hdr_in, 0, hdr_size);

	ATA_PASS_THROUGH_DIRECT	hdr_out;
	memset(&hdr_out, 0, hdr_size);
	if (prot == PIO_DATA_OUT || prot == UDMA_DATA_OUT)
	{
		hdr_out.DataBuffer = buf;
		hdr_out.DataTransferLength = boost::numeric_cast<ULONG>(buf_len);
		hdr_in.AtaFlags = ATA_FLAGS_DATA_OUT;
	}
	else
	{
		hdr_in.DataTransferLength = boost::numeric_cast<ULONG>(buf_len);
		hdr_in.DataBuffer = buf;
		hdr_in.AtaFlags = ATA_FLAGS_DATA_IN;
	}
	hdr_in.AtaFlags |= ATA_FLAGS_48BIT_COMMAND;
	hdr_in.AtaFlags |= ATA_FLAGS_USE_DMA;

	//JCASSERT(hdr_size <= USHRT_MAX);
	hdr_in.Length = boost::numeric_cast<USHORT>(hdr_size);
	hdr_in.TimeOutValue = 100;
	memcpy_s(hdr_in.CurrentTaskFile, 8, &reg_cur, 8);
	memcpy_s(hdr_in.PreviousTaskFile, 8, &reg_pre, 8);

	//hdr_out.Length = boost::numeric_cast<USHORT>(hdr_size);

	DWORD readsize = 0;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_ATA_PASS_THROUGH_DIRECT, 
		&hdr_in, boost::numeric_cast<DWORD>(hdr_size), &hdr_out, boost::numeric_cast<DWORD>(hdr_size), 
		&readsize, NULL);
	memcpy_s(&reg_cur, 8, hdr_out.CurrentTaskFile, 8);
	memcpy_s(&reg_pre, 8, hdr_out.PreviousTaskFile, 8);

	return true;
}

bool CAtaDeviceComm::AtaCommandNoData(ATA_REGISTER &reg)
{
	size_t hdr_size = sizeof(ATA_PASS_THROUGH_DIRECT);
	JCASSERT(hdr_size <= USHRT_MAX);

	ATA_PASS_THROUGH_DIRECT	hdr_in;
	memset(&hdr_in, 0, hdr_size);
	hdr_in.DataTransferLength = 0;
	hdr_in.DataBuffer = NULL;
	hdr_in.AtaFlags |= ATA_FLAGS_NO_MULTIPLE;
	hdr_in.Length = (USHORT)hdr_size;
	hdr_in.TimeOutValue = 5;
	memcpy_s(hdr_in.CurrentTaskFile, 8, &reg, 8);
	DWORD readsize = 0;
	BOOL br = DeviceIoControl(m_hdev, IOCTL_ATA_PASS_THROUGH_DIRECT, 
		&hdr_in, boost::numeric_cast<DWORD>(hdr_size), &hdr_in, boost::numeric_cast<DWORD>(hdr_size), 
		&readsize, NULL);
	if (!br)
	{
		jcvos::CWin32Exception err(GetLastError(), _T("failure on ata command "));
		LOG_ERROR(err.WhatT());
	}
	memcpy_s(&reg, 8, hdr_in.CurrentTaskFile, 8);
	return br != 0;
}


///////////////////////////////////////////////////////////////////////////////
// -- ata pass through device

bool CAtaDeviceComm::Detect(void)
{
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;
	bool br = Inquiry(buf);
	return br;
}

bool CAtaPassThroughDevice::AtaCommand(ATA_REGISTER & reg, ATA_PROTOCOL prot, BYTE * buf, size_t secs)
{
	LOG_STACK_TRACE();
	LOG_TRACE(L"Send ata cmd: %02X, %02X, %02X, %02X, %02X, %02X, %02X"
		, reg.feature, reg.sec_count, reg.lba_low, reg.lba_mid, reg.lba_hi, reg.device, reg.command);
	JCASSERT(m_hdev);

	BYTE cb[CDB10GENERIC_LENGTH];
	memset(cb, 0, CDB10GENERIC_LENGTH);

	cb[0] = 0xA1;		// ATA pass throug CDB
	cb[1] = (prot << 1);	// protocol
	cb[2] = (1 << 5) | (1 << 2);	// check_cond=1, block, 
	if (prot != NON_DATA)
	{
		cb[2] |= 0x2;		// length in sector count
	}
	READWRITE rw = write;
	if (prot == PIO_DATA_IN || prot == UDMA_DATA_IN)
	{
		cb[2] |= (1 << 3);	//t_dir=1
		rw = read;
	}
	memcpy_s(cb + 3, 7, &reg, 7);

	// 调用者确保缓存对齐
	BYTE sense_buf[SPT_SENSE_LENGT];
	BYTE status = ScsiCommand(rw, buf, SECTOR_TO_BYTE(secs), cb, CDB10GENERIC_LENGTH, 
		sense_buf, SPT_SENSE_LENGT, 5000);

	memcpy_s(cb, CDB10GENERIC_LENGTH, sense_buf + 8, min(CDB10GENERIC_LENGTH, 14));
	cb[13] |= 0x10;

	reg.error = cb[3];
	reg.sec_count = cb[5];
	reg.lba_low = cb[7];
	reg.lba_mid = cb[9];
	reg.lba_hi = cb[11];
	reg.device = cb[12];
	reg.status = cb[13];

	LOG_NOTICE(L"ATA pass through status = 0x%02X", status);
	LOG_DEBUG(L"Returned ata cmd: %02X, %02X", reg.error, reg.status);
//	if (!br) THROW_WIN32_ERROR(L"failure in invoking ATA PASS THROGH command.");
	return (status == 0);
//	return ((reg.status & 1) == 0);
}

bool CAtaPassThroughDevice::AtaCommand48(
	ATA_REGISTER &reg_pre, ATA_REGISTER &reg_cur, ATA_PROTOCOL prot, BYTE * buf, size_t secs)
{
	JCASSERT(m_hdev);

	BYTE cb[CDB10GENERIC_LENGTH];
	memset(cb, 0, CDB10GENERIC_LENGTH);
	cb[0] = 0x85;		// ATA pass throug CDB
	cb[1] = (prot << 1) | 1;	// protocol, extend command
	cb[2] = (1 << 5) | (1 << 2) | 0x2;		// check_cond=1, block, length in sector count

	READWRITE rw = write;
	if (prot == PIO_DATA_IN || prot == UDMA_DATA_IN)
	{
		cb[2] |= (1 << 3);	//t_dir=1
		rw = read;
	}

	cb[3] = reg_pre.feature;	cb[4] = reg_cur.feature;
	cb[5] = reg_pre.sec_count;	cb[6] = reg_cur.sec_count;
	cb[7] = reg_pre.lba_low;	cb[8] = reg_cur.lba_low;
	cb[9] = reg_pre.lba_mid;	cb[10] = reg_cur.lba_mid;
	cb[11] = reg_pre.lba_hi;		cb[12] = reg_cur.lba_hi;
	cb[13] = reg_cur.device;
	cb[14] = reg_cur.command;

	BYTE sense_buf[SPT_SENSE_LENGT];
	// 调用者确保缓存对齐
	BYTE status = ScsiCommand(rw, buf, SECTOR_TO_BYTE(secs), cb, CDB10GENERIC_LENGTH,
		sense_buf, SPT_SENSE_LENGT,	5000);

	memcpy_s(cb, CDB10GENERIC_LENGTH, sense_buf + 8, min(CDB10GENERIC_LENGTH, 14));
	cb[13] |= 0x10;

	reg_cur.error = cb[3];
	reg_cur.sec_count = cb[5];	reg_pre.sec_count = cb[4];
	reg_cur.lba_low = cb[7];		reg_pre.lba_low = cb[6];
	reg_cur.lba_mid = cb[9];		reg_pre.lba_mid = cb[8];
	reg_cur.lba_hi = cb[11];		reg_pre.lba_hi = cb[10];
	reg_cur.device = cb[12];
	reg_cur.status = cb[13];

	LOG_DEBUG(L"Returned ata cmd: %02X, %02X", reg_cur.error, reg_cur.status);
//	if (!br) THROW_WIN32_ERROR(L"failure in invoking ATA PASS THROGH command.");

//	return ((reg_cur.status & 1) == 0);
	return (status == 0);
}

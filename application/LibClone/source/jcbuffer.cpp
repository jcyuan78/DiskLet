#include "pch.h"

#include "../include/jcbuffer.h"

LOCAL_LOGGER_ENABLE(L"jcbuffer", LOGGER_LEVEL_NOTICE);

JcCmdLet::BinaryType::BinaryType(jcvos::IBinaryBuffer * ibuf)
	: m_data(ibuf)
{
	LOG_STACK_TRACE();
	if (m_data) m_data->AddRef();
}

JcCmdLet::BinaryType::~BinaryType(void)
{
	jcvos::IBinaryBuffer * tmp = m_data; m_data = NULL;
	if (tmp) tmp->Release();
}

JcCmdLet::BinaryType::!BinaryType(void)
{
	jcvos::IBinaryBuffer * tmp = m_data; m_data = NULL;
	if (tmp) tmp->Release();
}

bool JcCmdLet::BinaryType::GetData(void * & data)
{
	JCASSERT(data == NULL);
	if (m_data) m_data->AddRef();
	data = reinterpret_cast<void*>(m_data);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// -- show binary
static const size_t STR_BUF_LEN = 80;
static const size_t ADD_DIGS = 6;
static const size_t HEX_OFFSET = ADD_DIGS + 3;
static const size_t ASCII_OFFSET = HEX_OFFSET + 51;

void _local_itohex(wchar_t * str, size_t dig, UINT d)
{
	size_t ii = dig;
	do
	{
		str[--ii] = jcvos::hex2char(d & 0x0F);
		d >>= 4;
	} while (ii > 0);
}


void JcCmdLet::ShowBinary::ProcessRecord()
{
	LOG_STACK_TRACE();

	jcvos::auto_interface<jcvos::IBinaryBuffer> bdata;
	data->GetData(bdata);	if (!bdata) return;
	LOG_DEBUG(L"data = %08X", bdata);

	size_t src_len = bdata->GetSize();
	size_t show_start = offset * SECTOR_SIZE;
	if (show_start >= src_len) show_start = src_len - SECTOR_SIZE;
	size_t show_end = show_start + secs * SECTOR_SIZE;
	if (show_end > src_len)	show_end = src_len;

	wchar_t		str_buf[STR_BUF_LEN];
	size_t ii = 0;

	// output head line
	wmemset(str_buf, ' ', STR_BUF_LEN);
	str_buf[STR_BUF_LEN - 3] = 0;

	// sector address
	_local_itohex(str_buf, ADD_DIGS, boost::numeric_cast<UINT>(show_start / SECTOR_SIZE));
	for (ii = 0; ii < 16; ++ii)
	{
		LPTSTR _str = str_buf + (ii * 3) + HEX_OFFSET - 1;
		_str[0] = '-';		
		_str[1] = '-';
		_str[2] = jcvos::hex2char((BYTE)(ii & 0xFF));
	}

	System::String ^ str = gcnew System::String(str_buf);
	System::Console::WriteLine(str);

	// output data
	BYTE * val_array = bdata->Lock();
	LOG_DEBUG(L"buf = %08X, data=%02X, %02X", val_array, val_array[0], val_array[1]);
	// 仅支持行对齐
	size_t add = (show_start) & 0xFFFFFF;

	ii = show_start;
	while (ii < show_end)
	{	// loop for line
		wmemset(str_buf, _T(' '), STR_BUF_LEN - 3);
		wmemset(str_buf + ASCII_OFFSET, _T('.'), 16);

		// output address
		_local_itohex(str_buf, ADD_DIGS, boost::numeric_cast<UINT>(add) );
		LPTSTR  str1 = str_buf + HEX_OFFSET;
		LPTSTR	str2 = str_buf + ASCII_OFFSET;

		for (int cc = 0; (cc < 0x10) && (ii < show_end); ++ii, ++cc)
		{
			BYTE dd = val_array[ii];
			_local_itohex(str1, 2, dd);	str1 += 3;
			if ((0x20 <= dd) && (dd < 0x7F))	str2[0] = dd;
			str2++;
		}
		add += 16;
		str = gcnew System::String(str_buf);
		System::Console::WriteLine(str);
	}
}

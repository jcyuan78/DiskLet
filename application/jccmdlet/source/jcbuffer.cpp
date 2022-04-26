#include "pch.h"


#include "../include/jcbuffer.h"
#include "cmdlet-buffer.h"
#include <jcfile.h>
#include <boost\cast.hpp>


LOCAL_LOGGER_ENABLE(L"jcbuffer", LOGGER_LEVEL_NOTICE);

///////////////////////////////////////////////////////////////////////////////
// -- show binary




JcCmdLet::BinaryType::BinaryType(jcvos::IBinaryBuffer * ibuf)
	: m_data(ibuf), m_locked(nullptr)
{
	LOG_STACK_TRACE();
	LOG_DEBUG(L"data = 0x%p", m_data);
	if (m_data) m_data->AddRef();
}

JcCmdLet::BinaryType::~BinaryType(void)
{
	LOG_STACK_TRACE();
	CleanData();
}

JcCmdLet::BinaryType::!BinaryType(void)
{
	LOG_STACK_TRACE();
	CleanData();
}

BYTE* JcCmdLet::BinaryType::LockData(void)
{
	JCASSERT(m_data && m_locked == nullptr);
	m_locked = m_data->Lock();
	return m_locked;
}

void JcCmdLet::BinaryType::Unlock(void)
{
	if (m_locked) m_data->Unlock(m_locked);
	m_locked = nullptr;
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


void JcCmdLet::ShowBinary::InternalProcessRecord()
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
	// 仅支持行对苼E
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

void JcCmdLet::ImportBinary::InternalProcessRecord()
{
	std::wstring str_fn;
	ToStdString(str_fn, fn);
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf;

	bool br = false;
	if (Mapping.ToBool())
	{
		jcvos::auto_interface<jcvos::IFileMapping> file;
		br = jcvos::CreateFileMappingObject(file, str_fn, GENERIC_READ);
		br = jcvos::CreateFileMappingBuf(file, 0, 0, buf);
	}
	else
	{
		br = jcvos::LoadBinaryFromFile(buf, str_fn);
	}
	if (!br || !buf) gcnew ApplicationException(L"failed loading binary");
	BinaryType ^ val = gcnew BinaryType(buf);
	WriteObject(val);
}

void JcCmdLet::ExportBinary::InternalProcessRecord()
{
	LOG_STACK_TRACE();
	JCASSERT(m_file);

	if (!data) return;
	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf;
	data->GetData(ibuf);	JCASSERT(ibuf);
	BYTE * _data = ibuf->Lock();
	for (size_t ii = 0; ii < ibuf->GetSize(); ++ii)
	{
		m_file->WriteByte(_data[ii]);
	}
	ibuf->Unlock(_data);
}

void JcCmdLet::SelectBinary::InternalProcessRecord()
{
	bool br;
	if (!data) return;
	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf;
	data->GetData(ibuf);
	if (!ibuf) return;
	jcvos::auto_interface<jcvos::IBinaryBuffer> ipartial;
	br = jcvos::CreatePartialBuffer(ipartial, ibuf, offset, secs);
	if (!br || !ipartial) throw gcnew System::ApplicationException(L"failed on creating partial buffer");

	JcCmdLet::BinaryType ^ val = gcnew JcCmdLet::BinaryType(ipartial);
	WriteObject(val);
}

void JcCmdLet::ConvertArrayToBinary::InternalProcessRecord()
{
	if (!DataIn) return;

	size_t input_len = DataIn->Length;
	size_t new_len = m_len + input_len;
	System::Array::Resize(m_buf, boost::numeric_cast<int>(new_len) );
	DataIn->CopyTo(m_buf, (int)m_len);
	m_len = new_len;
}

void JcCmdLet::ConvertArrayToBinary::EndProcessing()
{
	if (m_len)
	{
		jcvos::IBinaryBuffer * ibuf = NULL;
		jcvos::CreateBinaryBuffer(ibuf, m_len);

		System::Runtime::InteropServices::GCHandle hobject =
			System::Runtime::InteropServices::GCHandle::Alloc(m_buf,
				System::Runtime::InteropServices::GCHandleType::Pinned);
		const BYTE * src = (const BYTE*)(hobject.AddrOfPinnedObject().ToPointer());

		BYTE * buf = ibuf->Lock();
		memcpy_s(buf, m_len, src, m_len);
		ibuf->Unlock(buf);
		hobject.Free();

		JcCmdLet::BinaryType ^ val = gcnew JcCmdLet::BinaryType(ibuf);
		ibuf->Release();
		WriteObject(val);
	}
}

void JcCmdLet::ConvertBinaryToArray::InternalProcessRecord()
{
	if (!Data) gcnew System::ApplicationException(L"missing input data");
	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf;
	Data->GetData(ibuf);
	if (!ibuf) gcnew System::ApplicationException(L"input data does not contain binary data");
	int len = boost::numeric_cast<int>(ibuf->GetSize());

	System::Array ^ val;
	switch (WordLen)
	{
	case 1:	val = gcnew array<BYTE>(len); break;
	case 2:	val = gcnew array<WORD>((len + 1) / 2); break;	// round up
	case 4:	val = gcnew array<DWORD>((len + 3) / 4); break;
	case 8:	val = gcnew array<UINT64>((len + 7) / 8); break;
	default: {
		System::String ^ msg = System::String::Format(L"illeagle word length={0}", WordLen);
		throw gcnew System::ApplicationException(msg);
	}
	}

	System::Runtime::InteropServices::GCHandle hobject =
		System::Runtime::InteropServices::GCHandle::Alloc(val,
			System::Runtime::InteropServices::GCHandleType::Pinned);
	void * dst = (hobject.AddrOfPinnedObject().ToPointer());

	BYTE * src = ibuf->Lock();
	memcpy_s(dst, len, src, len);
	ibuf->Unlock(src);
	hobject.Free();

	WriteObject(val);
}

BYTE * JcCmdLet::CompareBinary::CheckData(BinaryType ^ data, jcvos::IBinaryBuffer *& ibuf, size_t & len)
{
	JCASSERT(ibuf == nullptr);
	size_t cmp_len = SECTOR_TO_BYTE(Secs);
	if (!data) throw gcnew System::ApplicationException(L"missing data");
	data->GetData(ibuf);
	if (!ibuf) throw gcnew System::ApplicationException(L"no binary data in data");
	len = ibuf->GetSize();
	if (len < cmp_len) throw gcnew System::ApplicationException(L"data is less then secs");
	return ibuf->Lock();
}

void JcCmdLet::CompareBinary::InternalProcessRecord()
{
	size_t cmp_len = SECTOR_TO_BYTE(Secs);
	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf1;
	size_t len1;
	BYTE * buf1 = CheckData(Data1, ibuf1, len1);

	jcvos::auto_interface<jcvos::IBinaryBuffer> ibuf2;
	size_t len2;
	BYTE * buf2 = CheckData(Data2, ibuf2, len2);

	int ir = memcmp(buf1, buf2, cmp_len);
	ibuf1->Unlock(buf1);
	ibuf2->Unlock(buf2);
	WriteObject(ir);
}

void JcCmdLet::JoinBinary::InternalProcessRecord()
{
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf1;
	Data1->GetData(buf1);
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf2;
	Data2->GetData(buf2);

	size_t size1 = buf1->GetSize();
	size_t total_size = size1 + buf2->GetSize();
	jcvos::auto_interface<jcvos::IBinaryBuffer> buf3;
	jcvos::CreateBinaryBuffer(buf3,total_size);
	BYTE* d3 = buf3->Lock();

	BYTE* d1 = buf1->Lock();
	memcpy_s(d3, total_size, d1, size1);
	buf1->Unlock(d1);
	d1 = buf2->Lock();
	memcpy_s(d3 + size1, total_size - size1, d1, buf2->GetSize());
	buf2->Unlock(d1);
	buf3->Unlock(d3);

	Data1->SetData(buf3);
	WriteObject(Data1);
}

void JcCmdLet::JcCmdletBase::ProcessRecord()
{
	try
	{
		InternalProcessRecord();
	}
	catch (std::exception & err)
	{
		std::wstring err_msg;
		jcvos::Utf8ToUnicode(err_msg, err.what());
		LOG_ERROR(L"[err] %s", err_msg.c_str());
//				System::String ^ msg = gcnew System::String(err.what());
		System::Exception ^ exp = gcnew PipelineStoppedException( System::String::Format(L"[err] {0}", 
			gcnew System::String(err_msg.c_str())) );
		ErrorRecord ^er = gcnew	ErrorRecord(exp, L"stderr", ErrorCategory::FromStdErr, this);
		WriteError(er);
	}
//			ShowPipeMessage();
}


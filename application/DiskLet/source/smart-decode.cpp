#include "pch.h"
#include <stdext.h>
#include <memory.h>
#include "smart-decode.h"

//LOCAL_LOGGER_ENABLE(_T("smart-decode"), LOGGER_LEVEL_WARNING);
static CJCLoggerNode * _local_logger=NULL;


size_t SmartDecodeById(
			CSmartAttribute * attri,	// [output] 调用者确保空间
			size_t attri_size,			// attri的大小，
			BYTE * buf,					// [input]	SMART的512 byte 缓存
			size_t buf_size				// 缓存大小，字节单位
			)
{
	if (buf_size < SECTOR_SIZE) ERROR_TERM_(return 0, L"data buffer is empty");

	size_t attri_index=0, out_index=0;
	size_t offset=2;

	for (attri_index = 0; 
		(attri_index < MAX_SMART_ATTRI_NUM) && (out_index < attri_size);
		++attri_index, offset +=ATTRIBUTE_SIZE)
	{
		BYTE * attri_buf = buf+offset;
		if (attri_buf[0] !=0)
		{
			CSmartAttribute * _a = attri + out_index;
			memcpy_s(_a, sizeof(CSmartAttribute), attri_buf, ATTRIBUTE_SIZE);
			out_index++;
		}
	}
	return out_index;
}

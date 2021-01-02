#pragma once

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned __int64	UINT64;

#pragma pack (1)	// 解除对齐
class CSmartAttribute
{
public:
	BYTE	m_id;
	WORD	m_flags;
	BYTE	m_val;
	BYTE	m_th;
	union
	{
		UINT64	m_raw_val;
		BYTE	m_raw[7];
	};
};
#pragma pack ()	

#define ATTRIBUTE_SIZE		(12)
#define MAX_SMART_ATTRI_NUM	(360 / ATTRIBUTE_SIZE)

// 根据SMART的attribute id来解析smart
//	返回解析到的smart attribute个数
size_t SmartDecodeById(
			CSmartAttribute * attri,	// [output] 调用者确保空间
			size_t attri_size,			// attri的大小，
			BYTE * buf,					// [input]	SMART的512 byte 缓存
			size_t buf_size				// 缓存大小，字节单位
			);


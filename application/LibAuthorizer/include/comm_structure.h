#pragma once

#include <boost/property_tree/xml_parser.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/cast.hpp>

namespace Authority
{
#define DELIVERY_LEN 16
	typedef BYTE DELIVERY_KEY[DELIVERY_LEN];

	class DELIVERY
	{
	public:
		DWORD m_app_id;
		DWORD m_sn;
		DWORD m_hash;
		DWORD m_crc;
	};

	#define LICENSE_BLOCK_SIZE	16

	enum LICENSE_TYPE
	{
		LT_ENDOF_LICENSE = 0xFF, LT_HEADER = 1, LT_DELIVERY = 2, LT_DRIVE = 3, LT_INTERNAL = 4,
	};

	template <class T>
	BYTE GetLicenseSize(void)
	{
		size_t ss = sizeof(T);
		size_t blocks = (ss - 1) / LICENSE_BLOCK_SIZE + 1;
		return (BYTE)blocks;
	}

	// License Set���ݽṹ��
	//  License Set��һ������
	class LICENSE_HEADER
	{
	public:
		DWORD m_crc;
		DWORD m_pre_crc;	// ǰһ��header��crc
		DWORD m_app_id;
		WORD m_reserved;	// ���е�serial number, obsoluted
		BYTE m_type;
		BYTE m_size;		// block (16 byte��Ϊ��λ
	public:
		DWORD CalculateCrc(BYTE size);	//size Ϊblock������һ��block 16 byte
	//	static LICENSE_TYPE type;
	};

	//#if sizeof(LICENSE_HEADER) != 16
	//#error


	// ������ɣ����ÿ�����е����
	#define LICENSE_PERIOD_INFINITE		(0xFAFBFCFD)
	#define COMPUTER_NAME_IGNOR			(0xEFEDECEBEAE9E8E7)
	#define DELIVERY_VERSION_INTERNAL	(1)
	#define DELIVERY_VERSION_NORMAL		(2)
	class DELIVERY_LICENSE : public LICENSE_HEADER
	{
	public:
		UINT64	m_computer_name;
		DWORD	m_version;
		DWORD	m_master_id;
		UINT	m_period;
		WORD	m_serial_num;		// ����license��sn
		boost::gregorian::date m_start_date;
		DELIVERY_KEY m_delivery;
		static BYTE GetSize(void) {
			return GetLicenseSize<DELIVERY_LICENSE>(); 	}
	};

	// �豸��ɣ����ʹ�õ��豸����
	class DRIVE_LICENSE : public LICENSE_HEADER
	{
	public:
		char m_drives[1];
		BYTE GetSize(void)
		{
			size_t size = 0;
			char* ch = m_drives;
			while (*ch)	
			{
				size_t len = strlen(ch);
				ch += (len+1);
				size += (len + 1);
			}
			// size��Ҫ+1, ��ȡ����blockʱ��sizeҪ-1���໥����;
			BYTE blocks = boost::numeric_cast<BYTE>((size / LICENSE_BLOCK_SIZE) + 1);
			blocks++;	// ����LICENSE_HEADER�Ĵ�С
			return blocks;
		}
	};
}
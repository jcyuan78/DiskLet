#include "pch.h"
#include "..\include\l0discovery.h"
#include <boost/endian.hpp>
#include <boost/cast.hpp>

LOCAL_LOGGER_ENABLE(L"tcg.l0discovery", LOGGER_LEVEL_NOTICE);


CL0DiscoveryDescription::~CL0DiscoveryDescription(void)
{
	Clear();
}

void CL0DiscoveryDescription::Clear(void)
{
	for (auto it = m_descriptions.begin(); it != m_descriptions.end(); ++it)
	{
		delete (it->second);
	}
	m_descriptions.clear();
}

bool CL0DiscoveryDescription::LoadConfig(const std::wstring& config_fn)
{
	// 如果没有指定config_fn，则读取和模块相同路径的配置文件
	std::string fn;
	if (config_fn.empty())
	{
		jcvos::auto_array<wchar_t> str_path(MAX_PATH);
//		GetModuleFileName(NULL, str_path, MAX_PATH);
		GetCurrentDirectory(MAX_PATH, str_path);
		wcscat_s(str_path, MAX_PATH, L"\\l0discovery.json");
//		wchar_t * _path = wcsrchr(str_path, '\\');
//		if (_path) wcscpy_s(_path + 1, (MAX_PATH - (_path - (wchar_t*)str_path)), L"l0discovery.json");
		jcvos::UnicodeToUtf8(fn, (wchar_t*)str_path);
	}
	else 	jcvos::UnicodeToUtf8(fn, config_fn);

	Clear();

	boost::property_tree::wptree pt;
	boost::property_tree::read_json(fn, pt);
	const boost::property_tree::wptree & desc_pt = pt.get_child(L"Features");
	for (auto pp = desc_pt.begin(); pp != desc_pt.end(); pp++)
	{
		const boost::property_tree::wptree & feature_pt = pp->second;
		CFeatureDescription* fd = new CFeatureDescription;
		const std::wstring& str_code = feature_pt.get<std::wstring>(L"FeatureCode");
		DWORD code = 0;
		swscanf_s(str_code.c_str(), L"0x%X", &code);
		fd->m_feature_code = boost::numeric_cast<WORD>(code);
		fd->m_feature_name = feature_pt.get<std::wstring>(L"FeatureName");

		const boost::property_tree::wptree& fields_pt = feature_pt.get_child(L"Fields");
		for (auto fp = fields_pt.begin(); fp != fields_pt.end(); ++fp)
		{
			const boost::property_tree::wptree & field_pt = fp->second;
			CFieldDescription field;
			field.m_field_name = field_pt.get<std::wstring>(L"FieldName");
			field.m_start_byte = field_pt.get<int>(L"StartByte");
			field.m_byte_num = field_pt.get<int>(L"ByteNumber");
			// 如果没有定义start_bit/bit_num，则数据以byte为单位
			field.m_start_bit = field_pt.get<int>(L"StartBit", -1);
			field.m_bit_num = field_pt.get<int>(L"BitNumber", -1);
			fd->m_fields.push_back(field);
		}

		m_descriptions.insert(std::make_pair(fd->m_feature_code, fd));
	}

	return true;
}

bool CL0DiscoveryDescription::Parse(CTcgFeatureSet& feature_set, BYTE* buf, size_t buf_len)
{
	if (buf_len < sizeof(L0DISCOVERY_HEADER)) 
		THROW_ERROR(ERR_APP, L"buffer length (%zd) is less than header length (%zd)", sizeof(L0DISCOVERY_HEADER));
	memcpy_s(&feature_set.m_header, sizeof(L0DISCOVERY_HEADER), buf, sizeof(L0DISCOVERY_HEADER));

	boost::endian::endian_reverse_inplace(feature_set.m_header.length_of_parameter);
	boost::endian::endian_reverse_inplace(feature_set.m_header.major_version);
	boost::endian::endian_reverse_inplace(feature_set.m_header.minor_version);

	if (feature_set.m_header.length_of_parameter > buf_len)
		THROW_ERROR(ERR_APP, L"length in header is invalid, length=%zd, buffer size=%zd", 
			feature_set.m_header.length_of_parameter, buf_len);

	buf += sizeof(L0DISCOVERY_HEADER);
	buf_len -= sizeof(L0DISCOVERY_HEADER);

	size_t consumed = sizeof(L0DISCOVERY_HEADER);

	while (consumed < feature_set.m_header.length_of_parameter)
	{
		L0DISCOVERY_FEATURE_BASE * ff = reinterpret_cast<L0DISCOVERY_FEATURE_BASE*>(buf);
		CTcgFeature feature;
		feature.m_code = boost::endian::endian_reverse(ff->feature_code);
		if (feature.m_code == 0) continue;
		feature.m_length = ff->length;
		feature.m_version = (ff->version >> 4);
		if (buf_len < feature.m_length)
		{
			THROW_ERROR(ERR_APP, L"invalid length in feature(0x%02X): length=%d, buffer size=%d",
				feature.m_code, feature.m_length, buf_len);
		}
		size_t data_size = feature.m_length;
		if (data_size > MAX_FEATURE_SIZE)
		{
			LOG_WARNING(L"[warning] data size (%zd) is larger than buffer size (%d)", data_size, MAX_FEATURE_SIZE);
			memcpy_s(feature.m_raw_data, MAX_FEATURE_SIZE, ff->data, MAX_FEATURE_SIZE);
		}
		else 	memcpy_s(feature.m_raw_data, MAX_FEATURE_SIZE, ff->data, data_size);

		auto it_f = m_descriptions.find(feature.m_code);
		if (it_f == m_descriptions.end())
		{
			wchar_t str[32];
			swprintf_s(str, L"Feature_%04X", feature.m_code);
			feature.m_name = str;
		}
		else
		{
			CFeatureDescription* desc = it_f->second;
			feature.m_name = desc->m_feature_name;
			for (auto it = desc->m_fields.begin(); it != desc->m_fields.end(); ++it)
			{
				UINT64 data=0;
				BYTE* _dd = (BYTE*)(&data);
				CFieldDescription& fd = *it;
//				memcpy_s(_dd, 8, buf + fd.m_start_byte, fd.m_byte_num);
				for (size_t ii = 0; ii < fd.m_byte_num; ++ii) _dd[fd.m_byte_num - ii - 1] = buf[fd.m_start_byte + ii];
				if ((fd.m_start_bit >= 0) && (fd.m_bit_num > 0))
				{	// 提取bit
					UINT64 mask = 1;
					mask <<= (fd.m_bit_num);
					mask -= 1;
					data >>= (fd.m_start_bit);
					data &= mask;
				}
				feature.m_features.add(fd.m_field_name, data);
			}
		}
		data_size += 4;
		consumed += data_size;
		buf += data_size;
		buf_len -= data_size;
		feature_set.m_features.push_back(feature);
	}
	return true;
}

const CTcgFeature * CTcgFeatureSet::GetFeature(CTcgFeature::TCG_FEATURE_CODE code)
{
	for (auto it = m_features.begin(); it != m_features.end(); ++it)
	{
		if (it->m_code == code) return &(*it);
	}
	return NULL;
}

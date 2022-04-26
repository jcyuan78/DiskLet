///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "pch.h"

#include "l0discovery.h"

LOCAL_LOGGER_ENABLE(L"l0discovery", LOGGER_LEVEL_NOTICE);

using namespace tcg_parser;

L0Discovery::L0Discovery(BYTE * buf, size_t buf_len)
{
    L0DISCOVERY_HEADER* header = reinterpret_cast<L0DISCOVERY_HEADER*>(buf);
	length = endian_reverse(header->length_of_parameter);
    if (length > buf_len)
        THROW_ERROR(ERR_APP, L"length in header is invalid, length=%d, buffer size=%d", length, buf_len);
	ver_major = endian_reverse(header->major_version);
	ver_minor = endian_reverse(header->minor_version);

    BYTE * ptr = header->features;
    size_t offset = ptr - buf;

    while (offset < length)
    {
//        L0DISCOVERY_FEATURE * ff = reinterpret_cast<L0DISCOVERY_FEATURE*>(ptr);
        TcgFeatureBase ^ ff = CreateFeature(ptr, buf_len - offset);
        if (ff->length > buf_len - offset)
            THROW_ERROR(ERR_APP, L"invalid length in feature(0x%02X): length=%d, buffer size=%d",
                ff->feature_code, ff->length, (buf_len - offset));
//        features->push_back(ff);
        features.Add(ff);
        ptr += (ff->length + 4);
        offset = ptr - buf;
    }
}


TcgFeatureBase ^ L0Discovery::CreateFeature(BYTE * & buf, size_t buf_len)
{
    L0DISCOVERY_FEATURE * ff = reinterpret_cast<L0DISCOVERY_FEATURE*>(buf);
    WORD feature_code = endian_reverse(ff->feature_code);

    TcgFeatureBase ^ feature = nullptr;
    switch (feature_code)
    {
    case FAETURE_TPER:                  feature = gcnew TcgFeatureTper(ff); break;
    case FEATURE_LOCKING:               feature = gcnew TcgFeatureLocking(ff); break;
    case FEATURE_GEOMETRY_REPORTING:    feature = gcnew TcgFeatureGeometry(ff); break;
    case FEATURE_SINGLE_USER_MODE:      feature = gcnew TcgFeatureSingleUser(ff); break;
    case FEATURE_DATASTORE_TABLE:       feature = gcnew TcgFeatureDatastoreTable(ff); break;
    case FEATURE_OPAL_SSC:              feature = gcnew TcgFeatureOpalSSC(ff); break;

    default: feature = gcnew TcgFeatureBase(ff); break;
    }


    return feature;

}
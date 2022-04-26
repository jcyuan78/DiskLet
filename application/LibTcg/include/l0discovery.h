#pragma once

#include "itcg.h"
#include <boost/property_tree/json_parser.hpp>
#include <vector>
#include <map>

#define MAX_FEATURE_SIZE    128

class L0DISCOVERY_HEADER
{
public:
    DWORD length_of_parameter;
    WORD major_version;
    WORD minor_version;
    BYTE reserved[8];
    BYTE vendor[32];
    //BYTE features[1];
};

class L0DISCOVERY_FEATURE_BASE
{
public:
    WORD feature_code;
    BYTE version;
    BYTE length;
    BYTE data[1];
};

class CTcgFeature
{
public:
    enum TCG_FEATURE_CODE
    {
        FAETURE_TPER =                  0X0001,                    // Architecture Core Spec, 3.3.6.4
        FEATURE_LOCKING =               0X0002,               // Architecture Core Spec, 3.3.6.5
        FEATURE_GEOMETRY_REPORTING =    0x0003,    // Opal SSC, 3.1.1.4
        FEATURE_SECUREMSG=              0x0004,	/* Secure Messaging */
        FEATURE_ENTERPRISE =            0X0100,	/* Enterprise SSC */
        FEATURE_OPALV100   =            0X0200,	/* Opal SSC V1.00 */
        FEATURE_SINGLE_USER_MODE =      0x0201,        // Opal Feature Set Single User Mode, 4.2.1
        FEATURE_DATASTORE_TABLE =       0x0202,       // Opal Feature Set Additinal DataStore Tables, 4.1.1
        FEATURE_OPAL_SSC =              0x0203,              // Opal SSC, 3.1.1.5
        FEATURE_OPALITE    =            0X0301,	/* Opalite SSC */
        FEATURE_PYRITEV100 =            0X0302,	/* Pyrite SSC V1.00 */
        FEATURE_PYRITEV200 =            0X0303,	/* Pyrite SSC V2.00 */
        FEATURE_RUBYV100   =            0X0304,	/* Ruby SSC V1.00 */
        FEATURE_LOCKINGLBA =            0X0401,	/* Locking LBA Ranges Control */
        FEATURE_BLOCKSID   =            0X0402,	/* Block SID Authentication */
        FEATURE_NAMESPACE  =            0X0403,	/* Configurable Namespace Locking*/
        FEATURE_DATAREM    =            0X0404,	/* Supported Data Removal Mechanism */
        FEATURE_NSGEOMETRY =            0X0405,	/* Namespace Geometry Reporting */
    };

public:
    CTcgFeature(void) {};
    ~CTcgFeature(void) {};

public:
    WORD m_code;
    BYTE m_version;
    BYTE m_length;
    std::wstring m_name;
    BYTE m_raw_data[MAX_FEATURE_SIZE];

    boost::property_tree::wptree m_features;
};

class CTcgFeatureSet: public tcg::ISecurityObject
{
public:
    virtual ~CTcgFeatureSet(void) {};
    virtual void ToString(std::wostream& out, UINT layer, int opt);
    virtual void GetPayload(jcvos::IBinaryBuffer*& data, int index) {};
    virtual void GetSubItem(ISecurityObject*& sub_item, const std::wstring& name) {};

    const CTcgFeature & GetFeature(const std::wstring& name);
    const CTcgFeature * GetFeature(CTcgFeature::TCG_FEATURE_CODE code);

public:
    std::vector<CTcgFeature> m_features;
    L0DISCOVERY_HEADER m_header;
};


class CFieldDescription
{
public:
    std::wstring m_field_name;
    int m_start_byte, m_byte_num;
    int m_start_bit, m_bit_num;
};

class CFeatureDescription
{
public:
    WORD m_feature_code;
    std::wstring m_feature_name;
    std::vector<CFieldDescription> m_fields;

};

class CL0DiscoveryDescription
{
public:
    CL0DiscoveryDescription(void) {};
    ~CL0DiscoveryDescription(void);

public:
    void Clear(void);
    bool LoadConfig(const std::wstring& config_fn);
    bool Parse(CTcgFeatureSet& features,  BYTE* buf, size_t buf_len);
//    void Print(void);

protected:
    std::map<WORD, CFeatureDescription*> m_descriptions;
};



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <boost/endian.hpp>
using namespace boost::endian;

class L0DISCOVERY_FEATURE
{
public:
	WORD feature_code;
	BYTE version;
	BYTE length;
	BYTE data[1];
};

class L0DISCOVERY_FEATURE_DATASTORE_TABLE
{
public:
    WORD feature_code;
    BYTE version;
    BYTE length;
    WORD reserved;
    WORD max_datastore;
    DWORD max_total_size;
    DWORD total_size_alignment;
};

class L0DISCOVERY_FEATURE_GEOMETRY_REPORTING
{
public:
    WORD feature_code;
    BYTE version;
    BYTE length;
    BYTE align;
    BYTE reserved[7];

    DWORD logical_block_size;
    UINT64 alignment_granularity;
    UINT64 lowest_aligned_lba;
};


class L0DISCOVERY_FEATURE_OPAL_SSC
{
public:
    WORD feature_code;
    BYTE version;
    BYTE length;
    WORD base_comid;
    WORD num_of_comid;
    BYTE range_crossing;
    // number of locking sp admin authorities, 高字节与低字节。这里没有WORD对其，不能用WORD
    BYTE admin_hi;
    BYTE admin_lo;
    // number of locking sp user authorities, 高字节与低字节。这里没有WORD对其，不能直接用WORD定义
    BYTE user_hi;
    BYTE user_lo;
    BYTE initial_c_pin_sid;
    BYTE behavior_of_c_pin_sid;
};





class L0DISCOVERY_HEADER
{
public:
	DWORD length_of_parameter;
	WORD major_version;
	WORD minor_version;
	BYTE reserved[8];
	BYTE vendor[32];
	BYTE features[1];
};

namespace tcg_parser 
{
    enum TCG_FEATURE_CODE 
    {
        FAETURE_TPER=0X0001,                    // Architecture Core Spec, 3.3.6.4
        FEATURE_LOCKING = 0X0002,               // Architecture Core Spec, 3.3.6.5
        FEATURE_GEOMETRY_REPORTING = 0x0003,    // Opal SSC, 3.1.1.4

        FEATURE_SINGLE_USER_MODE=0x0201,        // Opal Feature Set Single User Mode, 4.2.1
        FEATURE_DATASTORE_TABLE = 0x0202,       // Opal Feature Set Additinal DataStore Tables, 4.1.1
        FEATURE_OPAL_SSC = 0x0203,              // Opal SSC, 3.1.1.5
    };

    
    public ref class TcgFeatureBase : public System::Object
    {
    public:
        TcgFeatureBase(L0DISCOVERY_FEATURE * ff)
        {
            feature_code = endian_reverse(ff->feature_code);
            version = ff->version >> 4;
            length = ff->length;
        }
        virtual System::String^ GetFeatureName(void) { return L"Unknown"; }
    public:
        property System::String^ feature_name 
        {
            System::String^ get(void) { return GetFeatureName(); }
        };
        WORD feature_code;
        BYTE version;
        BYTE length;
    };

    // 
    public ref class TcgFeatureTper : public TcgFeatureBase
    {
    public:
        TcgFeatureTper(L0DISCOVERY_FEATURE * ff) : TcgFeatureBase(ff)
        {
            BYTE data = ff->data[0];
            Sync_Supported = data & 0x01;
            Async_Supported = data & 0x02;
            ACK_ANK_Supported = data & 0x04;
            Buffer_Mgnt_Supported = data & 0x08;
            Streaming_Supported = data & 0x10;
            ComID_Mgnt_Supported = data & 0x40;
        }
        virtual System::String^ GetFeatureName(void) override { return L"Tper Feature"; }

    public:
        bool ComID_Mgnt_Supported;
        bool Streaming_Supported;
        bool Buffer_Mgnt_Supported;
        bool ACK_ANK_Supported;
        bool Async_Supported;
        bool Sync_Supported;
    };

    public ref class TcgFeatureLocking : public TcgFeatureBase
    {
    public:
        TcgFeatureLocking(L0DISCOVERY_FEATURE* ff) : TcgFeatureBase(ff)
        {
            BYTE data = ff->data[0];
            Locking_Supported = data & 0x01;
            Locking_Enabled = data & 0x02;
            Locked = data & 0x04;
            Media_Encryption = data & 0x08;
            MBR_Enabled = data & 0x10;
            MBR_Done = data & 0x20;
        }
        virtual System::String^ GetFeatureName(void) override { return L"Locking Feature"; }

    public:
        bool MBR_Done;
        bool MBR_Enabled;
        bool Media_Encryption;
        bool Locked;
        bool Locking_Enabled;
        bool Locking_Supported;
    };

    public ref class TcgFeatureGeometry : public TcgFeatureBase
    {
    public:
        TcgFeatureGeometry(L0DISCOVERY_FEATURE* ff) : TcgFeatureBase(ff)
        {
            L0DISCOVERY_FEATURE_GEOMETRY_REPORTING* f0 = reinterpret_cast<
                L0DISCOVERY_FEATURE_GEOMETRY_REPORTING*>(ff);
            align = f0->align & 0x01;
            logical_block_size = endian_reverse(f0->logical_block_size);
            alignment_granularity = endian_reverse(f0->alignment_granularity);
            lowest_aligned_lba = endian_reverse(f0->lowest_aligned_lba);
         }
        virtual System::String^ GetFeatureName(void) override { return L"Geometry Reporting Feature"; }

    public:
        bool align;
        DWORD logical_block_size;
        UINT64 alignment_granularity;
        UINT64 lowest_aligned_lba;
    };    

    public ref class TcgFeatureSingleUser : public TcgFeatureBase
    {
    public:
        TcgFeatureSingleUser(L0DISCOVERY_FEATURE* ff) : TcgFeatureBase(ff)
        {
            number_of_locking_objects = MAKELONG(MAKEWORD(ff->data[3], ff->data[2]), 
                MAKEWORD(ff->data[1], ff->data[0]));
            BYTE data = ff->data[4];
            any = data & 0x01;
            all = data & 0x02;
            policy = data & 0x04;
        }
        virtual System::String^ GetFeatureName(void) override { return L"Single User Mode Feature"; }

    public:
        DWORD number_of_locking_objects;
        bool policy;
        bool all;
        bool any;
    };

    public ref class TcgFeatureDatastoreTable : public TcgFeatureBase
    {
    public:
        TcgFeatureDatastoreTable(L0DISCOVERY_FEATURE* ff) : TcgFeatureBase(ff)
        {
            L0DISCOVERY_FEATURE_DATASTORE_TABLE* f0 = reinterpret_cast<L0DISCOVERY_FEATURE_DATASTORE_TABLE*>(ff);
            max_datastore_table = endian_reverse(f0->max_datastore);
            max_total_size = endian_reverse(f0->max_total_size);
            datastore_table_size_alignment = endian_reverse(f0->total_size_alignment);
        }
        virtual System::String^ GetFeatureName(void) override { return L"Datastore Table Feature"; }

    public:
        WORD max_datastore_table;
        DWORD max_total_size;
        DWORD datastore_table_size_alignment;
    };

    public ref class TcgFeatureOpalSSC : public TcgFeatureBase
    {
    public:
        TcgFeatureOpalSSC(L0DISCOVERY_FEATURE* ff) : TcgFeatureBase(ff)
        {
            L0DISCOVERY_FEATURE_OPAL_SSC* f0 = reinterpret_cast<L0DISCOVERY_FEATURE_OPAL_SSC*>(ff);
            base_comid = endian_reverse(f0->base_comid);
            num_of_comid = endian_reverse(f0->num_of_comid);
            range_crossing = f0->range_crossing & 0x01;
            num_of_locking_sp_admin = MAKEWORD(f0->admin_lo, f0->admin_hi);
            num_of_locking_sp_user = MAKEWORD(f0->user_lo, f0->user_hi);
            initial_c_pin_sid = f0->initial_c_pin_sid;
            behavior_of_c_pin_sid = f0->behavior_of_c_pin_sid;
        }
        virtual System::String^ GetFeatureName(void) override { return L"Opal SSC Feature"; }

    public:
        WORD base_comid;
        WORD num_of_comid;
        bool range_crossing;
        WORD num_of_locking_sp_admin;
        WORD num_of_locking_sp_user;
        BYTE initial_c_pin_sid;
        BYTE behavior_of_c_pin_sid;
    };

    public enum class SecurityProtocol
    {
        PROTOCOL_INFO = 0,
        TCG = 1,
        TCG_COMID = 2,
        IEEE = 0xEE,
        ATA_PASSTHROUGH = 0xEF,
    };

    public ref class SecurityPayload : public System::Object
    {
    public:
 
    public:
        BYTE protocol;
        WORD sub_protocol;
        int cmd_id;
        bool in_out;    // true: in /  receive / host <- device; false: out / send / host -> device
    };

	public ref class L0Discovery : public SecurityPayload
	{
	public:
		L0Discovery(BYTE * buf, size_t length);
		~L0Discovery(void) {};
		!L0Discovery(void) {};

    public:
        TcgFeatureBase ^ CreateFeature(BYTE * &buf, size_t buf_len);

	public:
		DWORD length;
		WORD ver_major;
		WORD ver_minor;
//		array<TcgFeatureBase^> features;
        System::Collections::ArrayList features;
	};    

}
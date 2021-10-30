#pragma once

//#import "../PEAutomation.tlb" no_namespace named_guids

namespace pa
{
	enum DECODE_LEVEL
	{
		DECODE_LEVEL_PACKET=0,		//Decoding Level Packet
		DECODE_LEVEL_LINKTRA=1,		//Decoding Level Link Transaction
		DECODE_LEVEL_SPLITTRA=2,	//Decoding Level Split	Transaction
		DECODE_LEVEL_NVMHCI=3,		//Decoding Level NVM	Transaction
		DECODE_LEVEL_PQI=4,			//Decoding Level PQI
		DECODE_LEVEL_AHCI=5,		//Decoding Level AHCI
		DECODE_LEVEL_ATA=6,			//Decoding Level ATA
		DECODE_LEVEL_SOP=7,			//Decoding Level SOP
		DECODE_LEVEL_SCSI=8,		//Decoding Level SCSI
		DECODE_LEVEL_NVMC=9,		//Decoding Level NVM Command
		DECODE_LEVEL_RRAP=10,		//Decoding Level RRAP
		DECODE_LEVEL_MCTP_MSG=11,	//Decoding Level MCTP MSG
		DECODE_LEVEL_MCTP_CMD=12,	//Decoding Level MCTP CMD
		DECODE_LEVEL_LM=13,			//Decoding Level LM
	};

	enum PACKET_TYPE
	{
		PACKET_TYPE_TLP = 0x00,
		PACKET_TYPE_DLLP = 0x01,
		PACKET_TYPE_TS1 = 0x02,
		PACKET_TYPE_TS2 = 0x03,
		PACKET_TYPE_FTS = 0x04,
		PACKET_TYPE_EIOS = 0x05,
		PACKET_TYPE_SKIP = 0x06,
		PACKET_TYPE_PATN = 0x07,
		PACKET_TYPE_EIEOS = 0x08,
		PACKET_TYPE_LINK_EVENT = 0x09,
		PACKET_TYPE_DEBUG = 0x0A,
		PACKET_TYPE_GEN = 0x0B,
		PACKET_TYPE_SDS = 0x0C,
		PACKET_TYPE_EDB = 0x0D,
		PACKET_TYPE_EDS = 0x0E,
		PACKET_TYPE_RRAP = 0x0F,
		PACKET_TYPE_PMUX = 0x10,
		PACKET_TYPE_SMBUS = 0x11,
		PACKET_TYPE_UNKNOWN = 0xFF,
	};

	enum TLP_TYPE
	{
		TLP_MEMORY_READ = 0,
		TLP_MEMORY_READ_LOCKED = 4,
		TLP_MEMORY_WRITE = 2,
		TLP_IO_READ = 8,
		TLP_IO_WRITE = 0xA,
		TLP_CONFIG_READ0 = 0x10,
		TLP_CONFIG_READ1,
		TLP_CONFIG_WRITE0,
		TLP_CONFIG_WRITE1,
		TLP_TCONFIG_READ,
		TLP_TCONFIG_WRITE,
		TLP_MESSAGE,
		TLP_MESSAGE_DATA,
		TLP_COMPLETION = 0x28,
		TLP_COMPLETION_DATA = 0x2A,
		TLP_COMPLETION_LOCKED,
		TLP_COMPLETION_DATA_LOCKED,
		TLP_UNKOWN,
	};

	class IPacket : public IJCInterface
	{
	public:
		virtual void GetPacketData(jcvos::IBinaryBuffer*& buf) = 0;

	};

	class ITrace : public IJCInterface
	{
	public:
		virtual void Close(void) = 0;
		virtual void GetPacket(IPacket *& packet, UINT64 index) =0;
		virtual void ExportTraceToCsv(const std::wstring& out_fn, int level)=0;
		virtual void GetCommandPayload(jcvos::IBinaryBuffer* & buf, double start_time, UINT64 start_addr, 
			UINT data_len, bool receive) = 0;
	};

	class IAnalyzer : public IJCInterface
	{
	public:
		virtual void OpenTrace(ITrace*& trace, const std::wstring& fn) = 0;

	};

	void CreateAnalyzer(IAnalyzer*& aa, bool dll);
	void DestoryAnalyzer(bool dll);

}
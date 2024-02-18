#include "pch.h"

#include "dta_command.h"
#include "../include/dta_structures.h"
#include <boost/endian.hpp>

LOCAL_LOGGER_ENABLE(L"tcg.command", LOGGER_LEVEL_NOTICE);

using namespace std;

DtaCommand::DtaCommand()
{
    LOG_STACK_TRACE();
	cmdbuf = commandbuffer + IO_BUFFER_ALIGNMENT;
	cmdbuf = (uint8_t*)((uintptr_t)cmdbuf & (uintptr_t)~(IO_BUFFER_ALIGNMENT - 1));
}

/* Fill in the header information and format the call */
DtaCommand::DtaCommand(const BYTE InvokingUid[8], const BYTE method[8])
{
    LOG_STACK_TRACE();
	cmdbuf = commandbuffer + IO_BUFFER_ALIGNMENT;
	cmdbuf = (uint8_t*)((uintptr_t)cmdbuf & (uintptr_t)~(IO_BUFFER_ALIGNMENT - 1));
	reset(InvokingUid, method);
}

void
DtaCommand::reset()
{
    LOG_STACK_TRACE();
    memset(cmdbuf, 0, MAX_BUFFER_LENGTH);
    bufferpos = sizeof (OPALHeader);
}

void DtaCommand::reset(const TCG_UID invoking_uid, const TCG_UID method)
{
    LOG_STACK_TRACE();
    reset(); 
    cmdbuf[bufferpos++] = OPAL_TOKEN::CALL;
	addToken(invoking_uid);
	addToken(method);
}


void DtaCommand::addToken(uint64_t number)
{
    int startat = 0;
    //LOG(D1) << "Entering DtaCommand::addToken(uint64_t)";
    LOG_STACK_TRACE();
    if (number < 64) {
        cmdbuf[bufferpos++] = (uint8_t) number & 0x000000000000003f;
    }
    else {
        if (number < 0x100) {
            cmdbuf[bufferpos++] = 0x81;
            startat = 0;
        }
        else if (number < 0x10000) {
            cmdbuf[bufferpos++] = 0x82;
            startat = 1;
        }
        else if (number < 0x100000000) {
            cmdbuf[bufferpos++] = 0x84;
            startat = 3;
        }
        else {
            cmdbuf[bufferpos++] = 0x88;
            startat = 7;
        }
        for (int i = startat; i > -1; i--) {
            cmdbuf[bufferpos++] = (uint8_t) ((number >> (i * 8)) & 0x00000000000000ff);
        }
    }
}

void DtaCommand::addToken(const TCG_UID uid)
{
    LOG_STACK_TRACE();
    cmdbuf[bufferpos++] = OPAL_SHORT_ATOM::BYTESTRING8;
    memcpy_s(&cmdbuf[bufferpos], (MAX_BUFFER_LENGTH - bufferpos), uid, 8);
    bufferpos += 8;
}

void DtaCommand::addTokenByteList(vector<uint8_t> token)
{
    LOG_STACK_TRACE();
    for (uint32_t i = 0; i < token.size(); i++) 
    {
        cmdbuf[bufferpos++] = token[i];
    }
}

void DtaCommand::addToken(const char * bytestring)
{
    LOG_STACK_TRACE();
    uint16_t length = (uint16_t) strlen(bytestring);
    if (length == 0) 
    {   /* null token e.g. default password */
        cmdbuf[bufferpos++] = (uint8_t)0xa1;
        cmdbuf[bufferpos++] = (uint8_t)0x00;
    }
    else if (length < 16) 
    {   /* use tiny atom */
        cmdbuf[bufferpos++] = (uint8_t) length | 0xa0;
    }
    else if (length < 2048) 
    {   /* Use Medium Atom */
        cmdbuf[bufferpos++] = 0xd0 | (uint8_t) ((length >> 8) & 0x07);
        cmdbuf[bufferpos++] = (uint8_t) (length & 0x00ff);
    }
    else 
    {   /* Use Large Atom */
        LOG_ERROR(L"[err] can't send LARGE ATOM size bytestring in 2048 Packet")
		exit(EXIT_FAILURE);
    }
    memcpy(&cmdbuf[bufferpos], bytestring, length);
    bufferpos += length;

}

void DtaCommand::addToken(OPAL_TOKEN token)
{
    //LOG(D1) << "Entering DtaCommand::addToken(OPAL_TOKEN)";
    LOG_STACK_TRACE();
    cmdbuf[bufferpos++] = (uint8_t) token;
}

void
DtaCommand::addToken(OPAL_SHORT_ATOM token)
{
    //LOG(D1) << "Entering DtaCommand::addToken(OPAL_SHORT_ATOM)";
    LOG_STACK_TRACE();
    cmdbuf[bufferpos++] = (uint8_t)token;
}

void
DtaCommand::addToken(OPAL_TINY_ATOM token)
{
    //LOG(D1) << "Entering DtaCommand::addToken(OPAL_TINY_ATOM)";
    LOG_STACK_TRACE();
    cmdbuf[bufferpos++] = (uint8_t) token;
}
//
//void DtaCommand::addToken(OPAL_UID token)
//{
//    LOG_STACK_TRACE();
//    cmdbuf[bufferpos++] = OPAL_SHORT_ATOM::BYTESTRING8;
//    memcpy(&cmdbuf[bufferpos], &OPALUID[token][0], 8);
//    bufferpos += 8;
//}



void
DtaCommand::complete(uint8_t EOD)
{
    //LOG(D1) << "Entering DtaCommand::complete(uint8_t EOD)";
    LOG_STACK_TRACE();
    if (EOD) {
        cmdbuf[bufferpos++] = OPAL_TOKEN::ENDOFDATA;
        cmdbuf[bufferpos++] = OPAL_TOKEN::STARTLIST;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = 0x00;
        cmdbuf[bufferpos++] = OPAL_TOKEN::ENDLIST;
    }
    /* fill in the lengths and add the modulo 4 padding */
    OPALHeader * hdr;
    hdr = (OPALHeader *) cmdbuf;
//    hdr->subpkt.length = SWAP32(bufferpos - (sizeof (OPALHeader)));
    hdr->subpkt.length = boost::endian::endian_reverse<UINT32>(bufferpos - (sizeof(OPALHeader)));
    while (bufferpos % 4 != 0) {
        cmdbuf[bufferpos++] = 0x00;
    }
//    hdr->pkt.length = SWAP32((bufferpos - sizeof (OPALComPacket))
//                             - sizeof (OPALPacket));
    hdr->pkt.length = boost::endian::endian_reverse<UINT32>(bufferpos - sizeof(OPALComPacket) - sizeof(OPALPacket));
//    hdr->cp.length = SWAP32(bufferpos - sizeof (OPALComPacket));
    hdr->cp.length = boost::endian::endian_reverse<UINT32>(bufferpos - sizeof(OPALComPacket));
	if (bufferpos > MAX_BUFFER_LENGTH) {
//		LOG(D1) << " Standard Buffer Overrun " << bufferpos;
        THROW_ERROR(ERR_APP, L"Standard Buffer Overrun %d", bufferpos);
		//exit(EXIT_FAILURE);
	}
}

void DtaCommand::changeInvokingUid(std::vector<uint8_t> Invoker)
{
    //LOG(D1) << "Entering DtaCommand::changeInvokingUid()";
    LOG_STACK_TRACE();
    int offset = sizeof (OPALHeader) + 1; /* bytes 2-9 */
    for (uint32_t i = 0; i < Invoker.size(); i++) 
    {
        cmdbuf[offset + i] = Invoker[i];
    }

}

void DtaCommand::changeInvokingUid(const TCG_UID invoker)
{
    LOG_STACK_TRACE();
    int offset = sizeof (OPALHeader) + 1; /* bytes 2-9 */
    for (uint32_t i = 0; i < sizeof(TCG_UID); i++)  cmdbuf[offset + i] = invoker[i];
}


//void *
//DtaCommand::getCmdBuffer()
//{
//    return cmdbuf;
//}

//void *
//DtaCommand::getRespBuffer()
//{
//    return respbuf;
//}
void
DtaCommand::dumpCommand()
{
	OPALHeader * hdr = (OPALHeader *)cmdbuf;
//	DtaHexDump(cmdbuf, SWAP32(hdr->cp.length) + sizeof(OPALComPacket));
}
//void
//DtaCommand::dumpResponse()
//{
//	OPALHeader *hdr = (OPALHeader *)respbuf;
////	DtaHexDump(respbuf, SWAP32(hdr->cp.length) + sizeof(OPALComPacket));
//}
uint16_t DtaCommand::outputBufferSize() const
{
	if(bufferpos % 512) 	return(((uint16_t)(bufferpos / 512) + 1) * 512);
	else		            return((uint16_t)(bufferpos / 512) * 512);
}
void
DtaCommand::setcomID(uint16_t comID)
{
    LOG_STACK_TRACE();
    OPALHeader * hdr;
    hdr = (OPALHeader *) cmdbuf;
    //LOG(D1) << "Entering DtaCommand::setcomID()";
    hdr->cp.extendedComID[0] = ((comID & 0xff00) >> 8);
    hdr->cp.extendedComID[1] = (comID & 0x00ff);
    hdr->cp.extendedComID[2] = 0x00;
    hdr->cp.extendedComID[3] = 0x00;
}

void
DtaCommand::setTSN(uint32_t TSN)
{
	//LOG(D1) << "Entering DtaCommand::setTSN()";
    LOG_STACK_TRACE();
    OPALHeader * hdr;
    hdr = (OPALHeader *) cmdbuf;
    hdr->pkt.TSN = TSN;
}

void
DtaCommand::setHSN(uint32_t HSN)
{
    LOG_STACK_TRACE();
	//LOG(D1) << "Entering DtaCommand::setHSN()";
    OPALHeader * hdr;
    hdr = (OPALHeader *) cmdbuf;
    hdr->pkt.HSN = HSN;
}

DtaCommand::~DtaCommand()
{
    LOG_STACK_TRACE();
    //LOG(D1) << "Destroying DtaCommand";
}

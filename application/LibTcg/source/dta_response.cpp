#include "pch.h"

#include "dta_response.h"
#include <boost/endian.hpp>
using namespace std;

LOCAL_LOGGER_ENABLE(L"tcg.response", LOGGER_LEVEL_NOTICE);


DtaResponse::DtaResponse()
{
//    LOG(D1) << "Creating  DtaResponse()";
    LOG_STACK_TRACE();
}

DtaResponse::DtaResponse(void * buffer)
{
//    LOG(D1) << "Creating  DtaResponse(buffer)";
    LOG_STACK_TRACE();
    init(buffer);
}

void DtaResponse::init(void * buffer)
{
    //LOG(D1) << "Entering  DtaResponse::init";
    LOG_STACK_TRACE();
    std::vector<uint8_t> bytestring, empty_atom(1, 0xff);
    uint8_t * reply = (uint8_t *) buffer;
    uint32_t cpos = 0;
    uint32_t tokenLength;
    memcpy_s(&m_header, sizeof(OPALHeader), buffer, sizeof (OPALHeader));
    response.clear();
    reply += sizeof (OPALHeader);
    //while (cpos < SWAP32(m_header.subpkt.length)) {
    UINT32 len = boost::endian::endian_reverse(m_header.subpkt.length);
    while (cpos <len)
    {
        bytestring.clear();
        if (!(reply[cpos] & 0x80))          tokenLength = 1;        //tiny atom
        else if (!(reply[cpos] & 0x40))     tokenLength = (reply[cpos] & 0x0f) + 1;                // short atom
        else if (!(reply[cpos] & 0x20))     tokenLength = (((reply[cpos] & 0x07) << 8) | reply[cpos + 1]) + 2;      // medium atom
        else if (!(reply[cpos] & 0x10))     tokenLength = ((reply[cpos + 1] << 16) | (reply[cpos + 2] << 8) | reply[cpos + 3]) + 4;// long atom
        else                                tokenLength = 1;// TOKEN

        for (uint32_t i = 0; i < tokenLength; i++) 
        {
            bytestring.push_back(reply[cpos++]);
        }
		if (bytestring != empty_atom)		response.push_back(bytestring);
    }


}

void DtaResponse::init(BYTE* buffer, size_t len, DWORD protocol, DWORD comid)
{
    if (m_payload)
    {
        delete[] m_payload;
        m_payload = NULL;
    }
    RELEASE(m_result);
    m_payload = new BYTE[len];
    m_data_len = len;
    memcpy_s(m_payload, len, buffer, len);
    m_protocol = protocol;
    m_comid = comid;
    // for my parser
    jcvos::auto_interface<tcg::ISecurityParser> parser;
    tcg::GetSecurityParser(parser);
    parser->ParseSecurityCommand(m_result, m_payload, m_data_len, m_protocol, m_comid, true);

    init(buffer);
}

OPAL_TOKEN DtaResponse::tokenIs(uint32_t tokenNum)
{
//    LOG(D1) << "Entering  DtaResponse::tokenIs";
    LOG_STACK_TRACE();

    if (!(response[tokenNum][0] & 0x80))
    { //tiny atom
        if ((response[tokenNum][0] & 0x40))
            return OPAL_TOKEN::DTA_TOKENID_SINT;
        else
            return OPAL_TOKEN::DTA_TOKENID_UINT;
    }
    else if (!(response[tokenNum][0] & 0x40)) { // short atom
        if ((response[tokenNum][0] & 0x20))
            return OPAL_TOKEN::DTA_TOKENID_BYTESTRING;
        else if ((response[tokenNum][0] & 0x10))
            return OPAL_TOKEN::DTA_TOKENID_SINT;
        else
            return OPAL_TOKEN::DTA_TOKENID_UINT;
    }
    else if (!(response[tokenNum][0] & 0x20)) { // medium atom
        if ((response[tokenNum][0] & 0x10))
            return OPAL_TOKEN::DTA_TOKENID_BYTESTRING;
        else if ((response[tokenNum][0] & 0x08))
            return OPAL_TOKEN::DTA_TOKENID_SINT;
        else
            return OPAL_TOKEN::DTA_TOKENID_UINT;
    }
    else if (!(response[tokenNum][0] & 0x10)) { // long atom
        if ((response[tokenNum][0] & 0x02))
            return OPAL_TOKEN::DTA_TOKENID_BYTESTRING;
        else if ((response[tokenNum][0] & 0x01))
            return OPAL_TOKEN::DTA_TOKENID_SINT;
        else
            return OPAL_TOKEN::DTA_TOKENID_UINT;
    }
    else // TOKEN
        return (OPAL_TOKEN) response[tokenNum][0];
}

uint32_t DtaResponse::getLength(uint32_t tokenNum)
{
    return (uint32_t) response[tokenNum].size();
}

uint64_t DtaResponse::getUint64(uint32_t tokenNum) const
{
//    LOG(D1) << "Entering  DtaResponse::getUint64";
    LOG_STACK_TRACE();

    if (!(response[tokenNum][0] & 0x80))
    { //tiny atom
        if ((response[tokenNum][0] & 0x40)) THROW_ERROR(ERR_APP, L"unsigned int requested for signed tiny atom")
            //     {
            //         LOG(E) << "unsigned int requested for signed tiny atom";
                     //exit(EXIT_FAILURE);
            //     }
        else    return (uint64_t)(response[tokenNum][0] & 0x3f);
    }
    else if (!(response[tokenNum][0] & 0x40))
    { // short atom
        if ((response[tokenNum][0] & 0x10)) THROW_ERROR(ERR_APP, L"unsigned int requested for signed short atom")
            //     {
            //         LOG(E) << "unsigned int requested for signed short atom";
                     //exit(EXIT_FAILURE);
            //     }
        else
        {
            uint64_t whatever = 0;
            if (response[tokenNum].size() > 9) LOG_ERROR(L"[err] UINT64 with greater than 8 bytes")
                //            { LOG(E) << "UINT64 with greater than 8 bytes"; }
                int b = 0;
            for (uint32_t i = (uint32_t)response[tokenNum].size() - 1; i > 0; i--)
            {
                whatever |= ((uint64_t)response[tokenNum][i] << (8 * b));
                b++;
            }
            return whatever;
        }

    }
    else if (!(response[tokenNum][0] & 0x20))  THROW_ERROR(ERR_APP, L"unsigned int requested for medium atom is unsupported")
  //  { // medium atom
  //      LOG(E) << "unsigned int requested for medium atom is unsupported";
		//exit(EXIT_FAILURE);
  //  }
    else if (!(response[tokenNum][0] & 0x10)) THROW_ERROR(ERR_APP, L"unsigned int requested for long atom is unsupported")
  //  { // long atom
  //      LOG(E) << "unsigned int requested for long atom is unsupported";
		//exit(EXIT_FAILURE);
  //  }
    else THROW_ERROR(ERR_APP, L"unsigned int requested for token is unsupported")
  //  { // TOKEN
  //      LOG(E) << "unsigned int requested for token is unsupported";
		//exit(EXIT_FAILURE);
  //  }
}

uint32_t DtaResponse::getUint32(uint32_t tokenNum)
{
//    LOG(D1) << "Entering  DtaResponse::getUint32";
    LOG_STACK_TRACE();

    uint64_t i = getUint64(tokenNum);
    if (i > 0xffffffff) LOG_ERROR(L"[err] UINT32 truncated"); // { LOG(E) << "UINT32 truncated "; }
    return (uint32_t) i;

}

uint16_t DtaResponse::getUint16(uint32_t tokenNum)
{
//    LOG(D1) << "Entering  DtaResponse::getUint16";
    LOG_STACK_TRACE();

    uint64_t i = getUint64(tokenNum);
    if (i > 0xffff) LOG_ERROR(L"[err] UINT16 truncated"); //{ LOG(E) << "UINT16 truncated "; }
    return (uint16_t) i;
}

uint8_t DtaResponse::getUint8(uint32_t tokenNum) const
{
    LOG_STACK_TRACE();
    uint64_t i = getUint64(tokenNum);
    if (i > 0xff) LOG_ERROR(L"[err] UINT8 truncated"); //{ LOG(E) << "UINT8 truncated "; }
    return (uint8_t) i;
}
//int64_t DtaResponse::getSint(uint32_t tokenNum) {
//	LOG(E) << "DtaResponse::getSint() is not implemented";
//}

std::vector<uint8_t> DtaResponse::getRawToken(uint32_t tokenNum)
{
    return response[tokenNum];
}

std::string DtaResponse::getString(uint32_t tokenNum)
{
//    LOG(D1) << "Entering  DtaResponse::getString";
    LOG_STACK_TRACE();

    std::string s;
    s.erase();
    int overhead = 0;
    if (!(response[tokenNum][0] & 0x80)) THROW_ERROR(ERR_APP, L"Cannot get a string from a tiny atom")
  //  { //tiny atom
  //      LOG(E) << "Cannot get a string from a tiny atom";
		//exit(EXIT_FAILURE);
  //  }
    else if (!(response[tokenNum][0] & 0x40))   overhead = 1;       // short atom
    else if (!(response[tokenNum][0] & 0x20))   overhead = 2;       // medium atom
    else if (!(response[tokenNum][0] & 0x10))   overhead = 4;   // long atom
    else 
    {
//        LOG(E) << "Cannot get a string from a TOKEN";
        LOG_ERROR(L"[err] Cannot get a string from a TOKEN")
        return s;
    }
    for (uint32_t i = overhead; i < response[tokenNum].size(); i++) s.push_back(response[tokenNum][i]);
    return s;
}

void DtaResponse::getBytes(uint32_t tokenNum, uint8_t bytearray[])
{
//    LOG(D1) << "Entering  DtaResponse::getBytes";
    LOG_STACK_TRACE();

    int overhead = 0;
    if (!(response[tokenNum][0] & 0x80)) THROW_ERROR(ERR_APP, L"Cannot get a bytestring from a tiny atom")
  //  { //tiny atom
  //      LOG(E) << "Cannot get a bytestring from a tiny atom";
		//exit(EXIT_FAILURE);
  //  }
    else if (!(response[tokenNum][0] & 0x40))  overhead = 1; // short atom
    else if (!(response[tokenNum][0] & 0x20))  overhead = 2; // medium atom
    else if (!(response[tokenNum][0] & 0x10))  overhead = 4; // long atom
    else 
    {
//        LOG(E) << "Cannot get a bytestring from a TOKEN";
        LOG_ERROR(L"Cannot get a bytestring from a TOKEN")
		exit(EXIT_FAILURE);
    }

    for (uint32_t i = overhead; i < response[tokenNum].size(); i++) 
    {
        bytearray[i - overhead] = response[tokenNum][i];
    }
}

const wchar_t* DtaResponse::MethodStatusCodeToString(BYTE status)
{
    switch (status) 
    {
    case OPALSTATUSCODE::AUTHORITY_LOCKED_OUT:  return L"AUTHORITY_LOCKED_OUT";
    case OPALSTATUSCODE::FAIL:                  return L"FAIL";
    case OPALSTATUSCODE::INSUFFICIENT_ROWS:     return L"INSUFFICIENT_ROWS";
    case OPALSTATUSCODE::INSUFFICIENT_SPACE:    return L"INSUFFICIENT_SPACE";
    case OPALSTATUSCODE::INVALID_FUNCTION:      return L"INVALID_FUNCTION";
    case OPALSTATUSCODE::INVALID_PARAMETER:     return L"INVALID_PARAMETER";
    case OPALSTATUSCODE::INVALID_REFERENCE:     return L"INVALID_REFERENCE";
    case OPALSTATUSCODE::NOT_AUTHORIZED:        return L"NOT_AUTHORIZED";
    case OPALSTATUSCODE::NO_SESSIONS_AVAILABLE: return L"NO_SESSIONS_AVAILABLE";
    case OPALSTATUSCODE::RESPONSE_OVERFLOW:     return L"RESPONSE_OVERFLOW";
    case OPALSTATUSCODE::SP_BUSY:               return L"SP_BUSY";
    case OPALSTATUSCODE::SP_DISABLED:           return L"SP_DISABLED";
    case OPALSTATUSCODE::SP_FAILED:             return L"SP_FAILED";
    case OPALSTATUSCODE::SP_FROZEN:             return L"SP_FROZEN";
    case OPALSTATUSCODE::SUCCESS:               return L"SUCCESS";
    case OPALSTATUSCODE::TPER_MALFUNCTION:      return L"TPER_MALFUNCTION";
    case OPALSTATUSCODE::TRANSACTION_FAILURE:   return L"TRANSACTION_FAILURE";
    case OPALSTATUSCODE::UNIQUENESS_CONFLICT:   return L"UNIQUENESS_CONFLICT";
    default:                                    return L"Unknown status code";
    }
}

WORD DtaResponse::getStatusCode(void)
{
    if (!m_result) return 0xEFF;
    jcvos::auto_interface<tcg::ISecurityObject> token;
    m_result->GetSubItem(token, L"token");
    if (!token) return 0xEFE;
    jcvos::auto_interface<tcg::ISecurityObject> state;
    token->GetSubItem(state, L"state");
    if (!state) return 0xEFD;
    CStatePhrase* ss = state.d_cast<CStatePhrase*>();
    if (!ss) return 0xEFC;
    return ss->getState();
 }

void DtaResponse::GetResToken(tcg::ISecurityObject*& res)
{
    JCASSERT(res == NULL);
    if (!m_result) return;
    m_result->GetSubItem(res, L"token");
}

uint32_t DtaResponse::getTokenCount() const
{
    LOG_STACK_TRACE();
    return (uint32_t) response.size();
}

DtaResponse::~DtaResponse()
{
//    LOG(D1) << "Destroying DtaResponse";
    LOG_STACK_TRACE();
 //   RELEASE(m_res_obj);
    delete[] m_payload;
    RELEASE(m_result);
}

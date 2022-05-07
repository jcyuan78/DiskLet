#pragma once

#include <vector>
#include "../include/dta_lexicon.h"
#include "../include/tcg_token.h"

class DtaDevOpal;
class DtaDevEnterprise;

using namespace std;

/** A class to build & send Opal Command streams to a TPer.
* This class attempts to closely mimic the command
* pseudo code used in the TCG documents, the syntactic
* sugar is not represented.  See TCG document Storage Architecture
* Core Specification R2.00 V2.00 Section 3.2.1.2 for all
* the gory details.
*
* @see DtaLexicon.h for structs, typedefs and enums used to encode
* the bytestream.
*/
class DtaCommand 
{
	//friend class DtaDevOpal;
	//friend class DtaDevEnterprise;
public:
    /** Default constructor, allocates the command and resonse buffers. */
    DtaCommand();
    /** Constructor that initializes the incokingUid and method fields. 
     *   @param InvokingUid  The UID used to call the SSC method 
     *   @param method The SSC method to be called
     */
//    DtaCommand(OPAL_UID InvokingUid, OPAL_METHOD method);
    DtaCommand(const BYTE InvokingUid[8], const BYTE method[8]);
    /** destructor  frees the command and response buffers */
    ~DtaCommand();
    /** Add a Token to the bytstream of type OPAL_TOKEN. */ 
    void addToken(OPAL_TOKEN token);
    /** Add a Token to the bytstream of type OPL_SHORT ATOM. */
    void addToken(OPAL_SHORT_ATOM token);
    /** Add a Token to the bytstream of type OPL_TINY ATOM. */
    void addToken(OPAL_TINY_ATOM token);
    /** Add a Token to the bytstream of from the OPALUID array. */
    //void addToken(OPAL_UID token);
    /** Add a Token to the bytstream of type c-string */
    void addToken(const char * bytestring);
    /** Add a Token to the bytstream of type vector<uint8_t>.
     * This token must be a complete token properly encoded
     * with the proper TCG bytestream header information  */
    void addTokenByteList(std::vector<uint8_t> token);
    /** Add a Token to the bytstream of type uint64. */
    void addToken(uint64_t number);

    void addToken(const TCG_UID uid);

    template <class N, class T>
    void addNameToken(const N& name, const T& value)
    {
        addToken(OPAL_TOKEN::STARTNAME);
        addToken(name);
        addToken(value);
        addToken(OPAL_TOKEN::ENDNAME);
    }

    void addOptionalParam(UINT param_id, CTcgTokenBase* param);
    void makeOptionalParam(void);



    /** Set the commid to be used in the command. */
    void setcomID(uint16_t comID);
    /** set the Host session number to be used in the command. */
    void setHSN(uint32_t HSN);
    /** Set the TPer session number to be used for the command. */
    void setTSN(uint32_t TSN);
    /** Add the required fields to the end of the bytestream. If EOD is true (default) the
     * EOD token and the method status list will be added to the end of the bytestream. 
     * Then the bytstram is padded to a 4-byte boundary if required and the length fields
     * are populated in packet, subpacket and command packet.
     * 
     *  @param EOD a bool to signal that command requires the EOD and method status fields 
     */
    void complete(uint8_t EOD = 1);
    /** Clears the command buffer and resets the the end of buffer pointer
     * @see bufferpos
     */
    void reset();
    /** Clears the command buffer and resets the the end of buffer pointer
     * also initializes the invoker and method fields.
     * 
     *   @param InvokingUid  The UID used to call the SSC method
     *   @param method The SSC method to be called 
     */
    //void reset(OPAL_UID InvokingUid, OPAL_METHOD method);
    /** Clears the command buffer and resets the the end of buffer pointer
     * also initializes the invoker and method fields.
     * The invoker is passed as a vector<uint8_t> this is used for the case
     * where the invoker is not an OPAL user, typically a table. 
     * 
     *   @param InvokingUid  The UID used to call the SSC method 
     *   @param method The SSC method to be called  
     */
    //void reset(OPAL_UID InvokingUid, vector<uint8_t> method);

    /** Clears the command buffer and resets the the end of buffer pointer
     * also initializes the invoker and method fields.
     * Both the invoker and method are passed as a vector<uint8_t>
     * 
     *   @param InvokingUid  The UID used to call the SSC method 
     *   @param method The SSC method to be called   */
    //void reset(vector<uint8_t> InvokingUid, vector<uint8_t> method);
    void reset(const TCG_UID invoking_uid, const TCG_UID method);
    /** Changes the invoker field.
     * The invoker is passed as a vector<uint8_t> this is used for the case
     * where the invoker is not an OPAL user, typically a table. 
     * 
     *   @param Invoker  The UID used to call the SSC method
     */
    void changeInvokingUid(vector<uint8_t> Invoker);
    void changeInvokingUid(const TCG_UID invoker);
    /** Produce a hexdump of the response.  Typically used in debugging and tracing */
	//void dumpResponse();
    /** Produce a hexdump of the command.  Typically used in debugging and tracing */
	void dumpCommand();
	/** Return the space used in the command buffer (rounded to 512 bytes) */
	uint16_t outputBufferSize() const;
    /** return a pointer to the command buffer */
    void* getCmdBuffer() { return cmdbuf; }
    const BYTE* getCmdBuffer() const { return cmdbuf; }
    /** return a pointer to the response buffer. */
	//void * getRespBuffer();
private:
	uint8_t commandbuffer[MAX_BUFFER_LENGTH + IO_BUFFER_ALIGNMENT]; /**< buffer allocation allow for 1k alignment */
	//uint8_t responsebuffer[MIN_BUFFER_LENGTH + IO_BUFFER_ALIGNMENT]; /**< buffer allocation allow for 1k alignment */
	uint8_t *cmdbuf;  /**< Pointer to the command buffer */
    //uint8_t *respbuf;  /**< pointer to the response buffer */
    uint32_t bufferpos = 0;  /**< position of the next byte in the command buffer */

    ListToken * m_optional_param = nullptr;

    // cmdbuf“I™”—]—e—Ê
    //size_t cmdbuf_remain;
};

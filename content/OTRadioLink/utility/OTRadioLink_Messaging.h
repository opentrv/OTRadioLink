/*
The OpenTRV project licenses this file to you
under the Apache Licence, Version 2.0 (the "Licence");
you may not use this file except in compliance
with the Licence. You may obtain a copy of the Licence at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the Licence is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied. See the Licence for the
specific language governing permissions and limitations
under the Licence.

Author(s) / Copyright (s): Deniz Erbilgin 2017
                           Damon Hart-Davis 2017
*/

#ifndef UTILITY_OTRADIOLINK_MESSAGING_H_
#define UTILITY_OTRADIOLINK_MESSAGING_H_

#include <OTV0p2Base.h>
#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTRadValve_BoilerDriver.h"
#include "OTRadioLink_OTRadioLink.h"
#include "OTRadioLink_MessagingFS20.h"

namespace OTRadioLink
{


/**
 * @brief   Struct for passing frame data around in the RX call chain.
 * @param   msg: Raw RXed message.
 * @todo    Should msgLen be stored or is it fine to use msg[-1] to get it?
 * @todo    Is there a better way to order everything?
 * @note    Members are not initialised.
 * @note    decryptedBody is of fixed size. Could potentially be templated.
 */
struct OTFrameData_T
{
    OTFrameData_T(const uint8_t * const _msg) : msg(_msg) {}

    SecurableFrameHeader sfh;
    uint8_t senderNodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes];
    const uint8_t * const msg;
    static constexpr uint8_t decryptedBodyBufSize = ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE;
    uint8_t decryptedBody[decryptedBodyBufSize];
    uint8_t decryptedBodyLen;
    void * state = nullptr;

//    // Message length is stored in byte before first RXed message buffer.
//    inline uint8_t getMsgLen() { return msg[-1]; }
};


/**
 * @class   Interface for frame handlers.
 */
class OTFrameOperationBase
{
public:
    /*
     * @param   fd: Reference to frame data stored as OTFrameData_T.
     */
    // virtual bool handle(const OTFrameData_T &fd) = 0;
};

/**
 * @ class  Null handler that always returns true.
 */
class OTNullFrameOperationTrue final : public OTFrameOperationBase
{
public:
    inline bool handle(const OTFrameData_T & /*fd*/) { return (true); }
};

/**
 * @ class  Null handler that always returns false.
 */
// template <typename T, T &>
class OTNullFrameOperationFalse final : public OTFrameOperationBase
{
public:
    inline bool handle(const OTFrameData_T & /*fd*/) { return (false); }
};

/**
 * @class   Handler for printing to serial
 * @param   p_t: Type of printable object (usually Print, included for consistency with other handlers).
 * @param   p: Reference to printable object. Usually Serial on the arduino. NOTE! must be the concrete instance. AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 */
template <typename p_t, p_t &p>
class OTSerialFrameOperation final : public OTFrameOperationBase
{
public:
    /*
     * @brief   Construct a human/machine readable JSON frame and print to serial.
     */
    inline bool handle(const OTFrameData_T &fd)
    {
        const uint8_t * const db = fd.decryptedBody;
        const uint8_t dbLen = fd.decryptedBodyLen;
        const uint8_t * const senderNodeID = fd.senderNodeID;

        if((0 != (db[1] & 0x10)) && (dbLen > 3) && ('{' == db[2])) {
            // XXX Feel like this should be moved somewhere else.
            // TODO JSON output not implemented yet.
            // Write out the JSON message, inserting synthetic ID/@ and seq/+.
            p.print(F("{\"@\":\""));
            for(int i = 0; i < OTV0P2BASE::OpenTRV_Node_ID_Bytes; ++i) { p.print(senderNodeID[i], 16); }  // print in hex
            p.print(F("\",\"+\":"));
            p.print(fd.sfh.getSeq());
            p.print(',');
            p.write(db + 3, dbLen - 3);
            p.println('}');
            // OTV0P2BASE::outputJSONStats(&Serial, secure, msg, msglen);
            // Attempt to ensure that trailing characters are pushed out fully.
#ifdef ARDUINO_ARCH_AVR
            OTV0P2BASE::flushSerialProductive();
#endif // ARDUINO_ARCH_AVR
            return true;
        }
        return false;
    }
};

/**
 * @class   Handler for relaying over radio
 * @param   rt_t: Type of rt. Should be derived from
 * @param   rt: Radio to relay frame over. NOTE! must be the concrete instance. AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 */
template <typename rt_t, rt_t &rt>
class OTRelayFrameOperation final : public OTFrameOperationBase
{
public:
    /*
     * @brief   Relay frame over rt if basic validity check of decrypted frame passed.
     */
    inline bool handle(const OTFrameData_T &fd)
    {
        const uint8_t * const msg = fd.msg;
        // Check msg exists.
        if(nullptr == msg) return false;

        const uint8_t msglen = fd.msg[-1];
        const uint8_t * const db = fd.decryptedBody;
        const uint8_t dbLen = fd.decryptedBodyLen;

        if((0 != (db[1] & 0x10)) && (dbLen > 3) && ('{' == db[2])) {
            return rt.queueToSend(msg, msglen);
        }
        return false;
    }
};


/**
 * @class   Handler for operating boiler driver.
 * @param   bh_t: Type of bh
 * @param   bh: Boiler Hub driver. Should implement the interface of BoilerCallForHeat. NOTE! must be the concrete instance.
 *          AVR-GCC cannot currently detect compile time constness of references or pointers (20170608).
 * @param   minuteCount: Reference to the minuteCount variable in Control.cpp (20170608). TODO better description of this.
 */
template <typename bh_t, bh_t &bh, uint8_t &minuteCount>
class OTBoilerFrameOperation final : public OTFrameOperationBase
{
public:
    inline bool handle(const OTFrameData_T &fd)
    {
        const uint8_t * const db = fd.decryptedBody;

        const uint8_t percentOpen = db[0];
        if(percentOpen <= 100) { bh.remoteCallForHeatRX(0, percentOpen, minuteCount); }  // FIXME should this fail if false?
        return true;
    }
};


/**
 * @brief   Authenticate and decrypt secure frames. Expects syntax checking and validation to already have been done.
 * @param   fd: OTFrameData_T object containing message to decrypt.
 * @param   decryption_fn_t: Type of the decryption function
 * @param   decrypt: Function to decrypt secure frame with. Not included to reduce dependency on OTAESGCM.
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. Should return true on success.
 */
template <SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
          OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey>
static bool authAndDecodeOTSecurableFrame(OTFrameData_T &fd)
{
#ifdef ARDUINO_ARCH_AVR
    const uint8_t * const msg = fd.msg;
    const uint8_t msglen = msg[-1];
    uint8_t * outBuf = fd.decryptedBody;
#endif

    // Validate (authenticate) and decrypt body of secure frames.
    uint8_t key[16];
      // Get the 'building' key.
    if(!getKey(key)) { // CI throws address will never be null error.
        OTV0P2BASE::serialPrintlnAndFlush(F("!RX key"));
        return(false);
    }
    // Look up full ID in associations table,
    // validate RX message counter,
    // authenticate and decrypt,
    // update RX message counter.
    uint8_t decryptedBodyOutSize = 0;
#ifdef ARDUINO_ARCH_AVR
    const bool isOK = (0 != SimpleSecureFrame32or0BodyRXV0p2::getInstance().decodeSecureSmallFrameSafely(&fd.sfh, msg-1, msglen+1,
                                          decrypt,  // FIXME remove this dependency
                                          fd.state, key,
                                          outBuf, fd.decryptedBodyBufSize, decryptedBodyOutSize,
                                          fd.senderNodeID,
                                          true));
#else
    // Pretend it all works so we can at least test the first bit.
    const bool isOK = true;
#endif
    fd.decryptedBodyLen = decryptedBodyOutSize;
    if(!isOK) {
#if 1 // && defined(DEBUG)
        // Useful brief network diagnostics: a couple of bytes of the claimed ID of rejected frames.
        // Warnings rather than errors because there may legitimately be multiple disjoint networks.
        OTV0P2BASE::serialPrintAndFlush(F("?RX auth")); // Missing association or failed auth.
        if(fd.sfh.getIl() > 0) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(fd.sfh.id[0], 16); }
        if(fd.sfh.getIl() > 1) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(fd.sfh.id[1], 16); }
        OTV0P2BASE::serialPrintlnAndFlush();
        return (false);
#endif
    }
    return(true); // Stop if not OK.
}


/**
 * @brief   High level protocol/frame handler for decoding an RXed message.
 * @param   Pointer to message buffer. Message length is contained in the byte before the buffer.
 *          May contain trailing bytes after the message.
 * @retval  True if frame is successfully handled. NOTE: This does not mean it could be decoded/decrypted, just that the
 *          handler recognised the frame type.
 */
typedef bool (frameDecodeHandler_fn_t) (volatile const uint8_t *msg);

/**
 * @brief   Dummy frame decoder and handler.
 * @retval  Always returns false as frame could not be handled.
 * @note    Used as a dummy case for when multiple frame decoders are not used.
 */
frameDecodeHandler_fn_t decodeAndHandleDummyFrame;
inline bool decodeAndHandleDummyFrame(volatile const uint8_t * const /*msg*/)
{
    return false;
}

/**
 * @brief   Handle an OT style secure frame. Will return false for *secureable* small frames that aren't secure.
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   decryption_fn_t: Type of the decryption function
 * @param   decrypt: Function to decrypt secure frame with. Not included to reduce dependency on OTAESGCM.
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. Should return true on success.
 * @param   o(n)_t: Type of operation.
 * @param   o(n): Operation to perform on successful frame decode.
 *          Operations are performed in order, with no regard to whether a previous operation is successful.
 *          By default all operations but o1 will default to a dummy stub operation. TODO!
 * @return  true on successful frame type match (secure frame), false if no suitable frame was found/decoded and another parser should be tried.
 */
frameDecodeHandler_fn_t decodeAndHandleOTSecureFrame;
template<SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
         OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey,
         typename o1_t, o1_t &o1,
         typename o2_t, o2_t &o2>  // TODO dummy operation by default
bool decodeAndHandleOTSecureFrame(volatile const uint8_t * const _msg)
{
    const uint8_t * const msg = (const uint8_t *)_msg;
    const uint8_t firstByte = msg[0];
    const uint8_t msglen = msg[-1];

    // Buffer for receiving secure frame body.
    // (Non-secure frame bodies should be read directly from the frame buffer.)
    OTFrameData_T fd(msg);

    // Validate structure of header/frame first.
    // This is quick and checks for insane/dangerous values throughout.
    const uint8_t l = fd.sfh.checkAndDecodeSmallFrameHeader(msg-1, msglen+1);
    // If failed this early and this badly, let someone else try parsing the message buffer...
    if(0 == l) { return(false); }

    // Validate integrity of frame (CRC for non-secure, auth for secure).
    if(!fd.sfh.isSecure()) { return(false); }

    // After this point, once the frame is established as the correct protocol,
    // this routine must return true to avoid another handler
    // attempting to process it.

    // Even if auth fails, we have now handled this frame by protocol.
    if(!authAndDecodeOTSecurableFrame<decrypt, getKey>(fd)) { return(true); }

    switch(firstByte) // Switch on type.
    {
        case 'O' | 0x80: // Basic OpenTRV secure frame...
        {
            // Perform trivial validation of frame then loop through supplied handlers.
            if (fd.decryptedBodyLen < 2) { break; }
            o1.handle(fd);
            o2.handle(fd);
            // Handled message (of correct secure protocol).
            break;
        }

        // Reject unrecognised sub-type.
        default: break;
    }

    // This frame has now been dealt with (by protocol)
    // even if we happenned not to be able to process it successfully.
    return(true);
}


// Decode and handle inbound raw message (msg[-1] contains the count of bytes received).
// A message may contain trailing garbage at the end; the decoder/router should cope.
// The buffer may be reused when this returns,
// so a copy should be taken of anything that needs to be retained.
// If secure is true then this message arrived over an inherently secure channel.
// This will write any output to the supplied Print object,
// typically the Serial output (which must be running if so).
// This routine is NOT allowed to alter in any way the content of the buffer passed.
/*
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   h1_t: Type of h1
 * @param   h1: Handler object.
 * @param   frameTypen: Frame tyoe to be supplied to 1.
 * @note    decodeAndHandleFS20Frame is currently a stub and always returns false.
 */
template<frameDecodeHandler_fn_t &h1, frameDecodeHandler_fn_t &h2 = decodeAndHandleDummyFrame>
void decodeAndHandleRawRXedMessage(volatile const uint8_t * const msg)
{
    const uint8_t msglen = msg[-1];

//  // TODO: consider extracting hash of all message data (good/bad) and injecting into entropy pool.
//#if 0 && defined(DEBUG)
//  OTRadioLink::printRXMsg(p, msg-1, msglen+1); // Print len+frame.
//#endif
    if(msglen < 2) { return; } // Too short to be useful, so ignore.
   // Length-first OpenTRV securable-frame format...
    if(h1(msg)) { return; }
    if(h2(msg)) { return; }
//  // Unparseable frame: drop it; possibly log it as an error.
//#if 0 && defined(DEBUG) && !defined(ENABLE_TRIMMED_MEMORY)
//    p->print(F("!RX bad msg, len+prefix: ")); OTRadioLink::printRXMsg(p, msg-1, min(msglen+1, 8));
//#endif
    return;
}

/**
 * @brief   Abstract interface for handling message queues.
 *          Provided as V0p2 is still spaghetti (20170608).
 */
class OTMessageQueueHandlerBase
{
public:
    /**Returns true if a handler for this basic frame structure. */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) = 0;
};

/**
 * @brief   Null version. always returns false.
 */
class OTMessageQueueHandlerNull final : public OTMessageQueueHandlerBase
{
public:
    /**Never finds a handler and thus always returns false. */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) override { return false; };
};

/*
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   h1_t: Type of h1
 * @param   h1: Handler object.
 * @param   frameTypen: Frame tyoe to be supplied to 1.
 * @param   pollIO: Function pollIO in V0p2. FIXME work out how to bring pollIO into library.
 * @param   baud: Serial baud for serial output.
 */
template<frameDecodeHandler_fn_t &h1,
         frameDecodeHandler_fn_t &h2,
         bool (*pollIO) (bool), uint16_t baud>
class OTMessageQueueHandler final: public OTMessageQueueHandlerBase
{
public:
    // Incrementally process I/O and queued messages, including from the radio link.
    // This may mean printing them to Serial (which the passed Print object usually is),
    // or adjusting system parameters,
    // or relaying them elsewhere, for example.
    // This will write any output to the supplied Print object,
    // typically the Serial output (which must be running if so).
    // This will attempt to process messages in such a way
    // as to avoid internal overflows or other resource exhaustion,
    // which may mean deferring work at certain times
    // such as the end of minor cycle.
    // The Print object pointer must not be NULL.
    virtual bool handle(bool
#ifdef ARDUINO_ARCH_AVR
            wakeSerialIfNeeded
#endif // ARDUINO_ARCH_AVR
            , OTRadioLink &rl) override
    {
        // Avoid starting any potentially-slow processing very late in the minor cycle.
        // This is to reduce the risk of loop overruns
        // at the risk of delaying some processing
        // or even dropping some incoming messages if queues fill up.
        // Decoding (and printing to serial) a secure 'O' frame takes ~60 ticks (~0.47s).
        // Allow for up to 0.5s of such processing worst-case,
        // ie don't start processing anything later that 0.5s before the minor cycle end.
#ifdef ARDUINO_ARCH_AVR
        const uint8_t sctStart = OTV0P2BASE::getSubCycleTime();
        if(sctStart >= ((OTV0P2BASE::GSCT_MAX/4)*3)) { return(false); }
#endif // ARDUINO_ARCH_AVR

        // Deal with any I/O that is queued.
        bool workDone = pollIO(true);

        // Check for activity on the radio link.
        rl.poll();

#ifdef ARDUINO_ARCH_AVR
        bool neededWaking = false; // Set true once this routine wakes Serial.
#endif // ARDUINO_ARCH_AVR

        const volatile uint8_t *pb;
        if(NULL != (pb = rl.peekRXMsg())) {
#ifdef ARDUINO_ARCH_AVR
            if(!neededWaking && wakeSerialIfNeeded && OTV0P2BASE::powerUpSerialIfDisabled<baud>()) { neededWaking = true; } // FIXME
#endif // ARDUINO_ARCH_AVR
            // Don't currently regard anything arriving over the air as 'secure'.
            // FIXME: shouldn't have to cast away volatile to process the message content.
            decodeAndHandleRawRXedMessage<h1, h2> (pb);
            rl.removeRXMsg();
            // Note that some work has been done.
            workDone = true;
        }

        // Turn off serial at end, if this routine woke it.
#ifdef ARDUINO_ARCH_AVR
        if(neededWaking) { OTV0P2BASE::flushSerialProductive(); OTV0P2BASE::powerDownSerial(); }
#endif // ARDUINO_ARCH_AVR
        return(workDone);
    }
};

}

// Used to be in the switch on frame type in decodeAndHandleOTSecurableFrame.
//#if defined(ENABLE_SECURE_RADIO_BEACON)
//#if defined(ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED) // Allow insecure.
//    // Beacon / Alive frame, non-secure.
//    case OTRadioLink::FTS_ALIVE:
//      {
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("Beacon nonsecure");
//#endif
//      // Ignores any body data.
//      return(true);
//      }
//#endif // defined(ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED)
//    // Beacon / Alive frame, secure.
//    case OTRadioLink::FTS_ALIVE | 0x80:
//      {
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("Beacon");
//#endif
//      // Does not expect any body data.
//      if(decryptedBodyOutSize != 0)
//        {
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINT_FLASHSTRING("!Beacon data ");
//DEBUG_SERIAL_PRINT(decryptedBodyOutSize);
//DEBUG_SERIAL_PRINTLN();
//#endif
//        break;
//        }
//      return(true);
//      }
//#endif // defined(ENABLE_SECURE_RADIO_BEACON)
#endif /* UTILITY_OTRADIOLINK_MESSAGING_H_ */

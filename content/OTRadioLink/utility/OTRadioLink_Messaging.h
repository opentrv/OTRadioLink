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
#include "OTV0P2BASE_Util.h"
#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTRadValve_BoilerDriver.h"
#include "OTRadioLink_OTRadioLink.h"
#include "OTRadioLink_MessagingFS20.h"

namespace OTRadioLink
{

/**
 * @brief   Struct for passing frame data around in the RX call chain.
 * @param   msg: Raw RXed message. msgLen should be stored in the byte before and can be accessed with msg[-1].
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
    // The total size of the decryptedBody buffer.
    // TODO Should this be adjustable? Currently only a single buffer size in use.
    static constexpr uint8_t decryptedBodyBufSize = ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE;
    // Buffer for holding plain text.
    uint8_t decryptedBody[decryptedBodyBufSize];
    // Actual size of plain text held within decryptedBody. Should be set when decryptedBody is populated.
    uint8_t decryptedBodyLen = 0;
    // A pointer to the OTAESGCM state. This is currently not implemented.
    static constexpr void * state = nullptr;

//    // Message length is stored in byte before first RXed message buffer.
//    inline uint8_t getMsgLen() { return msg[-1]; }
};

//////////////////  FUNCTION TYPEDEFS.
/**
 * @brief   Function containing the desired operation for the frame handler to perform on receipt of a valid frame.
 * @retval  True if operation is performed successfully.
 */
typedef bool (frameOperator_fn_t) (const OTFrameData_T &fd);

/**
 * @brief   High level protocol/frame handler for decoding an RXed message.
 * @param   Pointer to message buffer. Message length is contained in the byte before the buffer.
 *          May contain trailing bytes after the message.
 * @retval  True if frame is successfully handled. NOTE: This does not mean it could be decoded/decrypted, just that the
 *          handler recognised the frame type.
 */
typedef bool (frameDecodeHandler_fn_t) (volatile const uint8_t *msg);



//////////////////  FUNCTION DECLARATIONS.
/**
 * @brief   Stub version of a frameOperator_fn_t type function.
 * @retval  Always false.
 * @note    Used as a dummy operation and should be optimised out by the compiler.
 */
inline frameOperator_fn_t nullFrameOperation;

/**
 * @brief   Operation for printing to serial
 * @param   p_t: Type of printable object (usually Print, included for consistency with other handlers).
 * @param   p: Reference to printable object. Usually the "Serial" object on the arduino. NOTE! must be the concrete instance. AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 * @retval  False if decryptedBody fails basic validation, else true. NOTE will return true even if printing fails, as long as the attempt was made.
 */
// template <typename p_t, p_t &p>
frameOperator_fn_t serialFrameOperation;

/**
 * @brief   Attempt to add raw RXed frame to rt send queue, if basic validity check of decrypted frame passed.
 * @param   rt_t: Type of rt. Should be an implementation of OTRadioLink.
 * @param   rt: Radio to relay frame over. NOTE! must be the concrete instance (e.g. SIM900 rather than SecondaryRadio). AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 * retval   True if frame successfully added to send queue on rt, else false.
 */
// template <typename rt_t, rt_t &rt>
frameOperator_fn_t relayFrameOperation;

/**
 * @brief   Operator for triggering a boiler call for heat.
 * @param   bh_t: Type of bh
 * @param   bh: Boiler Hub driver. Should implement the interface of OnOffBoilerDriverLogic. NOTE! must be the concrete instance.
 *          AVR-GCC cannot currently detect compile time constness of references or pointers (20170608).
 * @param   minuteCount: Reference to the minuteCount variable in Control.cpp (20170608).
 *          NOTE: minuteCount may be removed from the API in future (DE20170616)
 *          TODO better description of this.
 * @retval  True if call for heat handled. False if percentOpen is invalid.
 */
//template <typename bh_t, bh_t &bh, uint8_t &minuteCount>
frameOperator_fn_t boilerFrameOperation;

/**
 * @brief   Dummy frame decoder and handler.
 * @retval  Always returns false as it does not handle a frame.
 * @note    Used as a dummy case for when multiple frame decoders are not used.
 */
frameDecodeHandler_fn_t decodeAndHandleDummyFrame;

/**
 * @brief   Handle an OT style secure frame. Will return false for *secureable* small frames that aren't secure.
 * @param   msg: Raw RXed message. msgLen should be stored in the byte before and can be accessed with msg[-1].
 * @param   decrypt: Function to decrypt secure frame with. NOTE decrypt functions with workspace are not supported (20170616).
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. Should return true on success.
 * @param   o1: First operation to perform on successful frame decode.
 * @param   o2: Second operation to perform on successful frame decode.
 *          Operations are performed in order, with no regard to whether a previous operation is successful.
 *          By default all operations but o1 will default to a dummy stub operation (nullFrameOperation).
 * @retval  true on successful frame type match (secure frame), false if no suitable frame was found/decoded and another parser should be tried.
 */
//template<SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
//         OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey,
//         frameOperator_fn_t &o1,
//         frameOperator_fn_t &o2 = nullFrameOperation>
frameDecodeHandler_fn_t decodeAndHandleOTSecureFrame;



/**
 * @brief   Stub version of a frameOperator_fn_t type function.
 * @retval  Always false.
 * @note    Used as a dummy operation and should be optimised out by the compiler.
 */
bool nullFrameOperation (const OTFrameData_T & /*fd*/) { return (false); }


/**
 * @brief   Operation for printing to serial
 * @param   p_t: Type of printable object (usually Print, included for consistency with other handlers).
 * @param   p: Reference to printable object. Usually the "Serial" object on the arduino. NOTE! must be the concrete instance. AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 * @retval  False if decryptedBody fails basic validation, else true. NOTE will return true even if printing fails, as long as the attempt was made.
 */
template <typename p_t, p_t &p>
bool serialFrameOperation(const OTFrameData_T &fd)
{
    const uint8_t * const db = fd.decryptedBody;
    const uint8_t dbLen = fd.decryptedBodyLen;
    const uint8_t * const senderNodeID = fd.senderNodeID;

    // Perform some basic validation of the plain text (is it worth printing) and print.
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
        // XXX DHD is dubious about this setup.
#ifdef ARDUINO_ARCH_AVR
        OTV0P2BASE::flushSerialProductive();
#endif // ARDUINO_ARCH_AVR
        return true;
    }
    return false;
}

/**
 * @brief   Attempt to add raw RXed frame to rt send queue, if basic validity check of decrypted frame passed.
 * @param   rt_t: Type of rt. Should be an implementation of OTRadioLink.
 * @param   rt: Radio to relay frame over. NOTE! must be the concrete instance (e.g. SIM900 rather than SecondaryRadio). AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 * retval   True if frame successfully added to send queue on rt, else false.
 */
template <typename rt_t, rt_t &rt>
bool relayFrameOperation(const OTFrameData_T &fd)
{
    const uint8_t * const msg = fd.msg;
    // Check msg exists.
    if(nullptr == msg) return false;

    const uint8_t msglen = fd.msg[-1];
    const uint8_t * const db = fd.decryptedBody;
    const uint8_t dbLen = fd.decryptedBodyLen;

    // Perform some basic validation of the plain text (is it worth sending) and add to relay radio queue.
    if((0 != (db[1] & 0x10)) && (dbLen > 3) && ('{' == db[2])) {
        return rt.queueToSend(msg, msglen);
    }
    return false;
}

/**
 * @brief   Operator for triggering a boiler call for heat.
 * @param   bh_t: Type of bh
 * @param   bh: Boiler Hub driver. Should implement the interface of OnOffBoilerDriverLogic. NOTE! must be the concrete instance.
 *          AVR-GCC cannot currently detect compile time constness of references or pointers (20170608).
 * @param   minuteCount: Reference to the minuteCount variable in Control.cpp (20170608).
 *          NOTE: minuteCount may be removed from the API in future (DE20170616)
 *          TODO better description of this.
 * @retval  True if call for heat handled. False if percentOpen is invalid.
 */
template <typename bh_t, bh_t &bh, uint8_t &minuteCount>
bool boilerFrameOperation(const OTFrameData_T &fd)
{
    const uint8_t * const db = fd.decryptedBody;

    const uint8_t percentOpen = db[0];
    if(percentOpen <= 100) {
        bh.remoteCallForHeatRX(0, percentOpen, minuteCount);
        return (true);
    }
    return (false);
}


/**
 * @brief   Authenticate and decrypt secure frames. Expects syntax checking and validation to already have been done.
 * @param   fd: OTFrameData_T object containing message to decrypt.
 * @param   decrypt: Function to decrypt secure frame with. NOTE decrypt functions with workspace are not supported (20170616).
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. Should return true on success.
 * @retval  True if frame successfully authenticated and decoded, else false.
 */
template <typename sfrx_t,
          SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
          OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey>
inline bool authAndDecodeOTSecurableFrame(OTFrameData_T &fd)
{
    const uint8_t * const msg = fd.msg;
    const uint8_t msglen = msg[-1];
    uint8_t * outBuf = fd.decryptedBody;

    OTV0P2BASE::MemoryChecks::recordIfMinSP();

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
    const bool isOK = (0 != sfrx_t::getInstance().decodeSecureSmallFrameSafely(&fd.sfh, msg-1, msglen+1,
                                          decrypt,  // FIXME remove this dependency
                                          fd.state, key,
                                          outBuf, fd.decryptedBodyBufSize, decryptedBodyOutSize,
                                          fd.senderNodeID,
                                          true));
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
 * @brief   Stub version of a frameOperator_fn_t type function.
 * @retval  Always false.
 * @note    Used as a dummy operation and should be optimised out by the compiler.
 */
inline bool decodeAndHandleDummyFrame(volatile const uint8_t * const /*msg*/)
{
    return false;
}

/**
 * @brief   Handle an OT style secure frame. Will return false for *secureable* small frames that aren't secure.
 * @param   msg: Raw RXed message. msgLen should be stored in the byte before and can be accessed with msg[-1].
 * @param   decrypt: Function to decrypt secure frame with. NOTE decrypt functions with workspace are not supported (20170616).
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. Should return true on success.
 * @param   o1: First operation to perform on successful frame decode.
 * @param   o2: Second operation to perform on successful frame decode.
 *          Operations are performed in order, with no regard to whether a previous operation is successful.
 *          By default all operations but o1 will default to a dummy stub operation (nullFrameOperation).
 * @return  true on successful frame type match (secure frame), false if no suitable frame was found/decoded and another parser should be tried.
 */
template<typename sfrx_t,
         SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
         OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey,
         frameOperator_fn_t &o1,
         frameOperator_fn_t &o2 = nullFrameOperation>
bool decodeAndHandleOTSecureOFrame(volatile const uint8_t * const _msg)
{
#if 1
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
#endif
    const uint8_t * const msg = (const uint8_t * const)_msg;
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
    // Make sure frame thinks it is a secure OFrame.
    constexpr uint8_t expectedOFrameFirstByte = 'O' | 0x80;
    if(expectedOFrameFirstByte != firstByte) { return (false); }

    // Validate integrity of frame (CRC for non-secure, auth for secure).
    if(!fd.sfh.isSecure()) { return(false); }

    // After this point, once the frame is established as the correct protocol,
    // this routine must return true to avoid another handler
    // attempting to process it.

    // Even if auth fails, we have now handled this frame by protocol.
    if(!authAndDecodeOTSecurableFrame<sfrx_t, decrypt, getKey>(fd)) { return(true); }

    // Make sure frame is long enough to have useful information in it and call operations.
    if(2 < fd.decryptedBodyLen) {
        o1(fd);
        o2(fd);
    }
    // This frame has now been dealt with (by protocol)
    // even if we happenned not to be able to process it successfully.
    return(true);
}
/**
 * @brief   Version of decodeAndHandleOTSecureOFrame that takes
 **/
template<typename sfrx_t,
         SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
         OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey,
         frameOperator_fn_t &o1,
         frameOperator_fn_t &o2 = nullFrameOperation>
bool decodeAndHandleOTSecureOFrameWithWorkspace(volatile const uint8_t * const _msg, OTV0P2BASE::ScratchSpace & /*sW*/)
{
#if 1
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
#endif
    const uint8_t * const msg = (const uint8_t * const)_msg;
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
    // Make sure frame thinks it is a secure OFrame.
    constexpr uint8_t expectedOFrameFirstByte = 'O' | 0x80;
    if(expectedOFrameFirstByte != firstByte) { return (false); }

    // Validate integrity of frame (CRC for non-secure, auth for secure).
    if(!fd.sfh.isSecure()) { return(false); }

    // After this point, once the frame is established as the correct protocol,
    // this routine must return true to avoid another handler
    // attempting to process it.

    // Even if auth fails, we have now handled this frame by protocol.
    if(!authAndDecodeOTSecurableFrame<sfrx_t, decrypt, getKey>(fd)) { return(true); }

    // Make sure frame is long enough to have useful information in it and call operations.
    if(2 < fd.decryptedBodyLen) {
        o1(fd);
        o2(fd);
    }
    // This frame has now been dealt with (by protocol)
    // even if we happenned not to be able to process it successfully.
    return(true);
}


/*
 * @brief   Decode and handle inbound raw message (msg[-1] contains the count of bytes received).
 *          A message may contain trailing garbage at the end; the decoder/router should cope.
 *          The message is passed on to a set of handlers that are passed in as template parameters.
 *          Handlers should be passed in the order they should be executed as the function will return on the first
 *          successful handler, skipping all subsequent ones.
 *          The buffer may be reused when this returns, so a copy should be taken of anything that needs to be retained.
 * @param   msg: Raw RXed message. msgLen should be stored in the byte before and can be accessed with msg[-1].
 *          This routine is NOT allowed to alter in any way the content of the buffer passed.
 * @param   h1: First frame handler to attempt.
 * @param   h2: Second frame handler to attempt. Defaults to a dummy handler.
 *          Handlers are called in order, until the first successful handler.
 *          By default all operations but h1 will default to a dummy stub operation (decodeAndHandleDummyFrame).
 */
template<frameDecodeHandler_fn_t &h1, frameDecodeHandler_fn_t &h2 = decodeAndHandleDummyFrame>
void decodeAndHandleRawRXedMessage(volatile const uint8_t * const msg)
{
    const uint8_t msglen = msg[-1];

//  // TODO: consider extracting hash of all message data (good/bad) and injecting into entropy pool.
    if(msglen < 2) { return; } // Too short to be useful, so ignore.
    // Go through handlers. (20170616) Currently relying on compiler to optimise out anything unneeded.
    if(h1(msg)) { return; }
    if(h2(msg)) { return; }
  // Unparseable frame: drop it.
    return;
}

/**
 * @brief   Abstract interface for handling message queues.
 *          Provided as V0p2 is still spaghetti (20170608).
 */
class OTMessageQueueHandlerBase
{
public:
    /**
     * @brief   Check the supplied radio if a frame has been received and pass on to any relevant handlers.
     * @param   wakeSerialIfNeeded: If true, makes sure the serial port is enabled on entry and returns it to how it
     *          found it on exit.
     * @param   rl: Radio to check for new RXed frames.
     * @retval  Returns true if anything was done..
     */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) = 0;
};

/**
 * @brief   Null version. always returns false.
 */
class OTMessageQueueHandlerNull final : public OTMessageQueueHandlerBase
{
public:
    /**
     * @retval  Always false as it never never tries to do anything.
     */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) override { return false; };
};

/*
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   pollIO: Function pollIO in V0p2. This is intended to poll all IO lines, excluding sensors and serial ports
 *          and return true if any was processed.
 *          FIXME work out if we want to bring pollIO into library.
 * @param   baud: Serial baud for serial output.
 * @param   h1: First frame handler to attempt.
 * @param   h2: Second frame handler to attempt. Defaults to a dummy handler.
 *          Handlers are called in order, until the first successful handler.
 *          By default all operations but h1 will default to a dummy stub operation (decodeAndHandleDummyFrame).
 */
template<bool (*pollIO) (bool), uint16_t baud,
         frameDecodeHandler_fn_t &h1,
         frameDecodeHandler_fn_t &h2 = decodeAndHandleDummyFrame>
class OTMessageQueueHandler final: public OTMessageQueueHandlerBase
{
public:
    /**
     * @brief   Incrementally process I/O and queued messages, including from the radio link.
     *          This may mean printing them to Serial (which the passed Print object usually is),
     *          or adjusting system parameters, or relaying them elsewhere, for example.
     *          This will attempt to process messages in such a way
     *          as to avoid internal overflows or other resource exhaustion,
     *          which may mean deferring work at certain times
     *          such as the end of minor cycle.
     * @param   wakeSerialIfNeeded: If true, makes sure the serial port is enabled on entry and returns it to how it
     *          found it on exit.
     * @param   rl: Radio to check for new RXed frames.
     * @retval  Returns true if a message was RXed and processed, or if pollIO returns true.
     */
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

        // Pointer to RX message buffer of radio.
        const volatile uint8_t *pb = rl.peekRXMsg();
        // If pb is a nullptr at this stage, no message has been RXed.
        if(nullptr != pb) {
#ifdef ARDUINO_ARCH_AVR
            bool neededWaking = false; // Set true once this routine wakes Serial.
            if(!neededWaking && wakeSerialIfNeeded && OTV0P2BASE::powerUpSerialIfDisabled<baud>()) { neededWaking = true; } // FIXME
#endif // ARDUINO_ARCH_AVR
            // Don't currently regard anything arriving over the air as 'secure'.
            // FIXME: shouldn't have to cast away volatile to process the message content.
            decodeAndHandleRawRXedMessage<h1, h2> (pb);
            rl.removeRXMsg();
            // Note that some work has been done.
            workDone = true;
            // Turn off serial at end, if this routine woke it.
#ifdef ARDUINO_ARCH_AVR
            if(neededWaking) { OTV0P2BASE::flushSerialProductive(); OTV0P2BASE::powerDownSerial(); }
#endif // ARDUINO_ARCH_AVR
        }
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

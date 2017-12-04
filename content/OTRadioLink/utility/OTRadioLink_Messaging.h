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

//////////////////  FUNCTION TYPEDEFS.
/**
 * @brief   Function containing the desired operation for the frame handler to
 *          perform on receipt of a valid frame.
 * @retval  True if operation is performed successfully.
 * FIXME    Return values are always unused.
 */
typedef bool (frameOperator_fn_t) (const OTDecodeData_T &fd);

/**
 * @brief   High level protocol/frame handler for decoding an RXed message.
 * @param   Pointer to message buffer. Message length is contained in the byte
 *          before the buffer.
 *          May contain trailing bytes after the message.
 * @retval  True if frame is successfully handled. NOTE: This does not mean it 
 *          could be decoded/decrypted, just that the handler recognised the
 *          frame type.
 */
typedef bool (frameDecodeHandler_fn_t) (volatile const uint8_t *msg);



//////////////////  FUNCTION DECLARATIONS.
// Stub version of a frameOperator_fn_t type function.
inline frameOperator_fn_t nullFrameOperation;

// Print a JSON frame to serial
frameOperator_fn_t serialFrameOperation;

// Attempt to add raw RXed frame to rt send queue, if basic validity check of
// decrypted frame passed.
frameOperator_fn_t relayFrameOperation;

// Trigger a boiler call for heat.
frameOperator_fn_t boilerFrameOperation;

// Dummy frame decoder and handler.
frameDecodeHandler_fn_t decodeAndHandleDummyFrame;

// Handle an OT style secure frame. Will return false for *secureable* small
// frames that aren't secure.
frameDecodeHandler_fn_t decodeAndHandleOTSecureFrame;



/**
 * @brief   Stub version of a frameOperator_fn_t type function.
 * @retval  Always false.
 * @note    Used as a dummy operation. Should be optimised out by the compiler.
 */
bool nullFrameOperation (const OTDecodeData_T & /*fd*/) { return (false); }


/**
 * @brief   Operation for printing to serial
 * @param   p_t: Type of printable object (usually Print, included for
 *          consistency with other handlers).
 * @param   p: Reference to printable object. Usually the "Serial" object on 
 *          the arduino. NOTE! must be the concrete instance. AVR-GCC cannot 
 *          currently detect compile time constness of references or pointers (20170608).
 * @param   fd: Decoded frame data.
 * @retval  False if decryptedBody fails basic validation, else true. NOTE will
 *           return true even if printing fails, as long as it was attempted.
 */
template <typename p_t, p_t &p>
bool serialFrameOperation(const OTDecodeData_T &fd)
{
    const uint8_t * const db = fd.ptext;
    const uint8_t dbLen = fd.ptextLen;
    const uint8_t * const senderNodeID = fd.id;

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
 * @brief   Attempt to add raw RXed frame to rt send queue, if basic validity 
 *          check of decrypted frame passed.
 * @param   rt_t: Type of rt. Should be an implementation of OTRadioLink.
 * @param   rt: Radio to relay frame over. NOTE! must be the concrete instance 
 *          (e.g. SIM900 rather than SecondaryRadio). AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 * @param   fd: Decoded frame data.
 * retval   True if frame successfully added to send queue on rt, else false.
 */
template <typename rt_t, rt_t &rt>
bool relayFrameOperation(const OTDecodeData_T &fd)
{
    // Check msg exists.
    if(nullptr == fd.ctext) return false;

    const uint8_t * const db = fd.ptext;
    const uint8_t dbLen = fd.ptextLen;

    // Perform some basic validation of the plain text (is it worth sending) and add to relay radio queue.
    if((0 != (db[1] & 0x10)) && (dbLen > 3) && ('{' == db[2])) {
        return rt.queueToSend(fd.ctext + 1, fd.ctextLen);
    }
    return false;
}

/**
 * @brief   Operator for triggering a boiler call for heat.
 * @param   bh_t: Type of bh
 * @param   bh: Boiler Hub driver. Should implement the interface of
 *          OnOffBoilerDriverLogic. NOTE! must be the concrete instance.
 *          AVR-GCC cannot currently detect compile time constness of
 *          references or pointers (20170608).
 * @param   minuteCount: Reference to the minuteCount variable in Control.cpp (20170608).
 *          NOTE: minuteCount may be removed from the API in future (DE20170616)
 * @param   fd: Decoded frame data.
 * @retval  True if call for heat handled. False if percentOpen is invalid.
 */
template <typename bh_t, bh_t &bh, const uint8_t &minuteCount>
bool boilerFrameOperation(const OTDecodeData_T &fd)
{
    const uint8_t * const db = fd.ptext;

    const uint8_t percentOpen = db[0];
    if(percentOpen <= 100) {
        bh.remoteCallForHeatRX(0, percentOpen, minuteCount);
        return (true);
    }
    return (false);
}


/**
 * @brief   Authenticate and decrypt secure frames. Expects syntax checking and
 *          validation to already have been done.
 * @param   fd: OTFrameData_T object containing message to decrypt.
 * @param   decrypt: Function to decrypt secure frame with.
 * @param   getKey: Function that fills a buffer with the 16 byte secret key. 
 *          Should return true on success.
 * @retval  True if frame successfully authenticated and decoded, else false.
 *
 * Note: the scratch space (workspace) depends on the underlying decrypt
 * function and the receiver class.
 */
// Local scratch: total incl template srfx_t::decodeSecureSmallFrameSafely().
static constexpr uint8_t authAndDecodeOTSecurableFrameWithWorkspace_scratch_usage =
    16; // Primary building key size.
template <typename sfrx_t,
          SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
          OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey>
inline bool authAndDecodeOTSecurableFrame(OTDecodeData_T &fd, OTV0P2BASE::ScratchSpaceL &sW)
{
    const size_t scratchSpaceNeededHere = authAndDecodeOTSecurableFrameWithWorkspace_scratch_usage;
    if(sW.bufsize < scratchSpaceNeededHere) { return(false); } // ERROR

#if 0
    // Probe the stack here, in case we don't get deeper.
    OTV0P2BASE::MemoryChecks::recordIfMinSP();
#endif

    // Use scratch space for 16-byte key.
    uint8_t *key = sW.buf;
    // Get the building primary key.
    if(!getKey(key)) { // CI throws "address will never be null" error.
        OTV0P2BASE::serialPrintlnAndFlush(F("!RX key"));
        return(false);
    }

    // Create sub-space for callee.
    OTV0P2BASE::ScratchSpaceL subScratch(sW, scratchSpaceNeededHere);

    // Now attempt to decrypt.
    // Assumed no need to 'adjust' node ID for this form of RX.

    // Look up full ID in associations table,
    // validate the RX message counter,
    // authenticate and decrypt,
    // then update the RX message counter.
    const bool isOK = (0 != sfrx_t::getInstance().decode(
                                                      fd,
                                                      decrypt,
                                                      subScratch, key,
                                                      true));
#if 1 // && defined(DEBUG)
if(!isOK) {
// Useful brief network diagnostics: a couple of bytes of the claimed ID of rejected frames.
// Warnings rather than errors because there may legitimately be multiple disjoint networks.
OTV0P2BASE::serialPrintAndFlush(F("?RX auth")); // Missing association or failed auth.
if(fd.sfh.getIl() > 0) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(fd.sfh.id[0], 16); }
if(fd.sfh.getIl() > 1) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(fd.sfh.id[1], 16); }
OTV0P2BASE::serialPrintlnAndFlush();
}
#endif

    return(isOK); // Return if successfully decoded, authenticated, etc.
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
 * @brief   Attempt to decode a message as if it is a standard OT secure "O"
 *          type frame. May perform up to two "operations" if the decode
 *          succeeds.
 * 
 * First confirms that the frame "looks like" a secure O frame and that it can
 * attempt to decode, i.e.
 * - The message has a valid header.
 * - The first byte matches an O frame with the secure bit set.
 * - The message is a secure frame.
 * 
 * Any actions to be taken on a succesful decode must be passed in as callbacks,
 * or the message will be decoded then lost. In this library, they are labelled
 * `operators` to avoid confusion with the message/frame handlers.
 * The operators are called in ascending order and will always all be called
 * if the message is successfully decoded. They should not alter the decoded
 * frame data (fd) in any way.
 * Note that operators can only take the frame data as function parameters and
 * anything else must be passed in as template parameters.
 * 
 * @param   sfrx_t: TODO
 * @param   decrypt: A function to decrypt secure frame with.
 * @param   getKey: A function that fills a buffer with the 16 byte secret key.
 *          Should return true on success.
 * @param   o1: First operator to be called.
 * @param   o2: Second operator to be called. Defaults to a dummy impl.
 * @param   _msg: Raw RXed message. msgLen should be stored in the byte before
 *          and can be accessed with msg[-1]. This routine is NOT allowed to
 *          alter content of the buffer passed.
 * @param   sW: Scratch space to perform decode routine in. Should be large
 *          enough for both the frame RX type and the underlying decryption
 *          routine.
 * @retval  False if the frame header could not be decoded, does not match a
 *          secure "O" frame, or the frame is otherwise malformed in any way.
 *          True if the frame is a valid secure frame.
 *          NOTE! A frame that "looks like" a secure "O" frame but can not be
 *          authed or decoded, it will return true.
 */
template<typename sfrx_t,
         SimpleSecureFrame32or0BodyRXBase::fixed32BTextSize12BNonce16BTagSimpleDec_fn_t &decrypt,
         OTV0P2BASE::GetPrimary16ByteSecretKey_t &getKey,
         frameOperator_fn_t &o1,
         frameOperator_fn_t &o2 = nullFrameOperation>
bool decodeAndHandleOTSecureOFrame(volatile const uint8_t * const _msg, OTV0P2BASE::ScratchSpaceL &sW)
{
    const uint8_t * const msg = (const uint8_t * const)_msg - 1;
    const uint8_t firstByte = msg[1];
    const uint8_t msglen = msg[0];

    // Buffer for receiving secure frame body.
    // (Non-secure frame bodies should be read directly from the frame buffer.)
    uint8_t decryptedBodyOut[OTDecodeData_T::ptextLenMax];
    OTDecodeData_T fd(msg, decryptedBodyOut);

    // Validate structure of header/frame first.
    // This is quick and checks for insane/dangerous values throughout.
    const uint8_t l = fd.sfh.decodeHeader(msg, msglen + 1);
    // If failed this early and this badly,
    // then let another protocol handler try parsing the message buffer...
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
    if(!authAndDecodeOTSecurableFrame<sfrx_t, decrypt, getKey>(fd, sW))
        { return(true); }

    // Make sure frame is long enough to have useful information in it
    // and then call operations.
    if(2 < fd.ptextLenMax) {
        o1(fd);
        o2(fd);
    }
    // This frame has now been dealt with (by protocol)
    // even if we happened not to be able to process it successfully.
    return(true);
}


/*
 * @brief   Attempt to decode an inbound message using all available decoders.
 * 
 * The decoder/router should cope with message that contain trailing garbage at
 * the end. The message is passed on to a set of handlers that are passed in as
 * template parameters.
 * 
 * Handlers should be passed in the order they should be executed as the
 * function will return on the first successful handler, skipping all
 * subsequent ones.
 * 
 * `msg` may be reused when this returns, so a copy should be taken of anything
 * that needs to be retained.
 * 
 * @param   msg: Raw RXed message. msgLen should be stored in the byte before
 *          and can be accessed with msg[-1]. This routine is NOT allowed to
 *          alter content of the buffer passed.
 * @param   h1: First frame handler to attempt.
 * @param   h2: Second frame handler to attempt. Defaults to a dummy handler.
 *          Handlers are called in order, until the first successful handler.
 *          By default all operations but h1 will default to a dummy stub operation (decodeAndHandleDummyFrame).
 */
template<frameDecodeHandler_fn_t &h1, frameDecodeHandler_fn_t &h2 = decodeAndHandleDummyFrame>
void decodeAndHandleRawRXedMessage(volatile const uint8_t * const msg)
{
    const uint8_t msglen = msg[-1];

    // TODO: consider extracting hash of all message data (good/bad) and injecting into entropy pool.
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
     * @brief   Check the supplied radio if a frame has been received and pass
     *          on to any relevant handlers.
     * @param   wakeSerialIfNeeded: If true, makes sure the serial port is
     *          enabled on entry and returns it to how it found it on exit.
     * @param   rl: Radio to check for new RXed frames.
     * @retval  Returns true if anything was done..
     */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) = 0;
};

/**
 * @brief   Null implementation. Always returns false.
 */
class OTMessageQueueHandlerNull final : public OTMessageQueueHandlerBase
{
public:
    /**
     * @retval  Always false as it never never tries to do anything.
     */
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink & /*rl*/) override
        { return false; };
};

/*
 * @param   pollIO: Function to call before handling inbound messages. TODO Rename this.
 * @param   baud: Serial baud for serial output.
 * @param   h1: First frame handler to attempt.
 * @param   h2: Second frame handler to attempt. Defaults to a dummy handler.
 *          See `handle` for details on how handlers are called.
 */
template<bool (*pollIO) (bool), uint16_t baud,
         frameDecodeHandler_fn_t &h1,
         frameDecodeHandler_fn_t &h2 = decodeAndHandleDummyFrame>
class OTMessageQueueHandler final: public OTMessageQueueHandlerBase
{
public:
    /**
     * @brief   Poll radio and incrementally process any queued messages.
     * 
     * This will attempt to decode an inbound frame using up to two frame
     * handlers. The handlers will be tried in the order they are passed in to
     * the message queue handler and this function will:
     * - exit on the first handler to return a success (bool true)
     * - after all handlers have failed, in which case the frame is dropped.
     * Handlers are expected to be perform or trigger any action required by
     * the frame.
     * 
     * Typical workflow:
     * - Setup a radio (derived from OTRadioLink::OTRadioLink).
     * - Define a pollIO function, e.g. poll all IO lines, excluding sensors
     *   and serial ports and return true if any was processed.
     * - Define up to two frame handlers (See docs for `frameDecodeHandler_fn_t`
     *   in this file).
     * - Create an OTMessageQueueHandler object.
     * - Call handle(...) periodically to handle incoming messages.
     * See OpenTRV-Arduino-V0p2/Arduino/hardware/REV10/REV10SecureBHR/REV10SecureBHR.ino
     * for an example that relays a frame over another radio and notifies the
     * boiler hub driver of the states of any associated valves.
     * 
     * This will attempt to process messages in such a way
     * as to avoid internal overflows or other resource exhaustion,
     * which may mean deferring work at certain times
     * such as the end of minor cycle.
     * 
     * @param   wakeSerialIfNeeded: If true, makes sure the serial port is 
     *          enabled on entry and returns it to how it found it on exit.
     * @param   rl: Radio to check for new RXed frames.
     * @retval  Returns true if a message was RXed and processed, or if pollIO
     *          returns true. NOTE that this is independant of whether the
     *          message was successfully handled.
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

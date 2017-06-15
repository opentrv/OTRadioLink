/*
 * OTRadioLink_Messagin.h
 *
 *  Created on: 18 May 2017
 *      Author: denzo
 */

#ifndef UTILITY_OTRADIOLINK_MESSAGING_H_
#define UTILITY_OTRADIOLINK_MESSAGING_H_

#include <OTV0p2Base.h>
#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTRadValve_BoilerDriver.h"
#include "OTRadioLink_OTRadioLink.h"

namespace OTRadioLink
{

/**
 * @class   Interface for frame handlers.
 */
class OTFrameHandlerBase
{
public:
    /*
     * @param   msg: pointer to buffer containing encrypted message.
     * @param   decryptedBody: pointer to buffer containing plain text.
     * @param   decryptedBody: length of decryptedBody.
     * @fixme   Currently not sure how to pass in sfh and sender node ID.
     */
    virtual bool frameHandler(const uint8_t *const msg,
                              const uint8_t *const decryptedBody,
                              const uint8_t decryptedBodyLen) = 0;
};

/**
 * @class   Handler for printing to serial
 * @param   p_t: Type of printable object (usually Print, included for consistency with other handlers).
 * @param   p: Reference to printable object. Usually Serial on the arduino. NOTE! must be the concrete instance. AVR-GCC cannot currently
 *          detect compile time constness of references or pointers (20170608).
 */
template <typename p_t, p_t &p>
class OTSerialHandler final : public OTFrameHandlerBase
{
public:
    /*
     * @brief   Construct a human/machine readable JSON frame and print to serial.
     * @fixme   Currently not sure how to pass in sfh and sender node ID.
     */
    virtual bool frameHandler(const uint8_t *const /*msg*/,
                              const uint8_t *const decryptedBody,
                              const uint8_t decryptedBodyLen) override
    {
        if((0 != (decryptedBody[1] & 0x10)) && (decryptedBodyLen > 3) && ('{' == decryptedBody[2])) {
            // XXX Feel like this should be moved somewhere else.
            // TODO JSON output not implemented yet.
            // Write out the JSON message, inserting synthetic ID/@ and seq/+.
            p.print(F("{\"@\":\""));
//            for(int i = 0; i < OTV0P2BASE::OpenTRV_Node_ID_Bytes; ++i) { p.print(senderNodeID[i], HEX); }  // FIXME
            p.print(F("\",\"+\":"));  // FIXME
//            p.print(sfh.getSeq());
            p.print(',');
            p.write(decryptedBody + 3, decryptedBodyLen - 3);
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
class OTRadioHandler final : public OTFrameHandlerBase
{
public:
    /*
     * @brief   Relay frame over rt if basic validity check of decrypted frame passed.
     */
    virtual bool frameHandler(const uint8_t *const msg,
                              const uint8_t *const decryptedBody,
                              const uint8_t decryptedBodyLen) override
    {
        const uint8_t msglen = msg[-1];
        if((0 != (decryptedBody[1] & 0x10)) && (decryptedBodyLen > 3) && ('{' == decryptedBody[2])) {
            return rt.queueToSend(msg, msglen, 0, (OTRadioLink::OTRadioLink::TXpower) 0 );  // FIXME!!! this should be passed in? Ignored by OTSIM900Link.
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
class OTBoilerHandler final : public OTFrameHandlerBase
{
public:
    virtual bool frameHandler(const uint8_t *const /*msg*/,
                              const uint8_t *const decryptedBody,
                              const uint8_t /*decryptedBodyLen*/) override
    {
          const uint8_t percentOpen = decryptedBody[0];
          if(percentOpen <= 100) { bh.remoteCallForHeatRX(0, percentOpen, minuteCount); }
          return true;
    }
};

/**
 * @brief   Perform trivial validation of frame then loop through supplied handlers.
 * @param   hn_t: Type of hn
 * @param   hn: Handler object.
 * @param   frameTypen: Frame tyoe to be supplied to n.
 * @retval  True on success of all handlers, else false.
 * @TODO    work out what to do with frameTypen
 * @TODO    Implement parameter packing.
 */
template <typename h1_t, h1_t &h1, uint8_t frameType1>
bool handleOTSecureFrame(const uint8_t *const msg,
                         const uint8_t *const decryptedBody,
                         const uint8_t decryptedBodyLen)
{
   if (decryptedBodyLen < 2) return (false);
   return (h1.frameHandler(msg, decryptedBody, decryptedBodyLen));
}
template <typename h1_t, h1_t &h1, uint8_t frameType1,
          typename h2_t, h2_t &h2, uint8_t frameType2>
bool handleOTSecureFrame(const uint8_t *const msg,
                          const uint8_t *const decryptedBody,
                          const uint8_t decryptedBodyLen)
{
    bool success = true;
    if (decryptedBodyLen < 2) return (false);
    if (!h1.frameHandler(msg, decryptedBody, decryptedBodyLen)) success = false;
    if (!h2.frameHandler(msg, decryptedBody, decryptedBodyLen)) success = false;
    return success;
}


/**
 * @brief   Validate, authenticate and decrypt secure frames.
 * @param   msg: message to decrypt
 * @param   outBuf: output buffer
 * @param   decryptedBodyOutSize: Size of decrypted message
 * @param   allowInsecureRX: Allows insecure frames to be received. Defaults to false.
 */
template <bool allowInsecureRX = false>
static bool authAndDecodeOTSecureableFrame(const uint8_t * const msg, uint8_t * const outBuf,
                                           const uint8_t outBufSize, uint8_t &decryptedBodyOutSize)
{
    const uint8_t msglen = msg[-1];
    SecurableFrameHeader sfh;
    // Validate structure of header/frame first.
    // This is quick and checks for insane/dangerous values throughout.
    const uint8_t l = sfh.checkAndDecodeSmallFrameHeader(msg-1, msglen+1);
    // If failed this early and this badly, let someone else try parsing the message buffer...
    if(0 == l) { return(false); }

    // Validate integrity of frame (CRC for non-secure, auth for secure).
    const bool secureFrame = sfh.isSecure();
    // TODO: validate entire message, eg including auth, or CRC if insecure msg rcvd&allowed.
    if(!secureFrame) {
        if (allowInsecureRX) {
            // Only bother to check insecure form (and link code to do so) if insecure RX is allowed.
            // Reject if CRC fails.
            if(0 == decodeNonsecureSmallFrameRaw(&sfh, msg-1, msglen+1))
                { return false; }
        } else {
            // Decode fails
            return (false);
        }
    }
    // Validate (authenticate) and decrypt body of secure frames.
    uint8_t key[16];
    if(secureFrame)
      {
      // Get the 'building' key.
      if(!OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(key))
        {
        OTV0P2BASE::serialPrintlnAndFlush(F("!RX key"));
        return(false);
        }
      }
    uint8_t senderNodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes];
    if(secureFrame)
      {
      // Look up full ID in associations table,
      // validate RX message counter,
      // authenticate and decrypt,
      // update RX message counter.
#ifdef ARDUINO_ARCH_AVR
      const bool isOK = (0 != SimpleSecureFrame32or0BodyRXV0p2::getInstance().decodeSecureSmallFrameSafely(&sfh, msg-1, msglen+1,
                                              OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                              NULL, key,
                                              outBuf, outBufSize, decryptedBodyOutSize,
                                              senderNodeID,
                                              true));
  #if 1 // && defined(DEBUG)
      if(!isOK)
        {
        // Useful brief network diagnostics: a couple of bytes of the claimed ID of rejected frames.
        // Warnings rather than errors because there may legitimately be multiple disjoint networks.
        OTV0P2BASE::serialPrintAndFlush(F("?RX auth")); // Missing association or failed auth.
        if(sfh.getIl() > 0) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(sfh.id[0], HEX); }
        if(sfh.getIl() > 1) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(sfh.id[1], HEX); }
        OTV0P2BASE::serialPrintlnAndFlush();
        return (false);
        }
  #endif
#endif // ARDUINO_ARCH_AVR

      }

    return(true); // Stop if not OK.

}


/**
 * @brief   Try to decode an OT style secureable frame.
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   h1_t: Type of h1
 * @param   h1: Handler object.
 * @param   frameTypen: Frame tyoe to be supplied to 1.
 * @param   allowInsecureRX: Allow RX of insecure frames. Defaults to false.
 * @return  true on successful frame type match, false if no suitable frame was found/decoded and another parser should be tried.
 * @note    - Secure beacon frames commented to save complexity, as not currently used by any configs.
 */
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         bool allowInsecureRX = false>
static bool decodeAndHandleOTSecurableFrame(const uint8_t * const msg)
{
    const uint8_t firstByte = msg[0];

    // Buffer for receiving secure frame body.
    // (Non-secure frame bodies should be read directly from the frame buffer.)
    uint8_t secBodyBuf[ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE];
    uint8_t decryptedBodyOutSize = 0;

    if(!authAndDecodeOTSecureableFrame<allowInsecureRX>(msg, secBodyBuf, sizeof(secBodyBuf), decryptedBodyOutSize)) {
        return false;
    }

    switch(firstByte) // Switch on type.
    {
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

        case 'O' | 0x80: // Basic OpenTRV secure frame...
        {
            return (handleOTSecureFrame<h1_t, h1, frameType1>(msg, secBodyBuf, decryptedBodyOutSize)); // handleOTSecurableFrame
        }

          // Reject unrecognised type, though fall through potentially to recognise other encodings.
        default: break;
    }

    // Failed to parse; let another handler try.
    return(false);
}
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         typename h2_t, h2_t &h2, uint8_t frameType2,
         bool allowInsecureRX = false>
static bool decodeAndHandleOTSecurableFrame(const uint8_t * const msg)
{
    const uint8_t firstByte = msg[0];

    // Buffer for receiving secure frame body.
    // (Non-secure frame bodies should be read directly from the frame buffer.)
    uint8_t secBodyBuf[ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE];
    uint8_t decryptedBodyOutSize = 0;

    if(!authAndDecodeOTSecureableFrame<allowInsecureRX>(msg, secBodyBuf, sizeof(secBodyBuf), decryptedBodyOutSize)) {
        return false;
    }

    switch(firstByte) // Switch on type.
    {
        case 'O' | 0x80: // Basic OpenTRV secure frame...
        {
            return (handleOTSecureFrame<h1_t, h1, frameType1, h2_t, h2, frameType2>(msg, secBodyBuf, decryptedBodyOutSize)); // handleOTSecurableFrame
        }

          // Reject unrecognised type, though fall through potentially to recognise other encodings.
        default: break;
    }

    // Failed to parse; let another handler try.
    return(false);
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
 * @param   allowInsecureRX: Allow RX of insecure frames. Defaults to false.
 */
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         bool allowInsecureRX = false>
static void decodeAndHandleRawRXedMessage(const uint8_t * const msg)
{
    const uint8_t msglen = msg[-1];

//  // TODO: consider extracting hash of all message data (good/bad) and injecting into entropy pool.
//#if 0 && defined(DEBUG)
//  OTRadioLink::printRXMsg(p, msg-1, msglen+1); // Print len+frame.
//#endif

    if(msglen < 2) { return; } // Too short to be useful, so ignore.

   // Length-first OpenTRV securable-frame format...
    if(decodeAndHandleOTSecurableFrame<h1_t, h1, frameType1,
                                       allowInsecureRX>
                                       (msg)) { return; }

//  // Unparseable frame: drop it; possibly log it as an error.
//#if 0 && defined(DEBUG) && !defined(ENABLE_TRIMMED_MEMORY)
//    p->print(F("!RX bad msg, len+prefix: ")); OTRadioLink::printRXMsg(p, msg-1, min(msglen+1, 8));
//#endif
  return;
}
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         typename h2_t, h2_t &h2, uint8_t frameType2,
         bool allowInsecureRX = false>
static void decodeAndHandleRawRXedMessage(const uint8_t * const msg)
{
    const uint8_t msglen = msg[-1];
    if(msglen < 2) { return; } // Too short to be useful, so ignore.
   // Length-first OpenTRV securable-frame format...
    if(decodeAndHandleOTSecurableFrame<h1_t, h1, frameType1,
                                       h2_t, h2, frameType2,
                                       allowInsecureRX>
                                       (msg)) { return; }
    return;
}

/**
 * @brief   Abstract interface for handling message queues.
 *          Provided as V0p2 is still spagetti (20170608).
 */
class OTMessageQueueHandlerBase {
public:
    virtual bool handle(bool /*wakeSerialIfNeeded*/, OTRadioLink */*rl*/) { return false; };
};

#ifdef ARDUINO_ARCH_AVR
/*
 * @param   msg: Secure frame to authenticate, decrypt and handle.
 * @param   h1_t: Type of h1
 * @param   h1: Handler object.
 * @param   frameTypen: Frame tyoe to be supplied to 1.
 * @param   pollIO: Function pollIO in V0p2. FIXME work out how to bring pollIO into library.
 * @param   baud: Serial baud for serial output.
 * @param   allowInsecureRX: Allow RX of insecure frames. Defaults to false.
 */
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         bool (*pollIO) (bool), uint16_t baud,
         bool allowInsecureRX = false>
class OTMessageQueueHandler final: public OTMessageQueueHandlerBase {
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
    virtual bool handle(bool wakeSerialIfNeeded, OTRadioLink *rl) override
    {
        // Avoid starting any potentially-slow processing very late in the minor cycle.
        // This is to reduce the risk of loop overruns
        // at the risk of delaying some processing
        // or even dropping some incoming messages if queues fill up.
        // Decoding (and printing to serial) a secure 'O' frame takes ~60 ticks (~0.47s).
        // Allow for up to 0.5s of such processing worst-case,
        // ie don't start processing anything later that 0.5s before the minor cycle end.
        const uint8_t sctStart = OTV0P2BASE::getSubCycleTime();
        if(sctStart >= ((OTV0P2BASE::GSCT_MAX/4)*3)) { return(false); }

        // Deal with any I/O that is queued.
        bool workDone = pollIO(true);

        // Check for activity on the radio link.
        rl->poll();

        bool neededWaking = false; // Set true once this routine wakes Serial.
        const volatile uint8_t *pb;
        if(NULL != (pb = rl->peekRXMsg())) {
            if(!neededWaking && wakeSerialIfNeeded && OTV0P2BASE::powerUpSerialIfDisabled<baud>()) { neededWaking = true; } // FIXME
            // Don't currently regard anything arriving over the air as 'secure'.
            // FIXME: shouldn't have to cast away volatile to process the message content.
            decodeAndHandleRawRXedMessage< h1_t, h1, frameType1,
                                           allowInsecureRX>
                                           ((const uint8_t *)pb);
            rl->removeRXMsg();
            // Note that some work has been done.
            workDone = true;
        }

        // Turn off serial at end, if this routine woke it.
        if(neededWaking) { OTV0P2BASE::flushSerialProductive(); OTV0P2BASE::powerDownSerial(); }

        #if 0 && defined(DEBUG)
        const uint8_t sctEnd = OTV0P2BASE::getSubCycleTime();
        const uint8_t ticks = sctEnd - sctStart;
        if(ticks > 1) {
            OTV0P2BASE::serialPrintAndFlush(ticks);
            OTV0P2BASE::serialPrintlnAndFlush();
        }
        #endif

        return(workDone);
    }
};

template<typename h1_t, h1_t &h1, uint8_t frameType1,
         typename h2_t, h2_t &h2, uint8_t frameType2,
         bool (*pollIO) (bool), uint16_t baud,
         bool allowInsecureRX = false>
class OTMessageQueueHandler2 final: public OTMessageQueueHandlerBase {
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
    virtual bool handle(bool wakeSerialIfNeeded, OTRadioLink *rl) override
    {
        // Avoid starting any potentially-slow processing very late in the minor cycle.
        // This is to reduce the risk of loop overruns
        // at the risk of delaying some processing
        // or even dropping some incoming messages if queues fill up.
        // Decoding (and printing to serial) a secure 'O' frame takes ~60 ticks (~0.47s).
        // Allow for up to 0.5s of such processing worst-case,
        // ie don't start processing anything later that 0.5s before the minor cycle end.
        const uint8_t sctStart = OTV0P2BASE::getSubCycleTime();
        if(sctStart >= ((OTV0P2BASE::GSCT_MAX/4)*3)) { return(false); }

        // Deal with any I/O that is queued.
        bool workDone = pollIO(true);

        // Check for activity on the radio link.
        rl->poll();

        bool neededWaking = false; // Set true once this routine wakes Serial.
        const volatile uint8_t *pb;
        if(NULL != (pb = rl->peekRXMsg())) {
            if(!neededWaking && wakeSerialIfNeeded && OTV0P2BASE::powerUpSerialIfDisabled<baud>()) { neededWaking = true; } // FIXME
            // Don't currently regard anything arriving over the air as 'secure'.
            // FIXME: shouldn't have to cast away volatile to process the message content.
            decodeAndHandleRawRXedMessage< h1_t, h1, frameType1,
                                           h2_t, h2, frameType2,
                                           allowInsecureRX>
                                           ((const uint8_t *)pb);
            rl->removeRXMsg();
            // Note that some work has been done.
            workDone = true;
        }

        // Turn off serial at end, if this routine woke it.
        if(neededWaking) { OTV0P2BASE::flushSerialProductive(); OTV0P2BASE::powerDownSerial(); }
        return(workDone);
    }
};
#endif // ARDUINO_ARCH_AVR

}
#endif /* UTILITY_OTRADIOLINK_MESSAGING_H_ */

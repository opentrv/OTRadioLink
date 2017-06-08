/*
 * OTRadioLink_Messagin.h
 *
 *  Created on: 18 May 2017
 *      Author: denzo
 */

#ifndef UTILITY_OTRADIOLINK_MESSAGING_H_
#define UTILITY_OTRADIOLINK_MESSAGING_H_

#include <OTV0p2Base.h>
#include <OTAESGCM.h>
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
    virtual bool frameHandler(const uint8_t *const msg,
                              const uint8_t *const decryptedBody,
                              const uint8_t decryptedBodyLen) = 0;
};

/**
 * @class   Handler for printing to serial
 */
//template <Print &p>
//class OTSerialHandler final : public OTFrameHandlerBase
//{
//public:
//    virtual bool frameHandler(const uint8_t *const msg,
//                              const uint8_t *const decryptedBody,
//                              const uint8_t decryptedBodyLen) override
//    {
//        if((0 != (decryptedBody[1] & 0x10)) && (decryptedBodyLen > 3) && ('{' == decryptedBody[2])) {
//            // XXX Feel like this should be moved somewhere else.
//            // TODO JSON output not implemented yet.
//            // Write out the JSON message, inserting synthetic ID/@ and seq/+.
//            p.print(F("{\"@\":\""));
//            for(int i = 0; i < OTV0P2BASE::OpenTRV_Node_ID_Bytes; ++i) { p.print(senderNodeID[i], HEX); }
//            p.print(F("\",\"+\":"));
//            p.print(sfh.getSeq());
//            p.print(',');
//            p.write(decryptedBody + 3, decryptedBodyLen - 3);
//            p.println('}');
//            // OTV0P2BASE::outputJSONStats(&Serial, secure, msg, msglen);
//            // Attempt to ensure that trailing characters are pushed out fully.
//            OTV0P2BASE::flushSerialProductive();
//        }
//    }
//};

/**
 * @class   Handler for relaying over radio
 */
template <typename rt_t, rt_t &rt>
class OTRadioHandler final : public OTFrameHandlerBase
{
public:
    virtual bool frameHandler(const uint8_t *const msg,
                              const uint8_t *const decryptedBody,
                              const uint8_t decryptedBodyLen) override
    {
        const uint8_t msglen = msg[-1];
        if((0 != (decryptedBody[1] & 0x10)) && (decryptedBodyLen > 3) && ('{' == decryptedBody[2])) {
            return rt.queueToSend(msg, msglen, 0, (OTRadioLink::OTRadioLink::TXpower) 0 );
        }
    }
};


/**
 * @class   Handler for operating boiler driver.
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
          if(percentOpen <= 100) { bh.remoteCallForHeatRX(0, percentOpen, minuteCount); } // TODO call for heat valve id not passed in.
          return true;
    }
};


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
      }

    return(true); // Stop if not OK.

}


/**
 * @brief   Perform trivial validation of frame then loop through supplied handlers.
 * @param   hn_t:
 * @param   hn:
 * @param   frameTypen:
 * @retval  True on success of all handlers, else false.
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
bool handleOTSecureFrame2(const uint8_t *const msg,
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
 * @brief   Try to decode an OT style secureable frame.
 * @param   msg: Message to decode
 * @param   rt:  Reference to radio to relay with.
 * @return  true on successful frame type match, false if no suitable frame was found/decoded and another parser should be tried.
 * @note    - Secure beacon frames commented to save complexity, as not currently used by any configs.
 */
template<typename h1_t, h1_t &h1, uint8_t frameType1,
         typename h2_t, h2_t &h2, uint8_t frameType2,
         bool allowInsecureRX = false>
bool decodeAndHandleOTSecurableFrame(const uint8_t * const msg)
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
          return (handleOTSecureFrame2<h1_t, h1, frameType1, h2_t, h2, frameType2>(msg, secBodyBuf, decryptedBodyOutSize)); // handleOTSecurableFrame
      }

    // Reject unrecognised type, though fall through potentially to recognise other encodings.
    default: break;
    }

  // Failed to parse; let another handler try.
  return(false);
  }


}


// Old function
//template<typename bh_t, bool enableBoilerHub = false, bool enableRadioRelay >
//static bool handleOTSecurableFrame( Print *p, const uint8_t * const msg,
//                                    const uint8_t * const decryptedBody, const uint8_t decryptedBodyLen,
//                                    const uint8_t minuteCount,
//                                    bh_t &bh, OTRadioLink &rt)
//{
//    const uint8_t msglen = msg[-1];
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("'O'");
//#endif
//      if(decryptedBodyLen < 2)
//        {
//#if 1 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("!RX O short"); // "O' frame too short.
//#endif
//        return false;
//        }
//
//      // If acting as a boiler hub
//      // then extract the valve %age and pass to boiler controller
//      // but use only if valid.
//      // Ignore explicit call-for-heat flag for now.
//      if (enableBoilerHub) {
//          const uint8_t percentOpen = decryptedBody[0];
//          if(percentOpen <= 100) { bh.remoteCallForHeatRX(0, percentOpen, minuteCount); } // TODO call for heat valve id not passed in.
//      }
//
//      // If the frame contains JSON stats
//      // then forward entire secure frame as-is across the secondary radio relay link,
//      // else print directly to console/Serial.
//      if((0 != (decryptedBody[1] & 0x10)) && (decryptedBodyLen > 3) && ('{' == decryptedBody[2])) {
//          if (enableRadioRelay) {
//              rt.queueToSend(msg, msglen);
//          } else {
//              // XXX Feel like this should be moved somewhere else.
//              // TODO JSON output not implemented yet.
//            // Write out the JSON message, inserting synthetic ID/@ and seq/+.
////            p->print(F("{\"@\":\""));
////            for(int i = 0; i < OTV0P2BASE::OpenTRV_Node_ID_Bytes; ++i) { p->print(senderNodeID[i], HEX); }
////            p->print(F("\",\"+\":"));
////            p->print(sfh.getSeq());
////            p->print(',');
//            p->write(decryptedBody + 3, decryptedBodyLen - 3);
//            p->println('}');
//            // OTV0P2BASE::outputJSONStats(&Serial, secure, msg, msglen);
//            // Attempt to ensure that trailing characters are pushed out fully.
//            OTV0P2BASE::flushSerialProductive();
//          }
//      }
//      return(true);
//}


#endif /* UTILITY_OTRADIOLINK_MESSAGING_H_ */

/*
 * OTRadioLink_Messagin.h
 *
 *  Created on: 18 May 2017
 *      Author: denzo
 */

#ifndef UTILITY_OTRADIOLINK_MESSAGING_H_
#define UTILITY_OTRADIOLINK_MESSAGING_H_

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <OTV0p2Base.h>
#include <OTAESGCM.h>
#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"
#include "OTRadioLink_OTRadioLink.h"


namespace OTRadioLink
{



//#if defined(ENABLE_RADIO_RX) && defined(ENABLE_OTSECUREFRAME_ENCODING_SUPPORT) // && defined(ENABLE_FAST_FRAMED_CARRIER_SUPPORT)
// Returns true on successful frame type match, false if no suitable frame was found/decoded and another parser should be tried.
static bool decodeAndHandleOTSecureableFrame(Print * /*p*/, const bool /*secure*/, const uint8_t * const msg, OTRadioLink &rt)
  {
  const uint8_t msglen = msg[-1];
  const uint8_t firstByte = msg[0];

  // Validate structure of header/frame first.
  // This is quick and checks for insane/dangerous values throughout.
  SecurableFrameHeader sfh;
  const uint8_t l = sfh.checkAndDecodeSmallFrameHeader(msg-1, msglen+1);
  // If isOK flag is set false for any reason, frame is broken/unsafe/unauth.
  bool isOK = (l > 0);
//#if 0 && defined(DEBUG)
//if(!isOK) { DEBUG_SERIAL_PRINTLN_FLASHSTRING("!RX bad secure header"); }
//#endif
  // If failed this early and this badly, let someone else try parsing the message buffer...
  if(!isOK) { return(false); }

  // Buffer for receiving secure frame body.
  // (Non-secure frame bodies should be read directly from the frame buffer.)
  uint8_t secBodyBuf[ENC_BODY_SMALL_FIXED_PTEXT_MAX_SIZE];
  uint8_t decryptedBodyOutSize = 0;

  // Validate integrity of frame (CRC for non-secure, auth for secure).
  const bool secureFrame = sfh.isSecure();
  // Body length after any decryption, etc.
//  uint8_t receivedBodyLength; XXX
  // TODO: validate entire message, eg including auth, or CRC if insecure msg rcvd&allowed.
//#if defined(ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED) // Allow insecure.
//  // Only bother to check insecure form (and link code to do so) if insecure RX is allowed.
//  if(!secureFrame)
//    {
//    // Reject if CRC fails.
//    if(0 == decodeNonsecureSmallFrameRaw(&sfh, msg-1, msglen+1))
//      { isOK = false; }
//    else
//      { receivedBodyLength = sfh.bl; }
//    }
//#else  // ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
  // Only allow secure frames by default.
  if(!secureFrame) { isOK = false; }
//#endif  // ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED
  // Validate (authenticate) and decrypt body of secure frames.
  uint8_t key[16];
  if(secureFrame && isOK)
    {
    // Get the 'building' key.
    if(!OTV0P2BASE::getPrimaryBuilding16ByteSecretKey(key))
      {
      isOK = false;
      OTV0P2BASE::serialPrintlnAndFlush(F("!RX key"));
      }
    }
  uint8_t senderNodeID[OTV0P2BASE::OpenTRV_Node_ID_Bytes];
  if(secureFrame && isOK)
    {
    // Look up full ID in associations table,
    // validate RX message counter,
    // authenticate and decrypt,
    // update RX message counter.
    isOK = (0 != SimpleSecureFrame32or0BodyRXV0p2::getInstance().decodeSecureSmallFrameSafely(&sfh, msg-1, msglen+1,
                                            OTAESGCM::fixed32BTextSize12BNonce16BTagSimpleDec_DEFAULT_STATELESS,
                                            NULL, key,
                                            secBodyBuf, sizeof(secBodyBuf), decryptedBodyOutSize,
                                            senderNodeID,
                                            true));
//#if 1 // && defined(DEBUG)
    if(!isOK)
      {
      // Useful brief network diagnostics: a couple of bytes of the claimed ID of rejected frames.
      // Warnings rather than errors because there may legitimately be multiple disjoint networks.
      OTV0P2BASE::serialPrintAndFlush(F("?RX auth")); // Missing association or failed auth.
      if(sfh.getIl() > 0) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(sfh.id[0], HEX); }
      if(sfh.getIl() > 1) { OTV0P2BASE::serialPrintAndFlush(' '); OTV0P2BASE::serialPrintAndFlush(sfh.id[1], HEX); }
      OTV0P2BASE::serialPrintlnAndFlush();
      }
//#endif
    }

  if(!isOK) { return(false); } // Stop if not OK.

  // If frame still OK to process then switch on frame type.
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINT_FLASHSTRING("RX seq#");
//DEBUG_SERIAL_PRINT(sfh.getSeq());
//DEBUG_SERIAL_PRINTLN();
//#endif

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
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("'O'");
//#endif
      if(decryptedBodyOutSize < 2)
        {
//#if 1 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("!RX O short"); // "O' frame too short.
//#endif
        break;
        }
//#ifdef ENABLE_BOILER_HUB  // FIXME
//      // If acting as a boiler hub
//      // then extract the valve %age and pass to boiler controller
//      // but use only if valid.
//      // Ignore explicit call-for-heat flag for now.
//      const uint8_t percentOpen = secBodyBuf[0];
//      if(percentOpen <= 100) { remoteCallForHeatRX(0, percentOpen); } // TODO call for heat valve id not passed in.
//#endif
      // If the frame contains JSON stats
      // then forward entire secure frame as-is across the secondary radio relay link,
      // else print directly to console/Serial.
      if((0 != (secBodyBuf[1] & 0x10)) && (decryptedBodyOutSize > 3) && ('{' == secBodyBuf[2]))
        {
//#ifdef ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
        rt.queueToSend(msg, msglen);
//#else // Don't write to console/Serial also if relayed.
//        // Write out the JSON message, inserting synthetic ID/@ and seq/+.
//        Serial.print(F("{\"@\":\""));
//        for(int i = 0; i < OTV0P2BASE::OpenTRV_Node_ID_Bytes; ++i) { Serial.print(senderNodeID[i], HEX); }
//        Serial.print(F("\",\"+\":"));
//        Serial.print(sfh.getSeq());
//        Serial.print(',');
//        Serial.write(secBodyBuf + 3, decryptedBodyOutSize - 3);
//        Serial.println('}');
////        OTV0P2BASE::outputJSONStats(&Serial, secure, msg, msglen);
//        // Attempt to ensure that trailing characters are pushed out fully.
//        OTV0P2BASE::flushSerialProductive();
//#endif // ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
        }
      return(true);
      }

    // Reject unrecognised type, though fall through potentially to recognise other encodings.
    default: break;
    }

  // Failed to parse; let another handler try.
  return(false);
  }
//#endif // defined(ENABLE_OTSECUREFRAME_ENCODING_SUPPORT)

}

#endif /* UTILITY_OTRADIOLINK_MESSAGING_H_ */

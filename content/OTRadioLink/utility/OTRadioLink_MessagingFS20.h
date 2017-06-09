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
*/

#ifndef UTILITY_OTRADIOLINK_MESSAGINGFS20_H_
#define UTILITY_OTRADIOLINK_MESSAGINGFS20_H_


// FS20 stubs
// template commented as stub.
// template<typename h1_t, h1_t &h1, uint8_t frameType1>
static bool decodeAndHandleFS20Frame(const uint8_t * const /*msg*/)
{
    return(false);
}




// Copied from Messaging.cpp to preserve FS20 messaging code.

//#if defined(ENABLE_RADIO_RX) && defined(ENABLE_FHT8VSIMPLE_RX) // (defined(ENABLE_BOILER_HUB) || defined(ENABLE_STATS_RX)) && defined(ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX) // Listen for calls for heat from remote valves...
//// Handle FS20/FHT8V traffic including binary stats.
//// Returns true on success, false otherwise.
//static bool decodeAndHandleFTp2_FS20_native(Print *p, const bool secure, const uint8_t * const msg, const uint8_t msglen)
//{
//  // Decode the FS20/FHT8V command into the buffer/struct.
//  OTRadValve::FHT8VRadValveBase::fht8v_msg_t command;
//  uint8_t const *lastByte = msg+msglen-1;
//  uint8_t const *trailer = OTRadValve::FHT8VRadValveBase::FHT8VDecodeBitStream(msg, lastByte, &command);
//
//#if defined(ENABLE_BOILER_HUB)
//  // Potentially accept as call for heat only if command is 0x26 (38).
//  // Later filter on the valve being open enough for some water flow to be likely
//  // (for individual valves, and in aggregate)
//  // and for the housecode being accepted.
//  if(0x26 == command.command)
//    {
//    const uint16_t compoundHC = (((uint16_t)command.hc1) << 8) | command.hc2;
//#if 0 && defined(DEBUG)
//    p->print("FS20 RX 0x26 "); // Just notes that a 'valve %' FS20 command has been overheard.
//    p->print(command.hc1); p->print(' ');
//    p->println(command.hc2);
//#endif
//    const uint8_t percentOpen = OTRadValve::FHT8VRadValveUtil::convert255ScaleToPercent(command.extension);
//    BoilerHub.remoteCallForHeatRX(compoundHC, percentOpen, minuteCount);
//    }
//#endif
//
//  if(NULL != trailer)
//    {
//#if 0 && defined(DEBUG)
//p->print("FS20 msg HC "); p->print(command.hc1); p->print(' '); p->println(command.hc2);
//#endif
//#if defined(ENABLE_STATS_RX) // Only look for the trailer if supported.
//    // If whole FHT8V frame was OK then check if there is a valid stats trailer.
//
//    // Check for 'core' stats trailer.
//    if(OTV0P2BASE::MESSAGING_FULL_STATS_FLAGS_HEADER_MSBS == (trailer[0] & OTV0P2BASE::MESSAGING_FULL_STATS_FLAGS_HEADER_MASK))
//      {
//      OTV0P2BASE::FullStatsMessageCore_t content;
//      const uint8_t *tail = decodeFullStatsMessageCore(trailer, lastByte-trailer+1, OTV0P2BASE::stTXalwaysAll, false, &content);
//      if(NULL != tail)
//        {
//        // Received trailing stats frame!
//
//        // If ID is present then make sure it matches that implied by the FHT8V frame (else reject this trailer)
//        // else file it in from the FHT8C frame.
//        bool allGood = true;
//        if(content.containsID)
//          {
//          if((content.id0 != command.hc1) || (content.id1 != command.hc2))
//            { allGood = false; }
//          }
//        else
//          {
//          content.id0 = command.hc1;
//          content.id1 = command.hc2;
//          content.containsID = true;
//          }
//
//#if 0 && defined(DEBUG)
///*if(allGood)*/ { p->println("FS20 ts"); }
//#endif
//        // If frame looks good then capture it.
//        if(allGood) { outputCoreStats(p, false, &content); }
////            else { setLastRXErr(FHT8VRXErr_BAD_RX_SUBFRAME); }
//        // TODO: record error with mismatched ID.
//        }
//      }
//#if defined(ENABLE_MINIMAL_STATS_TXRX)
//    // Check for minimal stats trailer.
//    else if((trailer + MESSAGING_TRAILING_MINIMAL_STATS_PAYLOAD_BYTES <= lastByte) && // Enough space for minimum-stats trailer.
//       (MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MSBS == (trailer[0] & MESSAGING_TRAILING_MINIMAL_STATS_HEADER_MASK)))
//      {
//      if(verifyHeaderAndCRCForTrailingMinimalStatsPayload(trailer)) // Valid header and CRC.
//        {
//#if 0 && defined(DEBUG)
//        p->println("FS20 tsm"); // Just notes that a 'valve %' FS20 command has been overheard.
//#endif
//        trailingMinimalStatsPayload_t payload;
//        extractTrailingMinimalStatsPayload(trailer, &payload);
//        }
//      }
//#endif
//#endif // defined(ENABLE_STATS_RX)
//    }
//  return(true);
//  }
//#endif


//#ifdef ENABLE_RADIO_RX
//// Decode and handle inbound raw message (msg[-1] contains the count of bytes received).
//// A message may contain trailing garbage at the end; the decoder/router should cope.
//// The buffer may be reused when this returns,
//// so a copy should be taken of anything that needs to be retained.
//// If secure is true then this message arrived over an inherently secure channel.
//// This will write any output to the supplied Print object,
//// typically the Serial output (which must be running if so).
//// This routine is NOT allowed to alter in any way the content of the buffer passed.
//static void decodeAndHandleRawRXedMessage(const uint8_t * const msg)
//  {
//  const uint8_t msglen = msg[-1];
//
//  // TODO: consider extracting hash of all message data (good/bad) and injecting into entropy pool.
//#if 0 && defined(DEBUG)
//  OTRadioLink::printRXMsg(p, msg-1, msglen+1); // Print len+frame.
//#endif
//
//  if(msglen < 2) { return; } // Too short to be useful, so ignore.
//
//   // Length-first OpenTRV secureable-frame format...
//#if defined(ENABLE_OTSECUREFRAME_ENCODING_SUPPORT) // && defined(ENABLE_FAST_FRAMED_CARRIER_SUPPORT)
//constexpr bool allowOTSecureFrameRX = false; // ENABLE_OTSECUREFRAME_INSECURE_RX_PERMITTED  FIXME!
//  if(OTRadioLink::decodeAndHandleOTSecurableFrame<decltype(radioHandler), radioHandler, 'O',
//                                                   decltype(boilerHandler), boilerHandler, 'O',
//                                                   allowOTSecureFrameRX>
//                                                   (msg)) {
//      return;
//  }  // XXX
//#endif // ENABLE_OTSECUREFRAME_ENCODING_SUPPORT
//#ifdef ENABLE_FS20_ENCODING_SUPPORT
//  switch(firstByte)
//    {
//    default: // Reject unrecognised leading type byte.
//    case OTRadioLink::FTp2_NONE: // Reject zero-length with leading length byte.
//      break;
//
//#if defined(ENABLE_STATS_RX) && defined(ENABLE_FS20_ENCODING_SUPPORT) && defined(ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX)
//    // Stand-alone stats message.
//    case OTRadioLink::FTp2_FullStatsIDL: case OTRadioLink::FTp2_FullStatsIDH:
//      {
//#if 0 && defined(DEBUG)
//DEBUG_SERIAL_PRINTLN_FLASHSTRING("Stats IDx");
//#endif
//      // May be binary stats frame, so attempt to decode...
//      OTV0P2BASE::FullStatsMessageCore_t content;
//      // (TODO: should reject non-secure messages when expecting secure ones...)
//      const uint8_t *tail = OTV0P2BASE::decodeFullStatsMessageCore(msg, msglen, OTV0P2BASE::stTXalwaysAll, false, &content);
//      if(NULL != tail)
//         {
//         if(content.containsID)
//           {
//#if 0 && defined(DEBUG)
//           DEBUG_SERIAL_PRINT_FLASHSTRING("Stats HC ");
//           DEBUG_SERIAL_PRINTFMT(content.id0, HEX);
//           DEBUG_SERIAL_PRINT(' ');
//           DEBUG_SERIAL_PRINTFMT(content.id1, HEX);
//           DEBUG_SERIAL_PRINTLN();
//#endif
////           recordCoreStats(false, &content);
//           OTV0P2BASE::outputCoreStats(&Serial, secure, &content);
//           }
//         }
//      return;
//      }
//#endif
//
//#if defined(ENABLE_FHT8VSIMPLE_RX) // defined(ENABLE_STATS_RX) && defined(ENABLE_FS20_NATIVE_AND_BINARY_STATS_RX) && defined(ENABLE_FS20_ENCODING_SUPPORT) // Listen for calls for heat from remote valves...
//    case OTRadioLink::FTp2_FS20_native:
//      {
//      decodeAndHandleFTp2_FS20_native(p, secure, msg, msglen);
//      return;
//      }
//#endif
//
//#if defined(ENABLE_STATS_RX) && defined(ENABLE_FS20_ENCODING_SUPPORT)
//    case OTRadioLink::FTp2_JSONRaw:
//      {
//      if(-1 != OTV0P2BASE::checkJSONMsgRXCRC(msg, msglen))
//        {
//#ifdef ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
//        // Initial pass for Brent.
//        // Strip trailing high bit and CRC.  Not very nice, but it'll have to do.
//        uint8_t buf[OTV0P2BASE::MSG_JSON_ABS_MAX_LENGTH + 1];
//        uint8_t buflen = 0;
//        while(buflen < sizeof(buf))
//          {
//          const uint8_t b = msg[buflen];
//          if(('}' | 0x80) == b) { buf[buflen++] = '}'; break; } // End of JSON found.
//          buf[buflen++] = b;
//          }
//        // FIXME should only relay authenticated (and encrypted) traffic.
//        // Relay stats frame over secondary radio.
//        SecondaryRadio.queueToSend(buf, buflen);
//#else // Don't write to console/Serial also if relayed.
//        // Write out the JSON message.
//        OTV0P2BASE::outputJSONStats(&Serial, secure, msg, msglen);
//        // Attempt to ensure that trailing characters are pushed out fully.
//        OTV0P2BASE::flushSerialProductive();
//#endif // ENABLE_RADIO_SECONDARY_MODULE_AS_RELAY
//        }
//      return;
//      }
//#endif
//    }
//#endif // ENABLE_FS20_ENCODING_SUPPORT
//
//  // Unparseable frame: drop it; possibly log it as an error.
//#if 0 && defined(DEBUG) && !defined(ENABLE_TRIMMED_MEMORY)
//  p->print(F("!RX bad msg, len+prefix: ")); OTRadioLink::printRXMsg(p, msg-1, min(msglen+1, 8));
//#endif
//  return;
//  }
//#endif // ENABLE_RADIO_RX

#endif /* UTILITY_OTRADIOLINK_MESSAGINGFS20_H_ */

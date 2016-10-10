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

Author(s) / Copyright (s): Damon Hart-Davis 2014--2016
*/

/*
 Lightweight support for generating compact JSON stats.
 */

#include <stddef.h>
#include <string.h>

#include "OTV0P2BASE_JSONStats.h"

#include "OTV0P2BASE_CRC.h"
#include "OTV0P2BASE_EEPROM.h"
#include "OTV0P2BASE_QuickPRNG.h"


namespace OTV0P2BASE
{


// Returns true unless the buffer clearly does not contain a possible valid raw JSON message.
// This message is expected to be one object wrapped in '{' and '}'
// and containing only ASCII printable/non-control characters in the range [32,126].
// The message must be no longer than MSG_JSON_MAX_LENGTH excluding trailing null.
// This only does a quick validation for egregious errors.
bool quickValidateRawSimpleJSONMessage(const char * const buf)
  {
  if('{' != buf[0]) { return(false); }
  // Scan up to maximum length for terminating '}'.
  const char *p = buf + 1;
  for(int i = 1; i < MSG_JSON_MAX_LENGTH; ++i)
    {
    const char c = *p++;
    // With a terminating '}' (followed by '\0') the message is superficially valid.
    if(('}' == c) && ('\0' == *p)) { return(true); }
    // Non-printable/control character makes the message invalid.
    if((c < 32) || (c > 126)) { return(false); }
    // Premature end of message renders it invalid.
    if('\0' == c) { return(false); }
    }
  return(false); // Bad (unterminated) message.
  }

// Adjusts null-terminated text JSON message up to MSG_JSON_MAX_LENGTH bytes (not counting trailing '\0') for TX.
// Sets high-bit on final '}' to make it unique, checking that all others are clear.
// Computes and returns 0x5B 7-bit CRC in range [0,127]
// or 0xff if the JSON message obviously invalid and should not be TXed.
// The CRC is initialised with the initial '{' character.
// NOTE: adjusts content in place.
#define adjustJSONMsgForTXAndComputeCRC_ERR 0xff // Error return value.
uint8_t adjustJSONMsgForTXAndComputeCRC(char * const bptr)
  {
  // Do initial quick validation before computing CRC, etc,
  if(!quickValidateRawSimpleJSONMessage(bptr)) { return(adjustJSONMsgForTXAndComputeCRC_ERR); }
//  if('{' != *bptr) { return(adjustJSONMsgForTXAndComputeCRC_ERR); }
  bool seenTrailingClosingBrace = false;
  uint8_t crc = '{';
  for(char *p = bptr; *++p; ) // Skip first char ('{'); loop until '\0'.
    {
    const char c = *p;
//    if(c & 0x80) { return(adjustJSONMsgForTXAndComputeCRC_ERR); } // No high-bits should be set!
    if(('}' == c) && ('\0' == *(p+1)))
      {
      seenTrailingClosingBrace = true;
      const char newC = c | 0x80;
      *p = newC; // Set high bit.
      crc = crc7_5B_update(crc, (uint8_t)newC); // Update CRC.
      return(crc);
      }
    crc = crc7_5B_update(crc, (uint8_t)c); // Update CRC.
    }
  if(!seenTrailingClosingBrace) { return(adjustJSONMsgForTXAndComputeCRC_ERR); } // Missing ending '}'.
  return(crc);
  }


// Send (valid) JSON to specified print channel, terminated with "}\0" or '}'|0x80, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
void outputJSONStats(Print *p, bool secure, const uint8_t * const json, const uint8_t bufsize)
  {
#if 0 && defined(DEBUG)
  if(NULL == json) { panic(); }
#endif
  const uint8_t * const je = json + bufsize;
  for(const uint8_t *jp = json; ; ++jp)
    {
    if(jp >= je) { p->println(F(" ... bad")); return; } // Deliberately don't terminate with '}'...
    if(('}' | 0x80) == *jp) { break; }
    if(('}' == *jp) && ('\0' == jp[1])) { break; }
    p->print((char) *jp);
    }
  // Terminate the output.
  p->println('}');
  }

// Checks received raw JSON message followed by CRC, up to MSG_JSON_ABS_MAX_LENGTH chars.
// Returns length including bounding '{' and '}'|0x80 iff message superficially valid
// (essentially as checked by quickValidateRawSimpleJSONMessage() for an in-memory message)
// and that the CRC matches as computed by adjustJSONMsgForTXAndComputeCRC(),
// else returns -1 (accepts 0 or 0x80 where raw CRC is zero).
// Does not adjust buffer content.
//#define checkJSONMsgRXCRC_ERR -1
int8_t checkJSONMsgRXCRC(const uint8_t * const bptr, const uint8_t bufLen)
  {
  if('{' != *bptr) { return(checkJSONMsgRXCRC_ERR); }
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINT_FLASHSTRING("checkJSONMsgRXCRC_ERR()... {");
#endif
  uint8_t crc = '{';
  // Scan up to maximum length for terminating '}'-with-high-bit.
  const uint8_t ml = OTV0P2BASE::fnmin(MSG_JSON_ABS_MAX_LENGTH, bufLen);
  const uint8_t *p = bptr + 1;
  for(int i = 1; i < ml; ++i)
    {
    const char c = *p++;
    crc = crc7_5B_update(crc, (uint8_t)c); // Update CRC.
//#ifdef ALLOW_RAW_JSON_RX
    if(('}' == c) && ('\0' == *p))
      {
      // Return raw message as-is!
#if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINTLN_FLASHSTRING("} OK raw");
#endif
      return(i+1);
      }
//#endif
    // With a terminating '}' (followed by '\0') the message is superficially valid.
    if((((char)('}' | 0x80)) == c) && ((crc == *p) || ((0 == crc) && (0x80 == *p))))
      {
#if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINTLN_FLASHSTRING("} OK with CRC");
#endif
      return(i+1);
      }
    // Non-printable/control character makes the message invalid.
    if((c < 32) || (c > 126))
      {
#if 0 && defined(DEBUG)
      DEBUG_SERIAL_PRINT_FLASHSTRING(" bad: char 0x");
      DEBUG_SERIAL_PRINTFMT(c, HEX);
      DEBUG_SERIAL_PRINTLN();
#endif
      return(checkJSONMsgRXCRC_ERR);
      }
#if 0 && defined(DEBUG)
    DEBUG_SERIAL_PRINT(c);
#endif
    }
#if 0 && defined(DEBUG)
  DEBUG_SERIAL_PRINTLN_FLASHSTRING(" bad: unterminated");
#endif
  return(checkJSONMsgRXCRC_ERR); // Bad (unterminated) message.
  }


// Print a single char to a bounded buffer; returns 1 if successful, else 0 if full.
size_t BufPrint::write(const uint8_t c)
  {
  if(size < capacity) { b[size++] = c; b[size] = '\0'; return(1); }
  return(0);
  }

// Returns true iff if a valid key for OpenTRV subset of JSON.
// Rejects keys containing " or \ or any chars outside the range [32,126]
// to avoid having to escape anything.
bool isValidSimpleStatsKey(const SimpleStatsKey key)
  {
  if(NULL == key) { return(false); }
  for(const char *s = key; '\0' != *s; ++s)
    {
    const char c = *s;
    if((c < 32) || (c > 126) || ('"' == c) || ('\\' == c)) { return(false); }
    }
  return(true);
  }

// Returns pointer to stats tuple with given (non-NULL) key if present, else NULL.
// Does a simple linear search.
SimpleStatsRotationBase::DescValueTuple * SimpleStatsRotationBase::findByKey(const SimpleStatsKey key) const
  {
  for(int i = 0; i < nStats; ++i)
    {
    DescValueTuple * const p = stats + i;
    if(0 == strcmp(p->descriptor.key, key)) { return(p); }
    }
  return(NULL); // Not found.
  }

// Remove given stat and properties.
// True iff the item existed and was removed.
bool SimpleStatsRotationBase::remove(const SimpleStatsKey key)
  {
  DescValueTuple *p = findByKey(key);
  if(NULL == p) { return(false); }
  // If it needs to be removed and is not the last item
  // then move the last item down into its slot.
  const bool lastItem = ((p - stats) == (nStats - 1));
  if(!lastItem) { *p = stats[nStats-1]; }
  // We got rid of one!
  // TODO: possibly explicitly destroy/overwrite the removed one at the end.
  --nStats;
  return(true);
  }

// Create/update stat/key with specified descriptor/properties.
// The name is taken from the descriptor.
bool SimpleStatsRotationBase::putDescriptor(const GenericStatsDescriptor &descriptor)
  {
  if(!isValidSimpleStatsKey(descriptor.key)) { return(false); }
  DescValueTuple *p = findByKey(descriptor.key);
  // If item already exists, update its properties.
  if(NULL != p) { p->descriptor = descriptor; }
  // Else if not yet at capacity then add this new item at the end.
  // Don't mark it as changed since its value may not yet be meaningful
  else if(nStats < capacity)
    {
    p = stats + (nStats++);
    *p = DescValueTuple();
    p->descriptor = descriptor;
    }
  // Else failed: no space to add a new item.
  else { return(false); }
  return(true); // OK
  }

// Create/update value for given stat/key.
// If properties not already set and not supplied then stat will get defaults.
// If descriptor is supplied then its key must match (and the descriptor will be copied).
// True if successful, false otherwise (eg capacity already reached).
bool SimpleStatsRotationBase::put(const SimpleStatsKey key, const int newValue, const bool statLowPriority)
  {
  if(!isValidSimpleStatsKey(key))
    {
#if 0 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("Bad JSON key ");
DEBUG_SERIAL_PRINT(key);
DEBUG_SERIAL_PRINTLN();
#endif
    return(false);
    }

  DescValueTuple *p = findByKey(key);
  // If item already exists, update it.
  if(NULL != p)
    {
    // Update the value and mark as changed if changed.
    if(p->value != newValue)
      {
      p->value = newValue;
      p->flags.changed = true;
      }
    // Update done!
    return(true);
    }

  // If not yet at capacity then add this new item at the end.
  // Mark it as changed to prioritise seeing it in the JSON output.
  if(nStats < capacity)
    {
    p = stats + (nStats++);
    *p = DescValueTuple();
    p->value = newValue;
    p->flags.changed = true;
    // Copy descriptor .
    p->descriptor = GenericStatsDescriptor(key, statLowPriority);
    // Addition of new field done!
    return(true);
    }

#if 1 && defined(DEBUG)
DEBUG_SERIAL_PRINT_FLASHSTRING("Too many keys: ");
DEBUG_SERIAL_PRINT(key);
DEBUG_SERIAL_PRINTLN();
#endif
  return(false); // FAILED: full.
  }

//#if defined(ALLOW_JSON_OUTPUT)
// Print an object field "name":value to the given buffer.
size_t SimpleStatsRotationBase::print(BufPrint &bp, const SimpleStatsRotationBase::DescValueTuple &s, bool &commaPending) const
  {
  size_t w = 0;
  if(commaPending) { w += bp.print(','); }
  w += bp.print('"');
  w += bp.print(s.descriptor.key); // Assumed not to need escaping in any way.
  w += bp.print('"');
  w += bp.print(':');
  w += bp.print(s.value);
  commaPending = true;
  return(w);
  }
//#endif


// True if any changed values are pending (not yet written out).
bool SimpleStatsRotationBase::changedValue()
  {
  DescValueTuple const *p = stats + nStats;
  for(int8_t i = nStats; --i > 0; )
    { if((--p)->flags.changed) { return(true); } }
  return(false);
  }

// Write stats in JSON format to provided buffer; returns the non-zero JSON length if successful.
// Output starts with an "@" (ID) string field,
// then and optional count (if enabled),
// then the tracked stats as space permits,
// attempting to give priority to high-priority and changed values,
// allowing a potentially large set of values to my multiplexed over time
// into a constrained size/bandwidth message.
//
//   * buf  is the byte/char buffer to write the JSON to; never NULL
//   * bufSize is the capacity of the buffer starting at buf in bytes;
//       should be two (2) greater than the largest JSON output to be generated
//       to allow for a trailing null and one extra byte/char to ensure that the message is not over-large
//   * sensitivity  CURRENTLY IGNORED threshold below which (sensitive) stats will not be included; 0 means include everything
//   * maximise  if true attempt to maximise the number of stats squeezed into each frame,
//       potentially at the cost of significant CPU time
//   * suppressClearChanged  if true then 'changed' flag for included fields is not cleared by this
//       allowing them to continue to be treated as higher priority
uint8_t SimpleStatsRotationBase::writeJSON(uint8_t *const buf, const uint8_t bufSize, const uint8_t sensitivity,
                                           const bool maximise, const bool suppressClearChanged)
  {
#ifdef DEBUG
  if(NULL == buf) { panic(0); } // Should never happen.
#endif
  // Minimum size is for {"@":""} plus null plus extra padding char/byte to check for overrun.
  if(bufSize < 10) { return(0); } // Failed.

  // Write/print to buffer passed in.
  BufPrint bp((char *)buf, bufSize);
  // True if field has been written and will need a ',' if another field is written.
  bool commaPending = false;

  // Start object.
  bp.print('{');

  // Write ID first unless disabled entirely by being set to an empty string.
  if((NULL == id) || ('\0' != *id))
    {
    // If an explicit ID is supplied then use it
    // else use the first two bytes of the node ID if accessible.
    bp.print(F("\"@\":\""));
    if(NULL != id) { bp.print(id); } // Value has to be 'safe' (eg no " nor \ in it).
#ifdef V0P2BASE_EE_START_ID // TODO: improve logic/portability
    else
      {
      const uint8_t id1 = eeprom_read_byte(0 + (uint8_t *)V0P2BASE_EE_START_ID);
      const uint8_t id2 = eeprom_read_byte(1 + (uint8_t *)V0P2BASE_EE_START_ID);
      bp.print(hexDigit(id1 >> 4));
      bp.print(hexDigit(id1));
      bp.print(hexDigit(id2 >> 4));
      bp.print(hexDigit(id2));
      }
#endif
    bp.print('"');
    commaPending = true;
    }

  // Write count next iff enabled.
  if(c.enabled)
    {
    if(commaPending) { bp.print(','); commaPending = false; }
    bp.print(F("\"+\":"));
    bp.print(c.count);
    commaPending = true;
    }

  // Be prepared to rewind back to logical start of buffer.
  bp.setMark();

  bool gotHiPri = false;
  uint8_t hiPriIndex = 0;
//  bool gotLoPri = false;  // (DE20161010) Commented to fix 'unused variable' warning. Goes out of scope without anything ever reading it.
  uint8_t loPriIndex = 0;
  if(nStats != 0)
    {
    // High-pri/changed stats.
    // Only do this on a portion of runs to let 'normal' stats get a look-in.
    // This happens on even-numbered runs (eg including the first, typically).
    // TX at most one high-priority item this way.
    if(0 == (c.count & 1))
      {
      uint8_t next = lastTXedHiPri;
      for(int i = nStats; --i >= 0; )
        {
        // Wrap around the end of the stats.
        if(++next >= nStats) { next = 0; }
        DescValueTuple &s = stats[next];
//        // Skip stat if too sensitive to include in this output.
//        if(sensitivity > s.descriptor.sensitivity) { continue; }
        // Skip stat if unchanged.
        if(!s.flags.changed) { continue; }
        // Found suitable stat to include in output.
        hiPriIndex = next;
        gotHiPri = true;
        // Add to JSON output.
        print(bp, s, commaPending);
        // If successful, ie still space for the closing "}\0" without running over-length,
        // then mark this as a fall-back, else rewind and discard this item.
        if(bp.getSize() > bufSize - 3) { bp.rewind(); break; }
        else
          {
          bp.setMark();
          lastTXed = lastTXedHiPri = hiPriIndex;
          if(!suppressClearChanged) { stats[hiPriIndex].flags.changed = false; }
          break;
          }
        /* if(!maximise) */ { break; }
        }
      }

    // Insert normal-priority stats if space left.
    // Rotate through all eligible stats round-robin,
    // adding one to the end of the current message if possible,
    // checking first the item indexed after the previous one sent.
//    if(!gotHiPri)
      {
      uint8_t next = lastTXedLoPri;
      for(int i = nStats; --i >= 0; )
        {
        // Wrap around the end of the stats.
        if(++next >= nStats) { next = 0; }
        // Avoid re-transmitting the very last thing TXed unless there in only one item!
        if((lastTXed == next) && (nStats > 1)) { continue; }
        // Avoid transmitting the hi-pri item just sent if any.
        if(gotHiPri && (hiPriIndex == next)) { continue; }
        DescValueTuple &s = stats[next];
//        // Skip stat if too sensitive to include in this output.
//        if(sensitivity > s.descriptor.sensitivity) { continue; }
        // If low priority and unchanged then skip the chance to TX some of the time,
        // eg if a high-priority item was included already (so space is at a premium),
        // or just randomly.
        if(s.descriptor.lowPriority && !s.flags.changed && (gotHiPri || randRNG8NextBoolean())) { continue; }
        // Found suitable stat to include in output.
        loPriIndex = next;
//        gotLoPri = true;  // (DE20161010) Commented to fix 'unused variable' warning. Goes out of scope without anything ever reading it.
        // Add to JSON output.
        print(bp, s, commaPending);
        // If successful then mark this as a fall-back, else rewind and discard this item.
        // If successful, ie still space for the closing "}\0" without running over-length
        // then mark this as a fall-back, else rewind and discard this item.
        if(bp.getSize() > bufSize - 3) { bp.rewind(); break; }
        else
          {
          bp.setMark();
          lastTXed = lastTXedLoPri = loPriIndex;
          if(!suppressClearChanged) { stats[loPriIndex].flags.changed = false; }
          }
        if(!maximise) { break; }
        }
      }
    }

  // TODO: maximise.

  // Terminate object.
  bp.print('}');
#if 0
  DEBUG_SERIAL_PRINT_FLASHSTRING("JSON: ");
  DEBUG_SERIAL_PRINT((char *)buf);
  DEBUG_SERIAL_PRINTLN();
#endif
//  if(w >= (size_t)(bufSize-1))
  if(bp.isFull())
    {
    // Overrun, so failed/aborted.
    // Shouldn't really be possible unless buffer far far too small.
    *buf = '\0';
    return(0);
    }

  // On successfully creating output, update some internal state including success count.
  ++c.count;

  return(bp.getSize()); // Success!
  }


} // OTV0P2BASE

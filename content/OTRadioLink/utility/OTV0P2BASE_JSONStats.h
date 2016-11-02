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

#ifndef OTV0P2BASE_JSONSTATS_H
#define OTV0P2BASE_JSONSTATS_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "OTV0P2BASE_ArduinoCompat.h"
#endif

#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// Maximum length of JSON (text) message payload.
// A little bit less than a power of 2
// to enable packing along with other info.
// A little bit smaller than typical radio module frame buffers (eg RFM23B) of 64 bytes
// to allow other explicit preamble and postamble (such as CRC) to be added,
// and to allow time from final byte arriving to collect the data without overrun.
//
// Absolute maximum, eg with RFM23B / FS20 OOK carrier (and interrupt-serviced RX at hub).
static const uint8_t MSG_JSON_ABS_MAX_LENGTH = 55;
// Typical/recommended maximum.
static const uint8_t MSG_JSON_MAX_LENGTH = 54;
// Maximum for frames in 'secure' format, eg with authentication and encryption wrappers.
// Fits in a 32-byte (256-bit) typical encrypted block
// minus some overheads, padding, etc,
// but usually can dispense with one or both of
// ID ("@") and sequence number ("+") fields in secure frame
// since they can be recreated from the frame information.
static const uint8_t MSG_JSON_MAX_LENGTH_SECURE = 30;

// First character of raw JSON object { ... } in frame or on serial.
static const uint8_t MSG_JSON_LEADING_CHAR = ('{');


// Key used for SimpleStatsRotation items.
typedef const char *SimpleStatsKey;

// Returns true iff if a valid key for our subset of JSON.
// Rejects keys containing " or \ or any chars outside the range [32,126]
// to avoid having to escape anything.
bool isValidSimpleStatsKey(SimpleStatsKey key);

// Generic stats descriptor.
// Includes last value transmitted (to allow changed items to be sent selectively).
struct GenericStatsDescriptor final
  {
    // Create generic (integer) stats instance.
    // The name must be a valid printable ASCII7 char [32,126] name
    // and the pointer too it must remain valid until this instance
    // and all copies have been disposed of (so is probably best a static string).
    // By default the statistic is normal priority.
    // Sensitivity by default does not allow TX unless at minimal privacy level.
    GenericStatsDescriptor(const char * const statKey,
                           const bool statLowPriority = false)
                           // const uint8_t statSensitivity = 1)
      : key(statKey), lowPriority(statLowPriority) // , sensitivity(statSensitivity)
    { }

    // Null-terminated short stat/key name.
    // Should generally be of form "x" where x is a single letter (case sensitive) for a unitless quantity,
    // or "x|u" where x is the name followed by a vertical bar and the units,
    // eg "B|cV" for battery voltage in centi-volts.
    // This pointer must be to static storage, eg does not need lifetime management.
    SimpleStatsKey key;

    // If true, this statistic has low priority/importance and should be sent infrequently.
    // This is a way of saving TX bandwidth for more important stats.
    // Low priority items will usually be treated as normal when they change, ie sent quickly.
    // Candidates for this flag include slowly changing stats such as battery voltage,
    // and nominally redundant stats that can be derived from others
    // such as cumulative valve movement (can be deduced from valve % samples)
    // and hours vacancy (can be deduced from hours since last occupancy).
    bool lowPriority;

//    // Device sensitivity threshold has to be at or below this for stat to be sent.
//    // The default is to allow the stat to be sent unless device is in default maximum privacy mode.
//    uint8_t sensitivity;
  };

// Print to a bounded buffer.
class BufPrint final : public Print
  {
  private:
    char * const b;
    const uint8_t capacity;
    uint8_t size;
    uint8_t mark;
  public:
    // Wrap around a buffer of size bufSize-1 chars and a trailing '\0'.
    // The buffer must be of at least size 1.
    // A buffer of size n can accommodate n-1 characters.
    BufPrint(char *buf, uint8_t bufSize) : b(buf), capacity(bufSize-1), size(0), mark(0) { buf[0] = '\0'; }
    // Print a single char to a bounded buffer; returns 1 if successful, else 0 if full.
    virtual size_t write(uint8_t c) override
        { if(size < capacity) { b[size++] = c; b[size] = '\0'; return(1); } else { return(0); } }
    // True if buffer is completely full.
    bool isFull() const { return(size == capacity); }
    // Get size/chars already in the buffer, not including trailing '\0'.
    uint8_t getSize() const { return(size); }
    // Set to record good place to rewind to if necessary.
    void setMark() { mark = size; }
    // Rewind to previous good position, clearing newer text.
    void rewind() { size = mark; b[size] = '\0'; }
  };

// Manage sending of stats, possibly by rotation to keep frame sizes small.
// This will try to prioritise sending of changed and important values.
// This is primarily expected to support JSON stats,
// but a hook for other formats such as binary may be provided.
// The template parameter is the maximum number of values to be sent in one frame,
// beyond the compulsory (nominally unique) node ID.
// The total number of statistics that can be handled is limited to 255.
// Not thread-/ISR- safe.
class SimpleStatsRotationBase
  {
  public:
    // Create/update value for given stat/key.
    // If properties not already set and not supplied then stat will get defaults.
    // If descriptor is supplied then its key must match (and the descriptor will be copied).
    // True if successful, false otherwise (eg capacity already reached).
    bool put(SimpleStatsKey key, int newValue, bool statLowPriority = false);

    // Create/update value for the given sensor.
    // True if successful, false otherwise (eg capacity already reached).
    template <class T> bool put(const OTV0P2BASE::Sensor<T> &s, bool statLowPriority = false)
        { return(put(s.tag(), s.get(), statLowPriority)); }

    // Create/update stat/key with specified descriptor/properties.
    // The name is taken from the descriptor.
    bool putDescriptor(const GenericStatsDescriptor &descriptor);

    // Remove given stat and properties.
    // True iff the item existed and was removed.
    bool remove(SimpleStatsKey key);

    // Set ID to given value, or NULL to use first 2 bytes of system ID; returns false if ID unsafe.
    // If NULL (the default) then dynamically generate the system ID,
    // eg house code as two bytes of hex if set, else first two bytes of binary ID as hex.
    // If ID is non-NULL but points to an empty string then no ID is inserted at all.
    // The lifetime of the pointed to string must exceed that of this instance.
    bool setID(const char * const _id)
      {
      if(isValidSimpleStatsKey(_id)) { id = _id; return(true); }
      return(false); // Unsafe value.
      }

    // Get number of distinct fields/keys held.
    uint8_t size() { return(nStats); }

    // True if no stats items being managed.
    // May usefully indicate that the structure needs to be populated.
    bool isEmpty() { return(0 == nStats); }

    // True if any changed values are pending (not yet written out).
    bool changedValue();

    // Iff true enable the count ("+") field and display immediately after the "@"/ID field.
    // The unsigned count increments as a successful write() operation completes,
    // and wraps after 63 (to limit space), potentially allowing easy detection of lost stats/transmissions.
    void enableCount(bool enable) { c.enabled = enable; }

//#if defined(ALLOW_JSON_OUTPUT)
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
    uint8_t writeJSON(uint8_t * const buf, const uint8_t bufSize, const uint8_t sensitivity,
                      const bool maximise = false, const bool suppressClearChanged = false);
//#endif

  protected:
    struct DescValueTuple final
      {
      DescValueTuple() : descriptor(NULL), value(0) { }

      // Descriptor of this stat.
      GenericStatsDescriptor descriptor;

      // Value.
      int value;

      // Various run-time flags.
      struct Flags
        {
        Flags() : changed(false) { }

        // Set true when the value is changed.
        // Set false when the value written out,
        // ie nominally transmitted to a remote listener,
        // to allow priority to be given to sending changed values.
        bool changed /* : 1 */; // Note: bitfields are expensive in code size.
//
//        // True if included in the current putative JSON output.
//        // Initial state unimportant.
//        bool thisRun : 1;
        } flags;
      };

    // Maximum capacity including overheads.
    const uint8_t capacity;

    // Returns pointer to stat tuple with given key if present, else NULL.
    DescValueTuple *findByKey(SimpleStatsKey key) const;

    // Initialise base with appropriate storage (non-NULL) and capacity knowledge.
    SimpleStatsRotationBase(DescValueTuple *_stats, const uint8_t _capacity)
      : capacity(_capacity), stats(_stats), nStats(0),
        lastTXed(~0), lastTXedLoPri(~0), lastTXedHiPri(~0), // Show the first item on the first pass...
        id(NULL)
      { }

  private:
    // Stats to be tracked and sent; never NULL.
    // The initial nStats slots are used.
    DescValueTuple * const stats;

    // Number of stats being managed (packed at the start of the stats[] array).
    uint8_t nStats;

    // Last stat index TXed; used to avoid resending very last item redundantly.
    // Coerced into range if necessary.
    uint8_t lastTXed;

    // Last normal stat index TXed.
    // Coerced into range if necessary.
    uint8_t lastTXedLoPri;

    // Last high-priority/changed stat index TXed.
    // Coerced into range if necessary.
    uint8_t lastTXedHiPri;

    // ID as null terminated string, or NULL to use first 2 bytes of system ID.
    // Used as string value of compulsory leading "@" key/field.
    // If ID is non-NULL but points to an empty string then no ID is inserted at all.
    // Can be changed at run-time.
    const char *id;

    // Small write counter (and flag to enable its display).
    // Helps to track lost transmissions of generated stats.
    // Count field increments after a successful write,
    // and wraps back to zero after 7 (to limit space on the wire);
    // is displayed immediately after the @/ID field when enabled,
    // and missing count values suggest a lost transmission somewhere.
    // Takes minimal space (1 byte).
    struct WriteCount
      {
      WriteCount() : enabled(0), count(0) { }
      bool enabled /* : 1 */; // 1 if display of counter is enabled, else 0.
      uint8_t count : 3; // Increments on each successful write.
      } c;

    // Print an object field "name":value to the given buffer.
    size_t print(BufPrint &bp, const DescValueTuple &dvt, bool &commaPending) const;
  };

template<uint8_t MaxStats>
class SimpleStatsRotation final : public SimpleStatsRotationBase
  {
  private:
    // Stats to be tracked and sent; mandatory/priority items must be first.
    // A copy is taken of the user-supplied set of descriptions, preserving order.
    DescValueTuple stats[MaxStats];

  public:
    SimpleStatsRotation() : SimpleStatsRotationBase(stats, MaxStats) { }

    // Get capacity.
    uint8_t getCapacity() { return(MaxStats); }
  };


// Returns true unless the buffer clearly does not contain a possible valid raw JSON message.
// This message is expected to be one object wrapped in '{' and '}'
// and containing only ASCII printable/non-control characters in the range [32,126].
// The message must be no longer than MSG_JSON_MAX_LENGTH excluding trailing null.
// This only does a quick validation for egregious errors.
bool quickValidateRawSimpleJSONMessage(const char *buf);

// Adjusts null-terminated text JSON message up to MSG_JSON_MAX_LENGTH bytes (not counting trailing '\0') for TX.
// Sets high-bit on final '}' to make it unique, checking that all others are clear.
// Computes and returns 0x5B 7-bit CRC in range [0,127]
// or 0xff if the JSON message obviously invalid and should not be TXed.
// The CRC is initialised with the initial '{' character.
// NOTE: adjusts content in place.
uint8_t adjustJSONMsgForTXAndComputeCRC(char *bptr);

// Checks received raw JSON message followed by CRC, up to MSG_JSON_ABS_MAX_LENGTH chars.
// Returns length including bounding '{' and '}'|0x80 iff message superficially valid
// (essentially as checked by quickValidateRawSimpleJSONMessage() for an in-memory message)
// and that the CRC matches as computed by adjustJSONMsgForTXAndComputeCRC(),
// else returns -1 (accepts 0 or 0x80 where raw CRC is zero).
// Does not adjust buffer content.
static const int8_t checkJSONMsgRXCRC_ERR = -1;
int8_t checkJSONMsgRXCRC(const uint8_t * const bptr, const uint8_t bufLen);


// Send (valid) JSON to specified print channel, terminated with "}\0" or '}'|0x80, followed by "\r\n".
// This does NOT attempt to flush output nor wait after writing.
void outputJSONStats(Print *p, bool secure, const uint8_t *json, uint8_t bufsize = 1+OTV0P2BASE::MSG_JSON_ABS_MAX_LENGTH);


} // OTV0P2BASE

#endif // OTV0P2BASE_JSONSTATS_H

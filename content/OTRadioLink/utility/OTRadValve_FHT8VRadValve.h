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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2016
*/

/*
 * Driver for FHT8V wireless valve actuator (and FS20 protocol encode/decode).
 *
 * V0p2/AVR only.
 */

#ifndef ARDUINO_LIB_OTRADVALVE_FHT8VRADVALVE_H
#define ARDUINO_LIB_OTRADVALVE_FHT8VRADVALVE_H


#include <stdint.h>
#include <stdlib.h>

#include <OTV0p2Base.h>
#include "OTV0P2BASE_CLI.h"
#include <OTRadioLink.h>
#include "OTRadValve_AbstractRadValve.h"


// Use namespaces to help avoid collisions.
namespace OTRadValve
    {


// FHT8V radio-controlled radiator valve, using FS20 protocol.
// Most of this is tied somewhat to the AVR/V0p2 hardware,
// though some parts are portably testable,
// and there is nothing fundamental to prevent porting.
//
// http://stakeholders.ofcom.org.uk/binaries/spectrum/spectrum-policy-area/spectrum-management/research-guidelines-tech-info/interface-requirements/IR_2030-june2014.pdf
// IR 2030 - UK Interface Requirements 2030 Licence Exempt Short Range Devices
// FHT8V use at 868.35MHz is covered on p21 IR2030/1/16 2010/0168/UK Oct 2010 (ref EN 300 220 2013/752/EU Band No.48)
// 868.0 - 868.6 MHz max 25 mW e.r.p.
// Techniques to access spectrum and mitigate interference that provide at least equivalent performance
// to the techniques described in harmonised standards adopted under Directive 1999/5/EC must be used. Alternatively a duty cycle limit of 1 % may be used.
// See band 48: http://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:32013D0752&from=EN

// Some utility constants, types and methods.
#define FHT8VRadValveUtil_DEFINED
class FHT8VRadValveUtil
  {
  public:
    // Type for information content of FHT8V message.
    // Omits the address field unless it is actually used.
    typedef struct fht8v_msg
      {
      uint8_t hc1;
      uint8_t hc2;
#ifdef OTV0P2BASE_FHT8V_ADR_USED
      uint8_t address;
#endif
      uint8_t command;
      uint8_t extension;
      } fht8v_msg_t;

    // Typical FHT8V 'open' percentage, though partly depends on valve tails, etc.
    // This is set to err on the side of slightly open to allow
    // the 'linger' feature to work to help boilers dump heat with pump over-run
    // when the the boiler is turned off.
    // Actual values observed by DHD range from 6% to 25%.
    static const uint8_t TYPICAL_MIN_PERCENT_OPEN = 10;

    // Appends encoded 200us-bit representation of logical bit (true for 1, false for 0); returns new pointer.
    // If is1 is false this appends 1100 else (if is1 is true) this appends 111000
    // msb-first to the byte stream being created by FHT8VCreate200usBitStreamBptr.
    // bptr must be pointing at the current byte to update on entry which must start off as 0xff;
    // this will write the byte and increment bptr (and write 0xff to the new location) if one is filled up.
    // Partial byte can only have even number of bits present, ie be in one of 4 states.
    // Two least significant bits used to indicate how many bit pairs are still to be filled,
    // so initial 0xff value (which is never a valid complete filled byte) indicates 'empty'.
    // Exposed primarily to allow unit testing.
    static uint8_t *_FHT8VCreate200usAppendEncBit(uint8_t *bptr, const bool is1);

    // Returns 1 if there is an odd number of 1 bits in v.
    static inline uint8_t xor_parity_even_bit(uint8_t v)
        {
        v ^= (v >> 4);
        v ^= (v >> 2);
        v ^= (v >> 1);
        return(v & 1);
        }

    // For longest-possible encoded FHT8V/FS20 command in bytes plus terminating 0xff.
    static const uint8_t MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE = 46;
    // Create stream of bytes to be transmitted to FHT80V at 200us per bit, msbit of each byte first.
    // Byte stream is terminated by 0xff byte which is not a possible valid encoded byte.
    // On entry the populated FHT8V command struct is passed by pointer.
    // On exit, the memory block starting at buffer contains the low-byte, msbit-first, 0xff terminated TX sequence.
    // The maximum and minimum possible encoded message sizes are 35 (all zero bytes) and 45 (all 0xff bytes) bytes long.
    // Note that a buffer space of at least 46 bytes is needed to accommodate the longest-possible encoded message plus terminator.
    // This FHT8V messages is encoded with the FS20 protocol.
    // Returns pointer to the terminating 0xff on exit.
    static uint8_t *FHT8VCreate200usBitStreamBptr(uint8_t *bptr, const fht8v_msg_t *command);

    // Decode raw bitstream into non-null command structure passed in; returns true if successful.
    // Will return non-null if OK, else NULL if anything obviously invalid is detected such as failing parity or checksum.
    // Finds and discards leading encoded 1 and trailing 0.
    // Returns NULL on failure, else pointer to next full byte after last decoded.
    static uint8_t const *FHT8VDecodeBitStream(uint8_t const *bitStream, uint8_t const *lastByte, fht8v_msg_t *command);

    // Approximate maximum transmission (TX) time for bare FHT8V command frame in ms; strictly positive.
    // This ignores any prefix needed for particular radios such as the RFM23B.
    // ~80ms upwards.
    static const uint8_t FHT8V_APPROX_MAX_RAW_TX_MS = ((((MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE-1)*8) + 4) / 5);

    // Returns true if the supplied house code part is valid for an FHT8V valve.
    static inline bool isValidFHTV8HouseCode(const uint8_t hc) { return(hc <= 99); }


    // Helper method to convert from [0,100] %-open scale to [0,255] for FHT8V/FS20 frame.
    // Designed to be a fast and good approximation avoiding division or multiplication.
    // In particular this is monotonic and maps both ends of the scale correctly.
    // Needs to be a good enough approximation to avoid upsetting control algorithms/loops (TODO-593).
    // (Multiplication on the ATMega328P may be too fast for avoiding it to be important though).
    // Guaranteed to be 255 when valvePC is 100 (max), and 0 when TRVPercentOpen is 0,
    // and a decent approximation of (valvePC * 255) / 100 in between.
    // Unit testable.
    static inline uint8_t convertPercentTo255Scale(const uint8_t valvePC)
        {
//        // This approximation is (valvePC * 250) / 100, ie *2.5, as *(2+0.5).
//        // Mapped values at various key points on the scale:
//        //      %       mapped to       target  error   %error  comment
//        //      0       0               0       0       0       fully closed: must be correct
//        //      1       3               3       0       0
//        //     50     125               128     3       1.2     important boiler drop-out threshold
//        //     67     168               171     3       1.2     important boiler trigger threshold
//        //     99     248               252     4       1.6
//        //    100     255               255     0       0       fully open: must be correct
//        const uint8_t result = (valvePC >= 100) ? 255 :
//          ((valvePC<<1) + ((1+valvePC)>>1));

//        // This approximation is valvePC * (2 + 1/2 + 1/32) with each part rounded down.
//        // Mapped values at various key points on the scale:
//        //      %       mapped to       target  error   %error  comment
//        //      0       0               0       0       0       fully closed: must be correct
//        //      1       2               3       1       0.4%
//        //      2       5               5       0       0
//        //     50     126               128     2       0.7%    important boiler drop-out threshold
//        //     67     169               171     2       0.7%    important boiler trigger threshold
//        //     68     172               173     1       0.4%
//        //     99     250               252     2       0.7%
//        //    100     255               255     0       0       fully open: must be correct
//        const uint8_t result = (valvePC >= 100) ? 255 :
//          ((valvePC<<1) + (valvePC>>1) + (valvePC>>5));

        // This approximation is valvePC * (2 + 1/2 + 1/16) with each part rounded down.
        // Mapped values at various key points on the scale:
        //      %       mapped to       target  error   %error  comment
        //      0       0               0       0       0       fully closed: must be correct
        //      1       2               3       1       0.4%
        //      2       5               5       0       0
        //     50     128               128     0       0       important boiler drop-out threshold
        //     66     169               168     1       0.4%
        //     67     171               171     0       0       important boiler trigger threshold
        //     68     174               173     1       0.4%
        //     99     253               252     1       0.4%
        //    100     255               255     X       0       fully open: must be correct
        const uint8_t result = (valvePC >= 100) ? 255 :
          ((valvePC<<1) + (valvePC>>1) + (valvePC>>4));

        return(result);
        }

    // Helper method to convert from [0,255] scale to [0,100] %-open from FHT8V/FS20 frame.
    // Designed to be a fast and good approximation avoiding division.
    // Processes the common valve fully-closed and fully-open cases efficiently.
    // In particular this is monotonic and maps both ends of the scale correctly.
    // Needs to be a good enough approximation to avoid upsetting control algorithms/loops (TODO-593).
    // (Multiplication on the ATMega328P may be too fast for avoiding it to be important though).
    // Unit testable.
    static inline uint8_t convert255ScaleToPercent(const uint8_t scale255)
        {
        // Mapped values at various key points on the scale:
        // [0,255]   mapped %       target  error   %error  comment
        //      0           0       0       0       0       fully closed: must be correct
        //      1           1       0       1       1
        //      2           1       1       0       0
        //    126          49      49       0       0
        //    128          50      50       0       0       important boiler drop-out threshold
        //    169          66      66       0       0
        //    170          67      67       0       0
        //    171          67      67       0       0       important boiler trigger threshold
        //    172          67      67       0       0
        //    254          99     100       1       1
        //    255         100     100       0       0       fully open: must be correct
        const uint8_t percentOpen =
            (0 == scale255) ? 0 :
            ((255 == scale255) ? 100 :
            ((uint8_t) ((scale255 * (uint16_t)100U + 199U) >> 8)));
        return(percentOpen);
        }
  };



#ifdef ARDUINO_ARCH_AVR

// FIXME: FHT8V code may being loaded into Flash even when not used (TODO-1017)

#define FHT8VRadValveBase_DEFINED
class FHT8VRadValveBase : public OTRadValve::AbstractRadValve, public FHT8VRadValveUtil
  {
  public:
    // Type of function to extend the TX buffer, returns pointer to 0xff just beyond last content byte appended.
    // In case of failure this returns NULL.
    typedef uint8_t *appendToTXBufferFF_t(uint8_t *buf, uint8_t bufSize);

  protected:
    // Radio link usually expected to be RFM23B; non-NULL when available.
    OTRadioLink::OTRadioLink *radio;

    // Radio channel to use for TX; defaults to 0.
    // Should be set before any sync with the FHT8V.
    int8_t channelTX;

    // TX buffer (non-null) and size (non-zero).
    uint8_t * const buf;
    const uint8_t bufSize;

    // Function to append (stats) trailer(s) to TX buffer (and add trailing 0xff if anything added); NULL if not needed.
    // Pointer set at construction.
    appendToTXBufferFF_t *const trailerFn;

    // Construct an instance, providing TX buffer details.
    FHT8VRadValveBase(uint8_t *_buf, uint8_t _bufSize, appendToTXBufferFF_t *trailerFnPtr)
      : radio(NULL), channelTX(0),
        buf(_buf), bufSize(_bufSize),
        trailerFn(trailerFnPtr),
        halfSecondCount(0)
      {
      // Cleared housecodes will prevent any immediate attempt to sync with FTH8V.
      // This also sets state to force resync afterwards.
      clearHC();
      }

    // Sync status and down counter for FHT8V, initially zero; value not important once in sync.
    // If syncedWithFHT8V = 0 then resyncing, AND
    //     if syncStateFHT8V is zero then cycle is starting
    //     if syncStateFHT8V in range [241,3] (inclusive) then sending sync command 12 messages.
    uint8_t syncStateFHT8V;

    // Count-down in half-second units until next transmission to FHT8V valve.
    uint8_t halfSecondsToNextFHT8VTX;

    // Half second count within current minor cycle for FHT8VPollSyncAndTX_XXX().
    uint8_t halfSecondCount;

    // True once/while this node is synced with and controlling the target FHT8V valve; initially false.
    bool syncedWithFHT8V;

    // True if FHT8V valve is believed to be open under instruction from this system; false if not in sync.
    bool FHT8V_isValveOpen;

    // Send current (assumed valve-setting) command and adjust FHT8V_isValveOpen as appropriate.
    // Only appropriate when the command is going to be heard by the FHT8V valve itself, not just the hub.
    void valveSettingTX(bool allowDoubleTX);

    // Run the algorithm to get in sync with the receiver.
    // Uses halfSecondCount.
    // Iff this returns true then a(nother) call FHT8VPollSyncAndTX_Next() at or before each 0.5s from the cycle start should be made.
    bool doSync(const bool allowDoubleTX);

    #if defined(V0P2BASE_TWO_S_TICK_RTC_SUPPORT)
    static const uint8_t MAX_HSC = 3; // Max allowed value of halfSecondCount.
    #else
    static const uint8_t MAX_HSC = 1; // Max allowed value of halfSecondCount.
    #endif

    // Sleep in reasonably low-power mode until specified target subcycle time, optionally listening (RX) for calls-for-heat.
    // Returns true if OK, false if specified time already passed or significantly missed (eg by more than one tick).
    // May use a combination of techniques to hit the required time.
    // Requesting a sleep until at or near the end of the cycle risks overrun and may be unwise.
    // Using this to sleep less then 2 ticks may prove unreliable as the RTC rolls on underneath...
    // This is NOT intended to be used to sleep over the end of a minor cycle.
    // FIXME: be passed a function (such as pollIIO) to call while waiting.
    void sleepUntilSubCycleTimeOptionalRX(uint8_t sleepUntil);

    // Sends to FHT8V in FIFO mode command bitstream from buffer starting at bptr up until terminating 0xff.
    // The trailing 0xff is not sent.
    //
    // If doubleTX is true, this sends the bitstream twice, with a short (~8ms) pause between transmissions, to help ensure reliable delivery.
    //
    // Returns immediately without transmitting if the command buffer starts with 0xff (ie is empty).
    //
    // Returns immediately without trying got transmit if the radio is NULL.
    //
    // Note: single transmission time is up to about 80ms (without extra trailers), double up to about 170ms.
    void FHT8VTXFHTQueueAndSendCmd(uint8_t *bptr, const bool doubleTX);

    // Call just after TX of valve-setting command which is assumed to reflect current TRVPercentOpen state.
    // This helps avoiding calling for heat from a central boiler until the valve is really open,
    // eg to avoid excess load on (or energy wasting by) the circulation pump.
    // FIXME: compare against own threshold and have NominalRadValve look at least open of all vs minPercentOpen.
    void setFHT8V_isValveOpen();

    // House codes part 1 and 2 (must each be <= 99 to be valid).
    // Starts as '0xff' as unset EEPROM values would be to indicate 'unset'.
    // Marked volatile to allow thread-/ISR- safe lock-free access for read.
    volatile uint8_t hc1, hc2;

  public:
     // Clear both housecode parts (and thus disable use of FHT8V valve).
    void clearHC() { hc1 = ~0, hc2 = ~0; resyncWithValve(); }
    // Set (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control.
    // Both parts must be <= 99 for the house code to be valid and the valve used.
    // Forces resync with remote valve if house code changed.
    void setHC1(uint8_t hc) { if(hc != hc1) { hc1 = hc; resyncWithValve(); } }
    void setHC2(uint8_t hc) { if(hc != hc2) { hc2 = hc; resyncWithValve(); } }
    // Get (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control (will be 0xff until set).
    // Both parts must be <= 99 for the house code to be valid and the valve used.
    // Thread-/ISR- safe, eg for use in radio RX filter IRQ routine.
    uint8_t getHC1() const { return(hc1); }
    uint8_t getHC2() const { return(hc2); }
    // Check if housecode is valid for controlling an FHT8V.
    bool isValidHC() const { return(isValidFHTV8HouseCode(hc1) && isValidFHTV8HouseCode(hc2)); }

    // Set radio to use (if non-NULL) or clear access to radio (if NULL).
    void setRadio(OTRadioLink::OTRadioLink *r) { radio = r; }

    // Set radio channel to use for TX to FHT8V; defaults to 0.
    // Should be set before any sync with the FHT8V.
    void setChannelTX(int8_t channel) { channelTX = channel; }

    // Minimum and maximum FHT8V TX cycle times in half seconds: [115.0,118.5].
    // Fits in an 8-bit unsigned value.
    static const uint8_t MIN_FHT8V_TX_CYCLE_HS = (115*2);
    static const uint8_t MAX_FHT8V_TX_CYCLE_HS = (118*2+1);

    // Compute interval (in half seconds) between TXes for FHT8V given house code 2 (HC2).
    // (In seconds, the formula is t = 115 + 0.5 * (HC2 & 7) seconds, in range [115.0,118.5].)
    inline uint8_t FHT8VTXGapHalfSeconds(const uint8_t hc2) { return((hc2 & 7) + 230); }

    // Compute interval (in half seconds) between TXes for FHT8V given house code 2
    // given current halfSecondCountInMinorCycle assuming all remaining tick calls to _Next
    // will be foregone in this minor cycle,
    inline uint8_t FHT8VTXGapHalfSeconds(const uint8_t hc2, const uint8_t halfSecondCountInMinorCycle)
      { return(FHT8VTXGapHalfSeconds(hc2) - (MAX_HSC - halfSecondCountInMinorCycle)); }

    // Returns true if radio or house codes not set.
    // Remains false while syncing as that is only temporary unavailability.
    virtual bool isUnavailable() const { return((NULL == radio) || !isValidHC()); }

    // Get estimated minimum percentage open for significant flow for this device; strictly positive in range [1,99].
    // Defaults to typical value from observation.
    virtual uint8_t getMinPercentOpen() const { return(TYPICAL_MIN_PERCENT_OPEN); }

    // Call to reset comms with FHT8V valve and force (re)sync.
    // Resets values to power-on state so need not be called in program preamble if variables not tinkered with.
    // Requires globals defined that this maintains:
    //   syncedWithFHT8V (bit, true once synced)
    //   FHT8V_isValveOpen (bit, true if this node has last sent command to open valve)
    //   syncStateFHT8V (byte, internal)
    //   halfSecondsToNextFHT8VTX (byte).
    void resyncWithValve()
      {
      syncedWithFHT8V = false;
      syncStateFHT8V = 0;
      halfSecondsToNextFHT8VTX = 0;
      FHT8V_isValveOpen = false;
      }

//    //#ifndef IGNORE_FHT_SYNC
//    // True once/while this node is synced with and controlling the target FHT8V valve; initially false.
//    // FIXME: fit into standard RadValve API.
//    bool isSyncedWithFHT8V() { return(syncedWithFHT8V); }
//    //#else
//    //bool isSyncedWithFHT8V() { return(true); } // Lie and claim always synced.
//    //#endif

    // Returns true iff not in error state and not (re)calibrating/(re)initialising/(re)syncing.
    // By default there is no recalibration step.
    virtual bool isInNormalRunState() const { return(syncedWithFHT8V); }

    // True if the controlled physical valve is thought to be at least partially open right now.
    // If multiple valves are controlled then is this true only if all are at least partially open.
    // Used to help avoid running boiler pump against closed valves.
    // Must not be true while (re)calibrating.
    // Returns try if in sync AND current position AND last command sent to valve indicate open.
    virtual bool isControlledValveReallyOpen() const { return(syncedWithFHT8V && FHT8V_isValveOpen && (value >= getMinPercentOpen())); }

    // Values designed to work with FHT8V_RFM23_Reg_Values register settings.
    static const uint8_t RFM23_PREAMBLE_BYTE = 0xaa; // Preamble byte for RFM23 reception.
    static const uint8_t RFM23_PREAMBLE_MIN_BYTES = 4; // Minimum number of preamble bytes for reception.
    static const uint8_t RFM23_PREAMBLE_BYTES = 5; // Recommended number of preamble bytes for reliable reception.
    static const uint8_t RFM23_SYNC_BYTE = 0xcc; // Sync-word trailing byte (with FHT8V primarily).
    static const uint8_t RFM23_SYNC_MIN_BYTES = 3; // Minimum number of sync bytes.

    // Does nothing for now; different timing/driver routines are used.
    virtual uint8_t read() { return(value); }

    // Call at start of minor cycle to manage initial sync and subsequent comms with FHT8V valve.
    // Conveys this system's TRVPercentOpen value to the FHT8V value periodically,
    // setting FHT8V_isValveOpen true when the valve will be open/opening provided it received the latest TX from this system.
    //
    //   * allowDoubleTX  if true then a double TX is allowed for better resilience, but at cost of extra time and energy
    //
    // Uses its static/internal transmission buffer, and always leaves it in valid date.
    //
    // Iff this returns true then call FHT8VPollSyncAndTX_Next() at or before each 0.5s from the cycle start
    // to allow for possible transmissions.
    //
    // See https://sourceforge.net/p/opentrv/wiki/FHT%20Protocol/ for the underlying protocol.
    //
    // ALSO MANAGES RX FROM OTHER NODES WHEN ENABLED IN HUB MODE.
    bool FHT8VPollSyncAndTX_First(bool allowDoubleTX = false);

    // If FHT8VPollSyncAndTX_First() returned true then call this each 0.5s from the start of the cycle, as nearly as possible.
    // This allows for possible transmission slots on each half second.
    //
    //   * allowDoubleTX  if true then a double TX is allowed for better resilience, but at cost of extra time and energy
    //
    // This will sleep (at reasonably low power) as necessary to the start of its TX slot,
    // else will return immediately if no TX needed in this slot.
    //
    // Iff this returns false then no further TX slots will be needed
    // (and thus this routine need not be called again) on this minor cycle
    //
    // ALSO MANAGES RX FROM OTHER NODES WHEN ENABLED IN HUB MODE.
    bool FHT8VPollSyncAndTX_Next(bool allowDoubleTX = false);

    // Create FHT8V TRV outgoing valve-setting command frame (terminated with 0xff) in the shared TX buffer.
    //   * valvePC  the percentage open to set the valve [0,100]
    //   * forceExtraPreamble  if true then force insertion of an extra preamble
    //         to make it possible for an OpenTRV hub to receive the frame,
    //         typically when calling for heat or when there is a stats trailer;
    //         note that a preamble will be forced if a trailer is being added
    //         and FHT8Vs can hear without the preamble.
    // The generated command frame can be resent indefinitely.
    // If no valve is set up then this may simply terminate an empty buffer with 0xff.
    virtual void FHT8VCreateValveSetCmdFrame(const uint8_t valvePC, const bool forceExtraPreamble = false) = 0;

#ifdef ARDUINO_ARCH_AVR
    // EEPROM / non-volatile operations.
    // These operations all affect the same EEPROM backing store,
    // so if these are used the FHT8V instance should be a singleton.
    //
    // Clear both housecode parts (and thus disable local valve), in non-volatile (EEPROM) store also.
    void nvClearHC();
    // Set (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control.
    // Will cache in FHT8V instance for speed.
    void nvSetHC1(uint8_t hc);
    void nvSetHC2(uint8_t hc);
    // Get (non-volatile) HC1 and HC2 for single/primary FHT8V wireless valve under control (will be 0xff until set).
    // Used FHT8V instance as a transparent cache of the values for speed.
    uint8_t nvGetHC1();
    uint8_t nvGetHC2();
    inline uint16_t nvGetHC() { return(nvGetHC2() | (((uint16_t) nvGetHC1()) << 8)); }
    // Load EEPROM house codes into primary FHT8V instance at start-up or once cleared in FHT8V instance.
    void nvLoadHC();

    // CLI support.
    // Clear/set house code ("H" or "H nn mm").
    // Will clear/set the non-volatile (EEPROM) values and the live ones.
    class SetHouseCode : public OTV0P2BASE::CLIEntryBase
        {
        FHT8VRadValveBase *const v;
        public:
            SetHouseCode(FHT8VRadValveBase *const valve) : v(valve) { }
            virtual bool doCommand(char *buf, uint8_t buflen);
        };
#endif // ARDUINO_ARCH_AVR

  };

// maxTrailerBytes specifies the maximum number of bytes of trailer that can be added.
// preambleBytes specifies the space to leave for preamble bytes for remote receiver sync (defaults to RFM23-suitable value).
// preambleByte specifies the (default) preamble byte value to use (defaults to RFM23-suitable value).
#define FHT8VRadValve_DEFINED
template <uint8_t maxTrailerBytes, uint8_t preambleBytes = FHT8VRadValveBase::RFM23_PREAMBLE_BYTES, uint8_t preambleByte = FHT8VRadValveBase::RFM23_PREAMBLE_BYTE>
class FHT8VRadValve final : public FHT8VRadValveBase
  {
  public:
    static const uint8_t FHT8V_MAX_EXTRA_PREAMBLE_BYTES = preambleBytes; // RFM22_PREAMBLE_BYTES
    static const uint8_t FHT8V_MAX_EXTRA_TRAILER_BYTES = maxTrailerBytes; // (1+max(MESSAGING_TRAILING_MINIMAL_STATS_PAYLOAD_BYTES,FullStatsMessageCore_MAX_BYTES_ON_WIRE));
    static const uint8_t FHT8V_200US_BIT_STREAM_FRAME_BUF_SIZE = (FHT8V_MAX_EXTRA_PREAMBLE_BYTES + (FHT8VRadValveUtil::MIN_FHT8V_200US_BIT_STREAM_BUF_SIZE) + FHT8V_MAX_EXTRA_TRAILER_BYTES); // Buffer space needed.

    // Approximate maximum transmission (TX) time for FHT8V command frame in ms; strictly positive.
    // ~80ms upwards.
    static const uint8_t FHT8V_APPROX_MAX_TX_MS = ((((FHT8V_200US_BIT_STREAM_FRAME_BUF_SIZE-1)*8) + 4) / 5);

  private:
    // Shared command buffer for TX to FHT8V.
    uint8_t FHT8VTXCommandArea[FHT8V_200US_BIT_STREAM_FRAME_BUF_SIZE];

    // Create FHT8V TRV outgoing valve-setting command frame (terminated with 0xff) in the shared TX buffer.
    //   * valvePC  the percentage open to set the valve [0,100]
    //   * forceExtraPreamble  if true then force insertion of an extra preamble
    //         to make it possible for an OpenTRV hub to receive the frame,
    //         typically when calling for heat or when there is a stats trailer;
    //         note that a preamble will be forced if a trailer is being added
    //         and FHT8Vs can hear without the preamble.
    // The generated command frame can be resent indefinitely.
    // If no valve is set up then this may simply terminate an empty buffer with 0xff.
    virtual void FHT8VCreateValveSetCmdFrame(const uint8_t valvePC, const bool forceExtraPreamble = false)
      {
      FHT8VRadValveUtil::fht8v_msg_t command;
      command.hc1 = getHC1();
      command.hc2 = getHC2();
#ifdef OTV0P2BASE_FHT8V_ADR_USED
      command.address = 0;
#endif
      command.command = 0x26;
      //  command.extension = (valvePC * 255) / 100; // needlessly expensive division.
      // Optimised for speed and to avoid pulling in a division subroutine.
      // Guaranteed to be 255 when valvePC is 100 (max), and 0 when TRVPercentOpen is 0,
      // and a decent approximation of (valvePC * 255) / 100 in between.
      command.extension = FHT8VRadValveUtil::convertPercentTo255Scale(valvePC);
      // The approximation is (valvePC * 250) / 100, ie *2.5, as *(2+0.5).

      // Work out if a trailer is allowed (by security level) and is possible to encode.
      appendToTXBufferFF_t *const tfp = *trailerFn;
      const bool doTrailer = (NULL != tfp) && (OTV0P2BASE::getStatsTXLevel() <= OTV0P2BASE::stTXmostUnsec);

      // Usually add RFM23-friendly preamble (0xaaaaaaaa sync header) only
      // IF calling for heat from the boiler (TRV actually open)
      // OR if adding a (stats) trailer that the hub should see.
      const bool doHeader = forceExtraPreamble || doTrailer;

      uint8_t * const bptrInitial = FHT8VTXCommandArea;
      const uint8_t bufSize = sizeof(FHT8VTXCommandArea);
      uint8_t *bptr = bptrInitial;

      // Start with RFM23-friendly preamble if requested.
      if(doHeader)
        {
        memset(bptr, preambleByte, preambleBytes);
        bptr += preambleBytes;
        }

      // Encode and append FHT8V FS20 command.
      // ASSUMES sufficient buffer space.
      bptr = FHT8VRadValveBase::FHT8VCreate200usBitStreamBptr(bptr, &command);

      // Append trailer if allowed/possible.
      if(doTrailer)
        {
        uint8_t * const tail = tfp(bptr, bufSize - (bptr - bptrInitial));
        // If appending stats failed, write in a terminating 0xff explicitly.
        if(NULL == tail) { *bptr = 0xff; }
        //if(NULL != tail) { bptr = tail; } // Encoding should not actually fail, but this copes gracefully if so!
        }

#if 0 && defined(V0P2BASE_DEBUG)
// Check that the buffer end was not overrun.
if(bptr - bptrInitial >= bufSize) { panic(F("FHT8V frame too big")); }
#endif
      }

  public:
    // Construct an instance.
    // Optionally pass in a function to add a trailer, eg a stats trailer, to each TX buffer.
    // Set the TX buffer empty, currently terminated with an 0xff in [0].
    FHT8VRadValve(appendToTXBufferFF_t *trailerFnPtr)
      : FHT8VRadValveBase(FHT8VTXCommandArea, FHT8V_200US_BIT_STREAM_FRAME_BUF_SIZE, trailerFnPtr)
      { FHT8VTXCommandArea[0] = 0xff; }

    // Set new target %-open value (if in range).
    // Updates TX buffer with new command and new trailer.
    // Returns true if the specified value is accepted.
    virtual bool set(const uint8_t newValue)
      {
      if(newValue > 100) { return(false); }
      value = newValue;
      // Create new TX buffer, forcing extra preamble if valve probably open.
      FHT8VCreateValveSetCmdFrame(newValue, newValue >= getMinPercentOpen());
      return(true);
      }
  };
#endif // ARDUINO_ARCH_AVR


    }

#endif

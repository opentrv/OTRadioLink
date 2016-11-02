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

Author(s) / Copyright (s): Damon Hart-Davis 2015--2016
*/

/*
 * OpenTRV Radio Link base class.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_OTRADIOLINK_H
#define ARDUINO_LIB_OTRADIOLINK_OTRADIOLINK_H

//#define DEBUG_RL // TEMPORARY

#include <stddef.h>
#include <stdint.h>

#ifdef ARDUINO
#include <Print.h>
#endif

#include <OTV0p2Base.h>


// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {
    // Helper routine to compute the length of an 0xff-terminated frame,
    // excluding the trailing 0xff.
    // Returns 0 if NULL or unterminated (within 255 bytes).
    uint8_t frameLenFFTerminated(const uint8_t *buf);

#ifdef ARDUINO
    // Helper routine to dump data frame to a Print output in human- and machine- readable format.
    // Dumps as pipe (|) then length (in decimal) then space then two characters for each byte:
    // printable characters in range 32--126 are rendered as a space then the character,
    // others are rendered as a two-digit upper-case hex value;
    // the line is terminated with CRLF.
    // eg:
    //     |5 a {  81FD
    // for the 5-byte message 0x61, 0x7b, 0x20, 0x81, 0xfd.
    //
    // Useful for debugging but also for RAD
    // to relay frames without decoding to more powerful host
    // on other end of serial cable.
    //
    // Serial has to be set up and running for this to work.
    void printRXMsg(Print *p, const uint8_t *buf, const uint8_t len);
#endif

    // Helper routine to dump data frame to Serial in human- and machine- readable format.
    // As per printRXMsg() but to Serial,
    // which has to be set up and running for this to work.
    // Implemented as if defined as:
    //     { printRXMsg(&Serial, buf, len); }
    void dumpRXMsg(const uint8_t *buf, const uint8_t len);

    // Per-channel immutable configuration.
    // Includes some opaque data purely for the radio module implementation.
    // Includes public flags indicating features of the channel,
    // eg whether it inherently provides security features,
    // and whether it is framed (eg using the hardware packet handler) or not.
    typedef class OTRadioChannelConfig
        {
        public:
            OTRadioChannelConfig(const void *_config, bool _isFull, bool _isRX = true, bool _isTX = true, bool _isAuth = false, bool _isEnc = false, bool _isUnframed = false) :
                config(_config), isFull(_isFull), isRX(_isRX), isTX(_isTX), isAuth(_isAuth), isEnc(_isEnc), isUnframed(_isUnframed) { }
            // Opaque configuration dependent on radio type.
            // Nothing other than the radio module should attempt to access/use this.
            const void *config;
            // True if this is a full radio configuration, including default register values, else partial/delta.
            const bool isFull:1;
            // True if this configuration is/supports RX.  For many radios TX/RX may be exclusive.
            const bool isRX:1;
            // True if this configuration is/supports TX.  For many radios TX/RX may be exclusive.
            const bool isTX:1;
            // True if this bearer inherently provides an authenticated/hard-to-spoof link.
            const bool isAuth:1;
            // True if this bearer inherently provides an encrypted/secure/private link.
            const bool isEnc:1;
            // True if this bearer does not provide framing including explicit (leading) frame length.
            const bool isUnframed:1;
        } OTRadioChannelConfig_t;

    // Type of a fast ISR-safe filter routine to quickly reject uninteresting RX frames.
    // Return false if the frame is uninteresting and should be dropped.
    // The aim of this is to drop such uninteresting frames quickly and reduce queueing pressure.
    // This should reduce CPU load and dropped frames in a busy channel.
    // The received frame is in the leading portion of the supplied buffer
    // (there may be trailing undefined data).
    // The buffer content may not be altered.
    // The message length is passed by reference and may be *reduced* by the filter if appropriate.
    // This routine must complete quickly and must not do things unsafe in an ISR context,
    // such as access to non-volatile state or operations such as EEPROM access on the ATmega.
    // In extreme cases this can be used as a hook to trigger fast processing
    // entirely in the ISR, skipping the usual processing flow.
    // TODO: should take RX channel parameter (different protocols may be in use on each channel).
    typedef bool quickFrameFilter_t(const volatile uint8_t *buf, volatile uint8_t &buflen);

    // Heuristic filter, especially useful for OOK carrier, to trim (all but first) trailing zeros.
    // Useful (eg) to fit more frames into RX queues if frame type is not explicit
    // and (eg with OOK operation) tail of frame buffer is filled with zeros.
    // Leaves first trailing zero for those frame types that may legitimately have one trailing zero.
    // Always returns true, ie never rejects a frame outright.
    quickFrameFilter_t frameFilterTrailingZeros;

    // Base class for radio link hardware driver.
    // Radios can support multiple channels and can be (for example) TX-only for leaf nodes.
    // Implementation cannot be assume to either re-entrant or ISR-safe except where stated.
    class OTRadioLink
        {
        public:
            // Import typedef into this class for itself and derived classes.
            typedef ::OTRadioLink::quickFrameFilter_t quickFrameFilter_t;

        private:
            // Channel being listened on or -1.
            // Mode should not need to be changed (or even read) in an ISR,
            // so does not need to be volatile or protected by a mutex, etc.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile int8_t listenChannel;

        protected:
            // Number of channels; strictly positive.
            // Logically read-only after configuration.
            int8_t nChannels;
            // Per-channel configuration, read-only.
            // This is the pointer to the start of an array of channel configurations.
            // Cannot be NULL after successful configure() with nChannels > 0.
            const OTRadioChannelConfig * channelConfig;

            // Current recent/short count of dropped messages due to RX overrun.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile uint8_t droppedRXedMessageCountRecent;

            // Current recent/short count of dropped messages due to RX filter.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile uint8_t filteredRXedMessageCountRecent;

            // Optional fast filter for RX ISR/poll; NULL if not present.
            // The routine should return false to drop an inbound frame early in processing,
            // to save queue space and CPU, and cope better with a busy channel.
            // This pointer must by updated only with interrupts locked out.
            // Marked volatile for ISR-/thread- access.
            quickFrameFilter_t *volatile filterRXISR;

            // Configure the hardware.
            // Called from configure() once nChannels and channelConfig is set.
            // Returns false if hardware not present or configuration is invalid.
            // Need not be overridden if hardware configuration is postponed until begin().
            // Defaults to do nothing.
            virtual bool _doconfig() { return(true); }

            // Switch listening off, or on to specified channel.
            // listenChannel will have been set by time this is called.
            virtual void _dolisten() = 0;

        public:
            OTRadioLink()
              : listenChannel(-1), nChannels(0), channelConfig(NULL),
                droppedRXedMessageCountRecent(0), filteredRXedMessageCountRecent(0),
                filterRXISR(NULL)
                { }

            // Set (or clear) the optional fast filter for RX ISR/poll; NULL to clear.
            // The routine should return false to drop an inbound frame early in processing,
            // to save queue space and CPU, and cope better with a busy channel.
            // At most one filter can be set; setting a new one clears any previous.
            void setFilterRXISR(quickFrameFilter_t *const filterRX);

            // Do very minimal pre-initialisation, eg at power up, to get radio to safe low-power mode.
            // Argument is read-only pre-configuration data;
            // may be mandatory for some radio types, else can be NULL.
            // This pre-configuration data depends entirely on the radio implementation,
            // but could for example be a minimal set of register number/values pairs in ROM.
            // This routine must not lock up if radio is not actually available/fitted.
            // Defaults to do nothing.
            virtual void preinit(const void *) { }

            // Emergency shutdown of radio to save power on system panic.
            // Defaults to call preinit(NULL).
            virtual void panicShutdown() { preinit(NULL); }

            // Configure the hardware.
            // Must be called before begin().
            // Returns false if hardware problems evident or config is invalid.
            // At least one channel configuration (0) must be provided
            // and it must be a 'full' base configuration;
            // others can be reduced/partial reconfigurations that can be applied to switch channels.
            // The base/0 configuration may be neither RX nor TX, eg off/disabled.
            // The base/0 configuration will be applied at begin().
            // The supplied configuration lifetime must be at least that of this OTRadioLink instance
            // as the pointer will be retained internally.
            bool configure(int8_t channels, const OTRadioChannelConfig_t * const configs)
                {
                if((channels <= 0) || (NULL == configs)) { return(false); }
                nChannels = channels;
                channelConfig = configs;
                return(_doconfig());
                }

            // Get pointer to (read-only) config for specified channel (defaulting to 0).
            // Returns NULL if no channels set or invalid channel requested.
            // This includes the flags for such features as framing and inherent security.
            // The lifetime of the returned pointer/object is at least until
            // the next call to configure() or destruction of this instance.
            const OTRadioChannelConfig_t *getChannelConfig(const uint8_t channel = 0) const
                {
                if(channel >= nChannels) { return(NULL); }
                return(channelConfig + channel);
                }

            // Begin access to (initialise) this radio link if applicable and not already begun.
            // Returns true if it successfully began, false otherwise.
            // Allows logic to end() if required at the end of a block, etc.
            // Should if possible leave the radio initialised but in a low-power state,
            // with significant power only being drawn if the radio is put in RX/listen mode,
            // or a TX is being done or about to be done,
            // or the radio is being powered up to allow RX or TX to be done.
            // Defaults to do nothing (and return false).
            virtual bool begin() { return(false); }

            // Returns true if this radio link is currently available.
            // True by default unless implementation overrides.
            // Only valid between begin() and end() calls on an instance.
            virtual bool isAvailable() const { return(true); }

            // Fetches the current inbound RX minimum queue capacity and maximum RX (and TX) raw message size.
            virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) const = 0;

            // If activeRX is true, listen for incoming messages on the specified (default first/0) channel,
            // else (if activeRX is false) make sure that the receiver is shut down.
            // (If not listening and not transmitting then by default shut down and save energy.)
            // Does not block; may initiate a poll or equivalent.
            void listen(const bool activeRX, const int8_t channel = 0)
                {
                const int8_t oldListenChannel = listenChannel;
                const int8_t newListenChannel = (!activeRX) ? -1 :
                    ((channel <= -1) ? -1 : ((channel >= nChannels) ? (nChannels-1) : channel));
                listenChannel = newListenChannel;
                // Call always if turning off listening, else when channel changes.
                if((-1 == newListenChannel) || (oldListenChannel != newListenChannel)) { _dolisten(); }
                }

            // Returns channel being listened on, or -1 if none.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline int8_t getListenChannel() const { return(listenChannel); }

            // Fetches the current count of queued messages for RX.
            // ISR-/thread- safe.
            virtual uint8_t getRXMsgsQueued() const = 0;

            // Current recent/short count of dropped messages due to RX overrun.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsDroppedRecent() const { return(droppedRXedMessageCountRecent); }

            // Current recent/short count of filtered (dropped as uninteresting) messages due to RX overrun.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsFilteredRecent() const { return(filteredRXedMessageCountRecent); }

            // Peek at first (oldest) queued RX message, returning a pointer or NULL if no message waiting.
            // The pointer returned is NULL if there is no message,
            // else the pointer is to the start of the message/frame
            // and the length is in the byte before the start of the frame.
            // This allows a message to be decoded directly from the queue buffer
            // without copying or the use of another buffer.
            // The returned pointer and length are valid until the next
            //     peekRXMessage() or removeRXMessage()
            // This does not remove the message or alter the queue.
            // The buffer pointed to MUST NOT be altered.
            // Not intended to be called from an ISR.
            virtual const volatile uint8_t *peekRXMsg() const = 0;

            // Remove the first (oldest) queued RX message.
            // Typically used after peekRXMessage().
            // Does nothing if the queue is empty.
            // Not intended to be called from an ISR.
            virtual void removeRXMsg() = 0;

            // Basic RX error numbers in range 0--127 as returned by getRXRerr() (cast to uint8_t).
            // Implementations can provide more specific errors in range 128--255.
            // 0 (zero) means no error.
            // Higher numbers may mean worse or more specific errors.
            enum BaseRXErr
                {
                RXErr_NONE = 0,         // NO ERROR.
                RXErr_DupDropped,       // Duplicate RX frame dropped, eg from a double send. Not always reported as an error.
                RXErr_RXOverrun,        // Receiver FIFO overrun or similar; no full frame RXed.
                REErr_BadFraming,       // Bad framing, preamble, postamble, check/CRC or general structure.
                RXErr_DroppedFrame      // Frame discarded due to lack of space.
                };

            // Returns the current receive error state; 0 indicates no error, +ve is the error value.
            // RX errors may be queued with depth greater than one,
            // or only the last RX error may be retained.
            // Higher-numbered error states may be more severe or more specific.
            virtual uint8_t getRXErr() { return(0); }

            // Transmission importance/power from minimum to maximum.
            // As well as possibly dynamically adjusting power within allowed ranges:
            //   * TXmax may for example also do double transmissions to help frames get heard.
            //   * TXmin may for example be used to minimise the chance of being overheard during pairing.
            enum TXpower { TXmin, TXquiet, TXnormal, TXloud, TXmax };

            // Send/TX a raw frame on the specified (default first/0) channel.
            // This does not add any pre- or post- amble (etc)
            // that particular receivers may require.
            // Revert afterwards to listen()ing if enabled,
            // else usually power down the radio if not listening.
            //   * power  hint to indicate transmission importance
            //     and thus possibly power or other efforts to get it heard;
            //     this hint may be ignored.
            //   * listenAfter  if true then try to listen after transmit
            //     for enough time to allow a remote turn-around and TX;
            //     may be ignored if radio will revert to receive mode anyway.
            // Returns true if the transmission was made, else false.
            // May block to transmit (eg to avoid copying the buffer),
            // for as much as hundreds of milliseconds depending on the data, carrier, etc.
            virtual bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false) = 0;

            // Add raw frame to send queue, to be sent when radio is ready.	// FIXME check over this
            // This does not add any pre- or post- amble (etc)
            // that particular receivers may require.
            //   * power  hint to indicate transmission importance
            //     and thus possibly power or other efforts to get it heard;
            //     this hint may be ignored.
            // Defaults to redirect to sendRaw(), in which case see sendRaw() comments.
            // Should not block unless in a call to sendRaw().
            virtual bool queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal) { return sendRaw(buf, buflen, channel, power); };

            // Poll for incoming messages (eg where interrupts are not available) and other processing.
            // Can be used safely in addition to handling inbound/outbound interrupts.
            // Where interrupts are not available should be called at least as often
            // as messages are expected to arrive to avoid radio receiver overrun.
            // May also be used for output processing,
            // eg to run a transmit state machine.
            // May be called very frequently and should not take more than a few 100ms per call.
            // Default is to do nothing.
            virtual void poll() { }

            // Handle simple interrupt for this radio link.
            // Must be fast and ISR (Interrupt Service Routine) safe.
            // Returns true if interrupt was successfully handled and cleared
            // else another interrupt handler in the chain may be called
            // to attempt to clear the interrupt.
            // Loosely has the effect of calling poll(),
            // but may respond to and deal with things other than inbound messages.
            // Initiating interrupt assumed blocked until this returns.
            // By default does nothing (and returns false).
            virtual bool handleInterruptSimple() { return(false); }

            // End access to this radio link if applicable and not already ended.
            // Returns true if it needed to be ended.
            // Defaults to do nothing (and return false).
            virtual bool end() { return(false); }

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~OTRadioLink() { end(); }
#else
#define OTRADIOLINK_NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };


    // Forward some CRC definitions that were in OTRadioLink for compatibility (DHD20160117).
    inline uint8_t crc7_5B_update(uint8_t crc, uint8_t datum) { return(OTV0P2BASE::crc7_5B_update(crc, datum)); }
    static const uint8_t crc7_5B_update_nz_ALT = OTV0P2BASE::crc7_5B_update_nz_ALT;
    inline uint8_t crc7_5B_update_nz_final(uint8_t crc, uint8_t datum) { return(OTV0P2BASE::crc7_5B_update_nz_final(crc, datum)); }

    }


#endif

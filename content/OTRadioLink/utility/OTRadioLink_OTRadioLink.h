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

Author(s) / Copyright (s): Damon Hart-Davis 2015
*/

/*
 * OpenTRV Radio Link base class.
 */

#ifndef ARDUINO_LIB_OTRADIOLINK_OTRADIOLINK_H
#define ARDUINO_LIB_OTRADIOLINK_OTRADIOLINK_H

//#define DEBUG_RL // TEMPORARY

#include <stddef.h>
#include <stdint.h>

// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {
    // Helper routine to compute the length of an 0xff-terminated frame,
    // excluding the trailing 0xff.
    // Returns 0 if NULL or unterminated (within 255 bytes).
    uint8_t frameLenFFTerminated(const uint8_t *buf);

    typedef class OTRadioChannelConfig
        {
        public:
            OTRadioChannelConfig(const void *_config, bool _isFull, bool _isRX, bool _isTX, bool _isAuth = false, bool _isEnc = false) :
                config(_config), isFull(_isFull), isRX(_isRX), isTX(_isTX), isAuth(_isAuth), isEnc(_isEnc) { }
            // Opaque configuration dependent on radio type.
            const void *config;
            // True if this is a full radio configuration, else partial/delta.
            const bool isFull:1;
            // True if this configuration is/supports RX.  For many radios TX/RX may be exclusive.
            const bool isRX:1;
            // True if this configuration is/supports TX.  For many radios TX/RX may be exclusive.
            const bool isTX:1;
            // True if this bearer provides an authenticated/hard-to-spoof link.
            const bool isAuth:1;
            // True if this bearer provides an encrypted/secure/private link.
            const bool isEnc:1;
        } OTRadioChannelConfig_t;

    // Base class for radio link hardware driver.
    // Neither re-entrant nor ISR-safe except where stated.
    class OTRadioLink
        {
        private:
            // Channel being listened on or -1.
            // Mode should not need to be changed (or even read) in an ISR,
            // so does not need to be volatile or protected by a mutex, etc.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile int8_t listenChannel;

        protected:
            // Number of channels; strictly positive.
            int8_t nChannels;
            // Per-channel configuration, read-only.
            const OTRadioChannelConfig * channelConfig;

            // Current count of received messages queued.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile uint8_t queuedRXedMessageCount;

            // Current recent/short count of dropped messages due to RX overrun.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile uint8_t droppedRXedMessageCountRecent;

            // Configure the hardware.
            // Called from configure() once nChannels and channelConfig is set.
            // Returns false if hardware not present or config is invalid.
            // Need not be overridden if hardware configuration is postponed until begin().
            // Defaults to do nothing.
            virtual bool _doconfig() { return(true); }

            // Switch listening off, or on and to specified channel.
            // listenChannel will have been set by time this is called.
            virtual void _dolisten() = 0;

        public:
            OTRadioLink()
              : listenChannel(-1), nChannels(0), channelConfig(NULL),
                queuedRXedMessageCount(0), droppedRXedMessageCountRecent(0)
                { }

            // Do very minimal pre-initialisation, eg at power up, to get radio to safe low-power mode.
            // Argument is read-only pre-configuration data;
            // may be mandatory for some radio types, else can be NULL.
            // This pre-configuration data depends entirely on the radio implementation,
            // but could for example be a minimal set of register number/values pairs in ROM.
            // This routine must not lock up if radio is not actually available/fitted.
            // Defaults to do nothing.
            virtual void preinit(const void *preconfig) { }

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
            // Some radios will have everything hardwired
            // and can be called with (1, NULL) and will set everything internally.
            bool configure(int8_t channels, const OTRadioChannelConfig_t * const configs)
                {
                if((channels <= 0) || (NULL == configs)) { return(false); }
                nChannels = channels;
                channelConfig = configs;
                return(_doconfig());
                }

            // Begin access to (initialise) this radio link if applicable and not already begun.
            // Returns true if it successfully began, false otherwise.
            // Allows logic to end() if required at the end of a block, etc.
            // Defaults to do nothing (and return false).
            virtual bool begin() { return(false); }

            // Returns true if this radio link is currently available.
            // True by default unless implementation overrides.
            // Only valid between begin() and end() calls on an instance.
            virtual bool isAvailable() const { return(true); }

            // Fetches the current inbound RX minimum queue capacity and maximum RX (and TX) raw message size.
            virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) = 0;

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
            inline int8_t getListenChannel() { return(listenChannel); }

            // Fetches the current count of queued messages for RX.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsQueued() { return(queuedRXedMessageCount); }

            // Current recent/short count of dropped messages due to RX overrun.
            // Increments when an inbound frame is not dequeued quickly enough and one has to be dropped.
            // This value wraps after 255/0xff.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsDroppedRecent() { return(droppedRXedMessageCountRecent); }

            // Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            virtual uint8_t getRXMsg(uint8_t *buf, uint8_t buflen) = 0;

            // Returns the current receive error state; 0 indicates no error, +ve is the error value.
            // RX errors may be queued with depth greater than one,
            // or only the last RX error may be retained.
            // Higher-numbered error states may be more severe.
            virtual uint8_t getRXRerr() { return(0); }

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
            ///    and thus possibly power or other efforts to get it heard;
            //     this hint may be ignored.
            //   * listenAfter  if true then try to listen after transmit
            //     for enough time to allow a remote turn-around and TX;
            //     may be ignored if radio will revert to receive mode anyway.
            // Returns true if the transmission was made, else false.
            // May block to transmit (eg to avoid copying the buffer).
            virtual bool sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel = 0, TXpower power = TXnormal, bool listenAfter = false) = 0;

            // Poll for incoming messages (eg where interrupts are not available).
            // Will only have any effect when listen(true, ...) is in effect.
            // Can be used safely in addition to handling inbound interrupts.
            // Where interrupts are not available should be called at least as often
            // as messages are expected to arrive to avoid radio receiver overrun.
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
    }


#endif

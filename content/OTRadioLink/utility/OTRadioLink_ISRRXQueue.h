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

#include <stddef.h>
#include <stdint.h>

// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {
    // Base class for an ISR-based efficient RX packet queue.
    // All queueing operations are fixed (low) cost,
    // designed to be called from an ISR (or with interrupts disabled)
    // and frames can be copied directly into the queue for efficiency.
    // Dequeueing operations are assumed not to be called from ISRs,
    // and bear the most expensive/slow operations.
    class ISRRXQueue
        {
        protected:
            // Current count of received messages queued.
            // Marked volatile for ISR-/thread- safe access without a lock.
            volatile uint8_t queuedRXedMessageCount;

        public:
            // Fetches the current inbound RX minimum queue capacity and maximum RX (and TX) raw message size.
            virtual void getCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen, uint8_t &maxTXMsgLen) = 0;

            // Fetches the current count of queued messages for RX.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsQueued() { return(queuedRXedMessageCount); }

            // Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            virtual uint8_t getRXMsg(uint8_t *buf, uint8_t buflen) = 0;

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~ISRRXQueue() {  }
#else
#define OTRADIOLINK_ISRRXQUEUE NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };
    }


#endif

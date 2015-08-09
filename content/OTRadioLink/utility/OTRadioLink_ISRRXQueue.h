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

#ifndef ARDUINO_LIB_OTRADIOLINK_ISRRXQUEUE_H
#define ARDUINO_LIB_OTRADIOLINK_ISRRXQUEUE_H

#include <stddef.h>
#include <stdint.h>

#include <util/atomic.h>
#include <Arduino.h>

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

            // Initialise state and only allow deriving classes to instantiate.
            ISRRXQueue() : queuedRXedMessageCount(0) { }

        public:
            // Fetches the current inbound RX minimum queue capacity and maximum RX raw message size.
            virtual void getRXCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen) = 0;

            // Fetches the current count of queued messages for RX.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t getRXMsgsQueued() { return(queuedRXedMessageCount); }

            // True if the queue is empty.
            // Non-virtual, for speed.
            // ISR-/thread- safe.
            inline uint8_t isEmpty() { return(0 == queuedRXedMessageCount); }

            // True if the queue is full.
            // True iff _getRXBufForInbound() would return NULL.
            // ISR-/thread- safe.
            virtual uint8_t isFull() = 0;

            // Fetches the first (oldest) queued RX message, returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            virtual uint8_t getRXMsg(uint8_t *buf, uint8_t buflen) = 0;

            // Peek at first (oldest) queued RX message, returning a pointer or NULL if no message waiting.
            // The pointer returned is NULL if there is no message,
            // else the pointer is to the start of the message and len is filled in with the length.
            // This allows a message to be decoded directly from the queue buffer
            // without copying or use of another buffer.
            // The returned pointer and length are valid until the next
            //     peekRXMessage() or removeRXMessage() or getRXMsg()
            // The buffer pointed to MUST NOT be altered.
            // Not intended to be called from an ISR.
            virtual const volatile uint8_t *peekRXMessage(uint8_t &len) const = 0;

            // Remove the first (oldest) queued RX message.
            // Typically used after peekRXMessage().
            // Does nothing if the queue is empty.
            // Not intended to be called from an ISR.
            virtual void removeRXMessage() = 0;

            // Get pointer for inbound/RX frame able to accommodate max frame size; NULL if no space.
            // Call this to get a pointer to load an inbound frame (<=maxRXBytes bytes) into;
            // after uploading the frame call _loadedBuf() to queue the new frame
            // or abandon an upload on this occasion.
            // Must only be called from within an ISR and/or with interfering threads excluded;
            // typically there can be no other activity on the queue until _loadedBuf()
            // or use of the pointer is abandoned.
            // _loadedBuf() should not be called if this returns NULL.
            virtual volatile uint8_t *_getRXBufForInbound() = 0;

            // Call after loading an RXed frame into the buffer indicated by _getRXBufForInbound().
            // The argument is the size of the frame loaded into the buffer to be queued.
            // It is possible to formally abandon an upload attempt by calling this with 0.
            virtual void _loadedBuf(uint8_t frameLen) = 0;

#if 0 // Defining the virtual destructor uses ~800+ bytes of Flash by forcing use of malloc()/free().
            // Ensure safe instance destruction when derived from.
            // by default attempts to shut down the sensor and otherwise free resources when done.
            // This uses ~800+ bytes of Flash by forcing use of malloc()/free().
            virtual ~ISRRXQueue() {  }
#else
#define OTRADIOLINK_ISRRXQUEUE NO_VIRT_DEST // Beware, no virtual destructor so be careful of use via base pointers.
#endif
        };

    // Minimal, fast, 1-deep queue.
    // Can receive at most one frame.
    // A frame to be queued can be up to maxRXBytes bytes long.
    // Does minimal checking; all arguments must be sane.
    template<uint8_t maxRXBytes = 64>
    class ISRRXQueue1Deep : public ISRRXQueue
        {
        private:
            // 1-deep RX queue and buffer used to accept data during RX.
            // Marked as volatile for ISR-/thread- safe (sometimes lock-free) access.
            volatile uint8_t lengthRX; // Frame length (non-zero) when a frame is waiting.
            volatile uint8_t bufferRX[maxRXBytes];

        public:
            // Fetches the current inbound RX minimum queue capacity and maximum RX raw message size.
            virtual void getRXCapacity(uint8_t &queueRXMsgsMin, uint8_t &maxRXMsgLen)
                { queueRXMsgsMin = 1; maxRXMsgLen = maxRXBytes; }

            // Fetches the first (oldest) queued RX message returning its length, or 0 if no message waiting.
            // If the waiting message is too long it is truncated to fit,
            // so allocating a buffer at least one longer than any valid message
            // should indicate an oversize inbound message.
            // The buf pointer must not be NULL.
            virtual uint8_t getRXMsg(uint8_t *const buf, const uint8_t buflen)
                {
                // Lock out interrupts to safely access the queue/buffers.
                ATOMIC_BLOCK (ATOMIC_RESTORESTATE)
                    {
                    if(0 == queuedRXedMessageCount) { return(0); } // Queue is empty.
                    // If message waiting, copy into caller's buffer up to its capacity.
                    const uint8_t len = min(buflen, lengthRX);
                    memcpy(buf, (const uint8_t *)bufferRX, len);
                    // Update to mark the queue as now empty.
                    queuedRXedMessageCount = 0;
                    return(len);
                    }
                return(0);
                }

            // True if the queue is full.
            // True iff _getRXBufForInbound() would return NULL.
            // ISR-/thread- safe.
            virtual uint8_t isFull() { return(0 != queuedRXedMessageCount); }

            // Get pointer for inbound/RX frame able to accommodate max frame size; NULL if no space.
            // Call this to get a pointer to load an inbound frame (<=maxRXBytes bytes) into;
            // after uploading the frame call _loadedBuf() to queue the new frame
            // or abandon an upload on this occasion.
            // Must only be called from within an ISR and/or with interfering threads excluded;
            // typically there can be no other activity on the queue until _loadedBuf()
            // or use of the pointer is abandoned.
            // _loadedBuf() should not be called if this returns NULL.
            virtual volatile uint8_t *_getRXBufForInbound()
                {
                // If something already queued, so no space for a new message, return NULL.
                if(0 != queuedRXedMessageCount) { return(NULL); }
                return(bufferRX);
                }

            // Call after loading an RXed frame into the buffer indicated by _getRXBufForInbound().
            // The argument is the size of the frame loaded into the buffer to be queued.
            // The frame can be no larger than maxRXBytes bytes.
            // It is possible to formally abandon an upload attempt by calling this with 0.
            // Must still be in the scope of the same (ISR) call as _getRXBufForInbound().
            virtual void _loadedBuf(uint8_t frameLen)
                {
                if(0 == frameLen) { return; } // New frame not being uploaded.
                if(0 != queuedRXedMessageCount) { return; } // Prevent messing with existing queued message.
                if(frameLen > maxRXBytes) { frameLen = maxRXBytes; } // Be safe...
                lengthRX = frameLen;
                queuedRXedMessageCount = 1; // Mark message as queued.
                }

            // Peek at first (oldest) queued RX message, returning a pointer or NULL if no message waiting.
            // The pointer returned is NULL if there is no message,
            // else the pointer is to the start of the message and len is filled in with the length.
            // This allows a message to be decoded directly from the queue buffer
            // without copying or use of another buffer.
            // The returned pointer and length are valid until the next
            //     peekRXMessage() or removeRXMessage() or getRXMsg()
            // The buffer pointed to MUST NOT be altered.
            // Not intended to be called from an ISR.
<<<<<<< HEAD
            virtual const volatile uint8_t *peekRXMessage(uint8_t &len) const
=======
            virtual const volatile uint8_t *peekRXMessage(uint8_t &len)
>>>>>>> refs/remotes/origin/master
                {
                // Return NULL if no message waiting.
                if(0 == queuedRXedMessageCount) { return(NULL); }
                len = lengthRX;
                return(bufferRX);
                }

            // Remove the first (oldest) queued RX message.
            // Typically used after peekRXMessage().
            // Does nothing if the queue is empty.
            // Not intended to be called from an ISR.
            virtual void removeRXMessage()
                {
                // Clear any extant message in the queue.
                queuedRXedMessageCount = 0;
                }
        };
    }


#endif

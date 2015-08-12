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

#include "OTRadioLink_ISRRXQueue.h"


// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {


// True if the queue is full.
// True iff _getRXBufForInbound() would return NULL.
// Must be protected against re-entrance, eg by interrupts being blocked before calling.
uint8_t ISRRXQueueVarLenMsgBase::_isFull() const
    {
    // If 'next' index is on 'oldest' then queued-item count determines status.
    if(next == oldest) { return(!isEmpty()); }
    // If 'next' index is after 'oldest'
    // then this is full if there isn't space for the largest possible frame.
    // (If space is (or becomes) available before the 'oldest' index
    // then the 'next' index should have been wrapped around already;
    // this ISR routine should be as fast as possible.)
    if(next > oldest)
        {
        // Is there space for length byte + largest frame before the end of the buffer?
        const uint8_t spaceBeforeEndExclLen = bsm1 - next; // Note that buf size is bsm1+1.
        return(spaceBeforeEndExclLen < mf); // True if not enough space (excluding len).
        }
    // Else 'next' is before 'oldest'
    // so check for enough space *including* the leading length.
    const uint8_t spaceBeforeOldest = oldest - next;
    return(spaceBeforeOldest <= mf); // True if not enough space (including len).
    }

// True if the queue is full.
// True iff _getRXBufForInbound() would return NULL.
// ISR-/thread- safe.
uint8_t ISRRXQueueVarLenMsgBase::isFull() const
    { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return(_isFull()); } }

// Get pointer for inbound/RX frame able to accommodate max frame size; NULL if no space.
// Call this to get a pointer to load an inbound frame (<=maxRXBytes bytes) into;
// after uploading the frame call _loadedBuf() to queue the new frame
// or abandon an upload on this occasion.
// Must only be called from within an ISR and/or with interfering threads excluded;
// typically there can be no other activity on the queue until _loadedBuf()
// or use of the pointer is abandoned.
// _loadedBuf() should not be called if this returns NULL.
volatile uint8_t *ISRRXQueueVarLenMsgBase::_getRXBufForInbound()
    {
    // This ISR is kept as short/fast as possible.
    if(_isFull()) { return(NULL); }
    // Return access to content of frame area for 'next' item if queue not full.
    return(b + next + 1);
    }

// Call after loading an RXed frame into the buffer indicated by _getRXBufForInbound().
// The argument is the size of the frame loaded into the buffer to be queued.
// The frame can be no larger than maxRXBytes bytes.
// It is possible to formally abandon an upload attempt by calling this with 0.
// Must still be in the scope of the same (ISR) call as _getRXBufForInbound().
void ISRRXQueueVarLenMsgBase::_loadedBuf(uint8_t frameLen)
    {
    // This ISR is kept as short/fast as possible.
    if(0 == frameLen) { return; } // New frame not being uploaded.
    // PANIC if frameLen > max!
    b[next] = frameLen;
    const int newNext = next + 1 + (int)frameLen;
    if(newNext > bsm1) { next = 0; } // Wrap if at end.
    else if(newNext == bsm1) { b[newNext] = 0; next = 0; } // Leave forwarding pointer and wrap if at end.
    else { next = (uint8_t) newNext; }
    ++queuedRXedMessageCount;
    return;
    }

// Peek at first (oldest) queued RX message, returning a pointer or NULL if no message waiting.
// The pointer returned is NULL if there is no message,
// else the pointer is to the start of the message and len is filled in with the length.
// This allows a message to be decoded directly from the queue buffer
// without copying or the use of another buffer.
// The returned pointer and length are valid until the next
//     peekRXMessage() or removeRXMessage()
// This does not remove the message or alter the queue.
// The buffer pointed to MUST NOT be altered.
// Not intended to be called from an ISR.
const volatile uint8_t *ISRRXQueueVarLenMsgBase::peekRXMsg(uint8_t &len) const
    {
    if(isEmpty()) { return(NULL); }
    // Cannot now become empty nor can the 'oldest' index change even if an ISR is invoked,
    // thus interrupts need not be blocked here.
    // Return access to content of 'oldest' item if queue not empty.
    len = b[oldest];
    return(b + oldest + 1);
    }

// Remove the first (oldest) queued RX message.
// Typically used after peekRXMessage().
// Does nothing if the queue is empty.
// Not intended to be called from an ISR.
void ISRRXQueueVarLenMsgBase::removeRXMsg()
    {
    // Nothing to do if empty.
    if(isEmpty()) { return; }
    // May have to inspect and adjust all state, so block interrupts.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
        // Full state could change due to ISR activity if not locked out.
        const bool wasFull = _isFull(); // May need to adjust 'next' also.

        // Advance 'oldest' index to discard oldest length+frame, wrapping if necessary.
        const int newOldest = oldest + 1 + (int)(b[oldest]);
        if(newOldest >= bsm1) { oldest = 0; } // Wrap if at end.
        else
            {
            oldest = (uint8_t) newOldest;
            // If new item is of length 0 then wrap to the start.
            if(0 == b[oldest]) { oldest = 0; }
            }
        if(wasFull)
            {
            // If the queue was full and 'next' is after/at 'oldest'
            // (assumed with not enough space before the end for a max-size entry),
            // and there is enough space before 'oldest' for a max-size entry,
            // then pull the next pointer back to 0, ie wrap it around.
            if((next >= oldest) && (oldest >= mf+1))
                {
                if(next <= bsm1) { b[next] = 0; } // Put in forwarding pointer to start.
                next = 0;
                }
            // This may need to wrap 'next' index around start if this makes enough space,
            // at least in part to keep the ISR side as fast as possible.
            }
        --queuedRXedMessageCount;
        }
    }




    }

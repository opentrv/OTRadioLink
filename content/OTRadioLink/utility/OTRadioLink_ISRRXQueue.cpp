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

#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#endif

#include "OTRadioLink_ISRRXQueue.h"

#include "OTRadioLink_OTRadioLink.h"


// Use namespaces to help avoid collisions.
namespace OTRadioLink
    {

#ifdef ARDUINO_ARCH_AVR
// True if the queue is full.
// True iff _getRXBufForInbound() would return NULL.
// ISR-/thread- safe.
uint8_t ISRRXQueueVarLenMsgBase::isFull() const
    { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return(_isFull()); } }
#endif // ARDUINO_ARCH_AVR

#ifdef ARDUINO_ARCH_AVR
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
        // Advance 'oldest' index to discard oldest length+frame, wrapping if necessary.
        // A wrap will be needed if advancing 'oldest' would take it too close to the buffer end
        // for a valid max-size incoming frame to have been stored there.
        const uint8_t o = oldest; // Cache volatile value.
        oldest = newIndex(o, b[o]);
        --queuedRXedMessageCount;
        }
    }
#endif // ARDUINO_ARCH_AVR


#ifdef ISRRXQueueVarLenMsg_VALIDATE
// Validate state, dumping diagnostics to Print stream and returning false if problems found.
// Intended for use in debugging only.
bool ISRRXQueueVarLenMsgBase::validate(Print *p, uint8_t &n, uint8_t &o, uint8_t &c, const volatile uint8_t *&bp, int &s) const
    {
    n = next;
    o = oldest;
    c = queuedRXedMessageCount;
    bp = b;
    s = 1 + (int)bsm1;
    p->print("*** queuedRXedMessageCount="); p->print(queuedRXedMessageCount);
    p->print(" next="); p->print(next);
    p->print(" oldest="); p->print(oldest);
    p->println();
    if((next > bsm1) || (oldest > bsm1)) { return(false); } // Bad!
    ::OTRadioLink::printRXMsg(p, (const uint8_t *)b, bsm1+1);
    return(true); // No problem found.
    }
#endif


    }

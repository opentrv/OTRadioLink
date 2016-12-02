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

Author(s) / Copyright (s): Damon Hart-Davis 2013--2015
*/

/*
 Simple/small/fast Pseudo-Random Number Generator support.

 For when rand()/random() are too big/slow/etc.
 */

#ifndef OTV0P2BASE_QUICKPRNG_H
#define OTV0P2BASE_QUICKPRNG_H

#include <stdint.h>

namespace OTV0P2BASE
{

// "RNG8" 8-bit 'ultra fast' PRNG suitable for 8-bit microcontrollers: low bits probably least good.
// NOT in any way suitable for crypto, but may be good to help avoid TX collisions, etc.
// C/o: http://www.electro-tech-online.com/general-electronics-chat/124249-ultra-fast-pseudorandom-number-generator-8-bit.html
// Reseed with 3 bytes of state; folded in with existing state rather than overwriting.
extern void seedRNG8(uint8_t s1, uint8_t s2, uint8_t s3); // Originally called 'init_rnd()'.
// Get 1 byte of uniformly-distributed unsigned values.
extern uint8_t randRNG8(); // Originally called 'randomize()'.

// Reset to known state; only for tests as this destroys any residual entropy.
extern void _resetRNG8();

// Get a boolean from RNG8.
// Avoids suspect low-order bit(s).
static inline bool randRNG8NextBoolean() { return(0 != (0x8 & randRNG8())); }

}

#endif




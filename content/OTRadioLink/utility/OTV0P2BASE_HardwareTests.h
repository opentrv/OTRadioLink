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

Author(s) / Copyright (s): Damon Hart-Davis 2016
*/

/*
 Hardware tests for general POST (power-on self tests)
 and for detailed hardware diagnostics.

 Some are generic such as testing clock behaviour,
 others will be very specific to some board revisions
 (eg looking for shorts or testing expected attached hardware).

 Most should return true on success and false on failure.

 Some may require being passed a Print reference
 (which will often be an active hardware serial connection)
 to dump diagnostics to.
 */

#ifndef OTV0P2BASE_HARDWARETESTS_H
#define OTV0P2BASE_HARDWARETESTS_H

//#include "OTV0P2BASE_Sensor.h"


namespace OTV0P2BASE
{
namespace HWTEST
{




} }

#endif

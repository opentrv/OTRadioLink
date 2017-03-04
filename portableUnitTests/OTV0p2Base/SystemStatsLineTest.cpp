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

Author(s) / Copyright (s): Damon Hart-Davis 2017
*/

/*
 * Driver for OTV0p2Base SystemStatsLine tests.
 */

#include <stdint.h>
#include <gtest/gtest.h>
#include <OTV0p2Base.h>
//#include <OTRadValve.h>


// Test basic instance creation, etc.
//
// Imported 2016/10/29 from Unit_Tests.cpp testJSONStats().
TEST(SystemStatsLine,Basics)
{
    OTV0P2BASE::SystemStatsLine<> ssl0;
}


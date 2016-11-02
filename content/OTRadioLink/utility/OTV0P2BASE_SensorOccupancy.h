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
 Occupancy pseudo-sensor that combines inputs from other sensors.
 */

#ifndef OTV0P2BASE_SENSOROCCUPANCY_H
#define OTV0P2BASE_SENSOROCCUPANCY_H

#include "OTV0P2BASE_Concurrency.h"
#include "OTV0P2BASE_Sensor.h"
#include "OTV0P2BASE_Util.h"


namespace OTV0P2BASE
{


// Pseudo-sensor collating inputs from other primary sensors to estimate active room occupancy by humans.
// This measure of occupancy is not intended to include people asleep (or pets, for example).
// 'Sensor' value is % confidence that the room/area controlled by this unit has active human occupants.
// Occupancy is also available as more simple 3 (likely), 2 (possibly), 1 (not), 0 (unknown) scale.
// The model is relatively simple based on time since last likely/possibly indication.
class PseudoSensorOccupancyTracker final : public OTV0P2BASE::SimpleTSUint8Sensor
  {
  public:
    // Number of minutes that room is regarded as occupied after markAsOccupied() in range [3,100].
    // DHD20130528: no activity for ~30 minutes usually enough to declare room empty; an hour is conservative.
    // Should probably be at least as long as, or a little longer than, the BAKE timeout.
    // Should probably be significantly shorter than normal 'learn' on time to allow savings from that in empty rooms.
    // Vales of 25, 50, 100 work well for the internal arithmetic.
    static const uint8_t OCCUPATION_TIMEOUT_M = 50;

  private:
    // Threshold from 'likely' to 'probably'.  Not part of official API.
    static const uint8_t OCCUPATION_TIMEOUT_LIKELY_M = ((OCCUPATION_TIMEOUT_M*2)/3);
    // Threshold from 'probably' to 'maybe'.  Not part of official API.
    static const uint8_t OCCUPATION_TIMEOUT_MAYBE_M = OCCUPATION_TIMEOUT_LIKELY_M/2;

    // Time until room regarded as unoccupied, in minutes; initially zero (ie treated as unoccupied at power-up).
    // Marked volatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    // Compound operations must block interrupts.
    volatile Atomic_UInt8T occupationCountdownM;

    // Non-zero if occupancy system recently notified of activity.
    // Marked volatile for thread-safe lock-free non-read-modify-write access to byte-wide value.
    // Compound operations must block interrupts.
    volatile Atomic_UInt8T activityCountdownM;

    // Hours and minutes since room became vacant (doesn't roll back to zero from max hours); zero when room occupied.
    uint8_t vacancyH;
    uint8_t vacancyM;

  public:
    PseudoSensorOccupancyTracker() : occupationCountdownM(0), activityCountdownM(0), vacancyH(0), vacancyM(0) { }

    // Force a read/poll of the occupancy and return the % likely occupied [0,100].
    // Potentially expensive/slow.
    // Not thread-safe nor usable within ISRs (Interrupt Service Routines).
    // Poll at a fixed rate.
    virtual uint8_t read() override;

    // Returns true if this sensor reading value passed is potentially valid, eg in-range.
    // True if in range [0,100].
    virtual bool isValid(uint8_t value) const override { return(value <= 100); }

    // This routine should be called once per minute.
    virtual uint8_t preferredPollInterval_s() const override { return(60); }

    // Recommended JSON tag for full value; not NULL.
    virtual const char *tag() const override { return("occ|%"); }

    // True if activity/occupancy recently reported (within last couple of minutes).
    // Activity includes weak and strong reports.
    // ISR-/thread- safe.
    bool reportedRecently() const { return(0 != activityCountdownM.load()); }

    // Returns true if the room appears to be likely occupied (with active users) now.
    // Operates on a timeout; calling markAsOccupied() restarts the timer.
    // Defaults to false (and API still exists) when ENABLE_OCCUPANCY_SUPPORT not defined.
    // ISR-/thread- safe.
    bool isLikelyOccupied() const { return(0 != occupationCountdownM.load()); }

    // Returns true if the room appears to be likely occupied (with active users) recently.
    // This uses the same timer as isOccupied() (restarted by markAsOccupied())
    // but returns to false somewhat sooner for example to allow ramping up more costly occupancy detection methods
    // and to allow some simple graduated occupancy responses.
    // ISR-/thread- safe.
    bool isLikelyRecentlyOccupied() const { return(occupationCountdownM.load() > OCCUPATION_TIMEOUT_LIKELY_M); }

    // Returns true if room likely currently unoccupied (no active occupants).
    // Defaults to false (and API still exists) when ENABLE_OCCUPANCY_SUPPORT not defined.
    // This may require a substantial time after activity stops to become true.
    // This and isLikelyOccupied() cannot be true together; it is possible for neither to be true.
    // Thread-safe.
    bool isLikelyUnoccupied() const { return(!isLikelyOccupied()); }

    // Call when very strong evidence of active room occupation has occurred.
    // Do not call based on internal/synthetic events.
    // Such evidence may include operation of buttons (etc) on the unit or PIR.
    // Do not call from (for example) 'on' schedule change.
    // Makes occupation immediately visible.
    // Thread-safe and ISR-safe.
    void markAsOccupied() { value = 100; occupationCountdownM.store(OCCUPATION_TIMEOUT_M); activityCountdownM.store(2); }

    // Call when decent but not very strong evidence of active room occupation, such as a light being turned on, or voice heard.
    // Do not call based on internal/synthetic events.
    // Doesn't force the room to appear recently occupied.
    // If the hardware allows this may immediately turn on the main GUI LED until normal GUI reverts it,
    // at least periodically.
    // Preferably do not call for manual control operation to avoid interfering with UI operation.
    // Thread-safe.
    void markAsPossiblyOccupied();

    // Call when weak evidence of active room occupation, such rising RH% or CO2 or mobile phone RF levels while not dark.
    // Do not call based on internal/synthetic events.
    // Doesn't force the room to appear recently occupied.
    // If the hardware allows this may immediately turn on the main GUI LED until normal GUI reverts it,
    // at least periodically.
    // Preferably do not call for manual control operation to avoid interfering with UI operation.
    // Thread-safe.
    void markAsJustPossiblyOccupied();

    // Two-bit occupancy: 0 not known/disclosed, 1 not occupied, 2 possibly occupied, 3 probably occupied.
    // 0 is not returned by this implementation.
    // Thread-safe.
    uint8_t twoBitOccupancyValue() const { return(isLikelyRecentlyOccupied() ? 3 : (isLikelyOccupied() ? 2 : 1)); }

    // Recommended JSON tag for two-bit occupancy value; not NULL.
    const char *twoBitTag() const { return("O"); }

    // Returns true if it is worth expending extra effort to check for occupancy.
    // This will happen when confidence in occupancy is not yet zero but is approaching,
    // so checking more thoroughly now can help maintain non-zero value if someone is present and active.
    // At other times more relaxed checking (eg lower power) can be used.
    bool increaseCheckForOccupancy() const { return(!isLikelyRecentlyOccupied() && isLikelyOccupied() && !reportedRecently()); }

    // Get number of hours room vacant, zero when room occupied; does not wrap.
    // Is forced to zero as soon as occupancy is detected.
    uint16_t getVacancyH() const { return((value != 0) ? 0 : vacancyH); }

    // Recommended JSON tag for vacancy hours; not NULL.
    const char *vacHTag() const { return("vac|h"); }

    // Threshold hours above which room is considered long vacant.
    // At least 24h in order to allow once-daily room programmes (including pre-warm) to operate reliably.
    static const uint8_t longVacantHThrH = 24;
    // Threshold hours above which room is considered long long vacant.
    // Longer than longVacantHThrH but much less than 3 days to try to capture some weekend-absence savings.
    // ~8h less than 2d may capture full office energy savings for the whole day of Sunday
    // counting from from last occupancy at end of (working) day Friday for example.
    static const uint8_t longLongVacantHThrH = 39;

    // Returns true if room appears to have been vacant for over a day.
    // For a home or an office no sign of activity for this long suggests a weekend or a holiday for example.
    // At least 24h in order to allow once-daily room programmes (including pre-warm) to operate reliably.
    bool longVacant() const { return(getVacancyH() > longVacantHThrH); }

    // Returns true if room appears to have been vacant for much longer than longVacant().
    // For a home or an office no sign of activity for this long suggests a long weekend or a holiday for example.
    // Longer than longVacant() but much less than 3 days to try to capture some weekend-absence savings.
    bool longLongVacant() const { return(getVacancyH() > longLongVacantHThrH); }

    // Put directly into energy-conserving 'holiday mode' by making room appear to be 'long vacant'.
    // Be careful of retriggering presence immediately if this is set locally.
    // Set apparent vacancy to maximum to make setting obvious and to hide further vacancy from snooping.
    // Code elsewhere may wish to put the system in FROST mode also.
    void setHolidayMode() { value = 0; vacancyH = 255U; activityCountdownM.store(0); occupationCountdownM.store(0); }
  };


// Dummy placeholder occupancy 'sensor' class with always-false/inactive dummy static status methods.
// These methods should be fully optimised away by the compiler in many/most cases.
// Can be to reduce code complexity, by eliminating some need for preprocessing.
class DummySensorOccupancyTracker final
  {
  public:
    static void markAsOccupied() {} // Defined as NO-OP for convenience.
    static void markAsPossiblyOccupied() {} // Defined as NO-OP for convenience.
    static bool isLikelyRecentlyOccupied() { return(false); }
    static bool isLikelyOccupied() { return(false); }
    static bool isLikelyUnoccupied() { return(false); }
    static uint8_t twoBitOccValue() { return(0); }
    static uint16_t getVacancyH() { return(0); }
    static bool longVacant() { return(false); }
    static bool longLongVacant() { return(false); }
  };


}
#endif

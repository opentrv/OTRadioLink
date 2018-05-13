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
 Ambient light sensor with occupancy detection.

 Specific to V0p2/AVR for now.
 */


#include "OTV0P2BASE_SensorAmbientLight.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "OTV0P2BASE_ADC.h"
#include "OTV0P2BASE_BasicPinAssignments.h"
#include "OTV0P2BASE_Entropy.h"
#include "OTV0P2BASE_PowerManagement.h"
#include "OTV0P2BASE_Sleep.h"


namespace OTV0P2BASE
{


// DHD20161104: impl removed from main app.
//
//static const uint8_t shiftRawScaleTo8Bit = 2;
//#ifdef ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
//// This implementation expects a phototransitor TEPT4400 (50nA dark current, nominal 200uA@100lx@Vce=50V) from IO_POWER_UP to LDR_SENSOR_AIN and 220k to ground.
//// Measurement should be taken wrt to internal fixed 1.1V bandgap reference, since light indication is current flow across a fixed resistor.
//// Aiming for maximum reading at or above 100--300lx, ie decent domestic internal lighting.
//// Note that phototransistor is likely far more directionally-sensitive than LDR and its response nearly linear.
//// This extends the dynamic range and switches to measurement vs supply when full-scale against bandgap ref, then scales by Vss/Vbandgap and compresses to fit.
//// http://home.wlv.ac.uk/~in6840/Lightinglevels.htm
//// http://www.engineeringtoolbox.com/light-level-rooms-d_708.html
//// http://www.pocklington-trust.org.uk/Resources/Thomas%20Pocklington/Documents/PDF/Research%20Publications/GPG5.pdf
//// http://www.vishay.com/docs/84154/appnotesensors.pdf
//#if (7 == V0p2_REV) // REV7 initial board run especially uses different phototransistor (not TEPT4400).
//// Note that some REV7s from initial batch were fitted with wrong device entirely,
//// an IR device, with very low effective sensitivity (FSD ~ 20 rather than 1023).
//static const int LDR_THR_LOW = 180U;
//static const int LDR_THR_HIGH = 250U;
//#else // REV4 default values.
//static const int LDR_THR_LOW = 270U;
//static const int LDR_THR_HIGH = 400U;
//#endif
//#else // LDR (!defined(ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400))
//// This implementation expects an LDR (1M dark resistance) from IO_POWER_UP to LDR_SENSOR_AIN and 100k to ground.
//// Measurement should be taken wrt to supply voltage, since light indication is a fraction of that.
//// Values below from PICAXE V0.09 impl approx multiplied by 4+ to allow for scale change.
//#ifdef ENABLE_AMBLIGHT_EXTRA_SENSITIVE // Define if LDR not exposed to much light, eg for REV2 cut4 sideways-pointing LDR (TODO-209).
//static const int LDR_THR_LOW = 50U;
//static const int LDR_THR_HIGH = 70U;
//#else // Normal settings.
//static const int LDR_THR_LOW = 160U; // Was 30.
//static const int LDR_THR_HIGH = 200U; // Was 35.
//#endif // ENABLE_AMBLIGHT_EXTRA_SENSITIVE
//#endif // ENABLE_AMBIENT_LIGHT_SENSOR_PHOTOTRANS_TEPT4400
//AmbientLight AmbLight(LDR_THR_HIGH >> shiftRawScaleTo8Bit);


}

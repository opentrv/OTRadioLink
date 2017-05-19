/*
 * OTRadioLink_Messagin.h
 *
 *  Created on: 18 May 2017
 *      Author: denzo
 */

#ifndef UTILITY_OTRADIOLINK_MESSAGING_H_
#define UTILITY_OTRADIOLINK_MESSAGING_H_

#include <OTV0p2Base.h>
#include <OTAESGCM.h>
#include "OTRadioLink_SecureableFrameType.h"
#include "OTRadioLink_SecureableFrameType_V0p2Impl.h"

#include "OTRadioLink_OTRadioLink.h"

namespace OTRadioLink
{

// Returns true on successful frame type match, false if no suitable frame was found/decoded and another parser should be tried.
bool decodeAndHandleOTSecureableFrame(Print * /*p*/, const bool /*secure*/, const uint8_t * const msg, OTRadioLink &rt);

}

#endif /* UTILITY_OTRADIOLINK_MESSAGING_H_ */

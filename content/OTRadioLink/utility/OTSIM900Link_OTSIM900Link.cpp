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

Author(s) / Copyright (s): Deniz Erbilgin 2015
                           Damon Hart-Davis 2015
*/

#include "OTSIM900Link_OTSIM900Link.h"

namespace OTSIM900Link
{

/**
 * @brief	Constructor. Initializes softSerial and sets PWR_PIN
 * @todo	Can/should pinMode be replaced?
 * @param	pwrPin		SIM900 power on/off pin
 * @param	rxPin		Rx pin for software serial
 * @param	txPin		Tx pin for software serial
 */
OTSIM900Link::OTSIM900Link(const OTSIM900LinkConfig_t *_config, uint8_t pwrPin, uint8_t rxPin, uint8_t txPin) : PWR_PIN(pwrPin), softSerial(7, 8)
{
  pinMode(PWR_PIN, OUTPUT);
  bAvailable = false;
  bPowered = false;
  config = _config;
}

/**
 * @brief	begin software serial
 * @todo	What is this actually going to do?
 * 			- Turning module on automatically starts radio, so this does not fit desired begin behaviour
 * 			- There is a low power mode but this still requires an active connection to stay registered
 * 			- APN etc need to be set after each power up. Is this just going to store these in object?
 *          why can't I set softSerial->begin like this?
 *          Move check to a method
 */
bool OTSIM900Link::begin()
{
	softSerial.begin(baud);

#ifdef OTSIM900LINK_DEBUG
  Serial.println("Get Init State");
#endif // OTSIM900LINK_DEBUG
	if (!getInitState()) return false; 	// exit function if no/wrong module
	//flush();	// discard preamble
	//delay(1000);

	// perform steps that can be done without network connection
#ifdef OTSIM900LINK_DEBUG
  Serial.println("Power up");
#endif // OTSIM900LINK_DEBUG
	powerOn();
	//flush();

#ifdef OTSIM900LINK_DEBUG
  Serial.println("Check Pin");
#endif // OTSIM900LINK_DEBUG
	if (!checkPIN()) {
		setPIN();
	}

#ifdef OTSIM900LINK_DEBUG
  Serial.println("Wait for Registration");
#endif // OTSIM900LINK_DEBUG
	// block until network registered
	while(!isRegistered()) { delay(2000); }

#ifdef OTSIM900LINK_DEBUG
  Serial.println("Set APN");
#endif // OTSIM900LINK_DEBUG
  while(!setAPN());

#ifdef OTSIM900LINK_DEBUG
  Serial.println("Start GPRS");
#endif // OTSIM900LINK_DEBUG
	startGPRS(); // starting and shutting gprs brings module to state
	delay(500);
	shutGPRS();	 // where openUDP can automatically start gprs
	return true;
}

/**
 * @brief	close UDP connection and power down SIM module
 */
bool OTSIM900Link::end()
{
	closeUDP();
	powerOff();
	return false;
}

/**
 * @brief	Sends message
 * @param	buf		pointer to buffer to send
 * @param	buflen	length of buffer to send
 * @param	channel	ignored
 * @param	Txpower	ignored
 * @retval	returns true if send process inited
 * @note	requires calling of poll() to check if message sent successfully
 */
bool OTSIM900Link::sendRaw(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power, bool listenAfter)
{
	//if(isOpenUDP()){
		return sendUDP((const char *)buf, buflen);
	//} else return false;
}

/**
 * @brief	Puts message in queue to send on wakeup
 * @todo	implement and make virtual method in OTRadioLink
 * @param	buf		pointer to buffer to send
 * @param	buflen	length of buffer to send
 * @param	channel	ignored
 * @param	Txpower	ignored
 * @retval	returns true if send process inited
 * @note	requires calling of poll() to check if message sent successfully
 */
bool OTSIM900Link::queueToSend(const uint8_t *buf, uint8_t buflen, int8_t channel, TXpower power)
{
	openUDP();
	delay(5000);	// find better way?
	//flush();

	sendRaw(buf, buflen);
	shutGPRS();
	return true;
}

/**
 * @brief
 * @todo	everything
 */
void OTSIM900Link::poll()
{

}

/**
 * @brief	open UDP connection to input ip address
 * @todo	clean up writes
 * @param	array containing IP address to open connection to in format xxx.xxx.xxx.xxx (keep all zeros)
 * @retval	returns true if UDP opened
 * @note	is it necessary to check if UDP open?
 */
bool OTSIM900Link::openUDP()
{
	//if(!isOpenUDP()){
		print(AT_START);
		print(AT_START_UDP);
		print("=\"UDP\","); // FIXME! string length is declared 1 longer than it should
		print('\"');
		print(config->UDP_Address);
		print("\",\"");	// FIXME!
		print(config->UDP_Port);
		print('\"');
		print(AT_END);
	//}
	return true;
}

/**
 * @brief	close UDP connection
 * @todo	implement checks
 * @retval	returns true if UDP closed
 * @note	check UDP open
 */
bool OTSIM900Link::closeUDP()
{
	//if(isOpenUDP()){
		print(AT_START);
		print(AT_CLOSE_UDP);
		print(AT_END);
		return false;
	//} else return true;
}

/**
 * @brief	send UDP frame
 * @todo	add check for successful send
 * @param	pointer to array containing frame to send
 * @param	length of frame
 * @retval	returns true if send successful
 * @note	check UDP open
 */
bool OTSIM900Link::sendUDP(const char *frame, uint8_t length)
{
	print(AT_START);
	print(AT_SEND_UDP);
	print('=');
	print(length);
	print(AT_END);

	// '>' indicates module is ready for UDP frame
	if (flushUntil('>')) {
		write(frame, length);
		delay(500);
		return true;	// add check here
	}
	return false;
}

/**
 * @brief	Reads a single character from softSerial
 * @retval	returns character read, or 0 if no data to read
 */
uint8_t OTSIM900Link::read()
{
	uint8_t data;
	data = softSerial.read();	// FIXME
	return data;
}

/**
 * @brief	empties serial buffer
 */
/*void OTSIM900Link::flush()
{
	Serial.print("- Flush: ");
	while(softSerial.available() > 0) Serial.print((char) softSerial.read());
	Serial.println();
}*/

/**
 * @brief	Enter blocking read. Fills buffer or times out after 100 ms
 * @todo	what to do with terminatingChar? Using 0 as default fails?
 * 			Overloaded for now
 * @param	data	data buffer to write to
 * @param	length	length of data buffer
 * @retval	length of data received before time out
 */
uint8_t OTSIM900Link::timedBlockingRead(char *data, uint8_t length)
{;
  // clear buffer, get time and init i to 0
  memset(data, 0, length);
  uint8_t i = 0;

  /*uint32_t startTime = millis();
  // 100ms is time to fill buffer? probs got maths wrong on this.
  // May have to wait a little longer because of (eg) interactions with the network,
  // especially if a terminating char is known.
  const bool hasTerminatingChar = (0 != terminatingChar);
  const uint32_t timeoutms = 2000;//hasTerminatingChar ? 500 : 200;	FIXME return to normal
  while ((millis() - startTime) <= timeoutms) {
    if (softSerial.available() > 0) {	// FIXME
      const char c = read();
      *data++ = c;
      if(hasTerminatingChar && (c == terminatingChar)) { break; }
      i++;
#ifdef OTSIM900LINK_DEBUG
      //Serial.print((uint8_t) c, HEX);	// print raw values
      //Serial.print(" ");
#endif // OTSIM900LINK_DEBUG
    }
    // break if receive too long.
    if (i >= length) {
#ifdef OTSIM900LINK_DEBUG
      Serial.println("\n--Serial Overrun");
#endif // OTSIM900LINK_DEBUG
      // FIXME: rest of input still had to be absorbed to avoid fouling next interaction.
      break;
    }
  }*/
  i = softSerial.read((uint8_t *)data, length);



#ifdef OTSIM900LINK_DEBUG
  //Serial.print("\n--Buffer Length: ");
  //Serial.println(i);
#endif // OTSIM900LINK_DEBUG
  return i;
}

/**
 * @brief   blocks process until terminatingChar received
 * @param	terminatingChar		character to block until
 * @retval	returns true if character found, or false on 1000ms timeout
 */
bool OTSIM900Link::flushUntil(uint8_t _terminatingChar)
{
	const uint8_t terminatingChar = _terminatingChar;

#ifdef OTSIM900LINK_DEBUG
	Serial.print("- Flush Until 0x");
	Serial.print(terminatingChar, HEX);
	Serial.println(": ");
#endif // OTSIM900LINK_DEBUG

  uint32_t endTime = millis() + 2000; // time out after a second
  while(millis() < endTime) {
    const uint8_t c = read();

#ifdef OTSIM900LINK_DEBUG
    if(c != 0xFF) Serial.print((char) c);
#endif // OTSIM900LINK_DEBUG

    if (c == terminatingChar) return true;
  }
#ifdef OTSIM900LINK_DEBUG
  Serial.println("Timeout");
#endif // OTSIM900LINK_DEBUG
  return false;
}
/**
 * @brief	Writes an array to software serial
 * @todo	change these to overloaded prints?
 * @param	data	data buffer to write from
 * @param	length	length of data buffer
 */
void OTSIM900Link::write(const char *data, uint8_t length)
{
	softSerial.write(data, length);
}

/**
 * @brief	Writes a character to software serial
 * @param	data	character to write
 */
void OTSIM900Link::print(char data)
{
	softSerial.print(data);	// FIXME
}

/**
 * @brief  Writes a character to software serial
 * @param data  character to write
 */
void OTSIM900Link::print(const int value)
{
  softSerial.print(value);	// FIXME
}

void OTSIM900Link::print(const char *string)
{
  softSerial.print(string);	// FIXME
}


/**
 * @brief	Checks module ID
 * @todo	Implement check?
 * @param	name	pointer to array to compare name with
 * @param	length	length of array name
 * @retval	returns true if ID recovered successfully
 */
bool OTSIM900Link::checkModule()
{
  char data[32];
  print(AT_START);
  print(AT_GET_MODULE);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));
#ifdef OTSIM900LINK_DEBUG
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG
  return true;
}

/**
 * @brief	Checks connected network
 * @todo	implement check
 * @param	buffer	pointer to array to store network name in
 * @param	length	length of buffer
 * @param	returns true if connected to network
 */
bool OTSIM900Link::checkNetwork()
{
  char data[64];
  print(AT_START);
  print(AT_NETWORK);
  print(AT_QUERY);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut= getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message

  return true;
}

/**
 * @brief 	check if module connected and registered (GSM and GPRS)
 * @todo	implement check
 * 			are two registration checks needed?
 * @retval	true if registered
 */
bool OTSIM900Link::isRegistered()
{
//  Check the GSM registration via AT commands ( "AT+CREG?" returns "+CREG:x,1" or "+CREG:x,5"; where "x" is 0, 1 or 2).
//  Check the GPRS registration via AT commands ("AT+CGATT?" returns "+CGATT:1" and "AT+CGREG?" returns "+CGREG:x,1" or "+CGREG:x,5"; where "x" is 0, 1 or 2). 

  char data[64];
  /*write(AT_START, sizeof(AT_START)-1);
  write(AT_REGISTRATION, sizeof(AT_REGISTRATION)-1);
  write(AT_QUERY);
  write(AT_END);*/

  print(AT_START);
  print(AT_REGISTRATION);
  print(AT_QUERY);
  print(AT_END);

  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message

/*  delay(100);
  write(AT_START, sizeof(AT_START));
  write(AT_GPRS_REGISTRATION0, sizeof(AT_GPRS_REGISTRATION0));
  write(AT_QUERY);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
#ifdef OTSIM900LINK_DEBUG
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG
  delay(100);
  write(AT_START, sizeof(AT_START));
  write(AT_GPRS_REGISTRATION, sizeof(AT_GPRS_REGISTRATION));
  write(AT_QUERY);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
#ifdef OTSIM900LINK_DEBUG
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG*/

  if (dataCut[2] == '1' || dataCut[2] == '5' ) return true;	// expected response '1' or '5'
  else return false;
}

/**
 * @brief	Set Access Point Name and start task
 * @param	APN		pointer to access point name
 * @param	length	length of access point name
 * @retval	Returns true if APN set
 */
bool OTSIM900Link::setAPN()
{
  char data[96];
  print(AT_START);
  print(AT_SET_APN);
  print(AT_SET);
  print('\"');
  print(config->APN);
  print('\"');
  print(AT_END);

  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut = getResponse(dataCutLength, data, sizeof(data), 0x0A);
#ifdef OTSIM900LINK_DEBUG
  //Serial.println(data);
#endif // OTSIM900LINK_DEBUG

  if (*dataCut == 'O') return true;	// expected response 'OK'
  else return false;
}

/**
 * @brief	Start GPRS connection
 * @retval	returns true if connected
 * @note	check power, check registered, check gprs active
 */
bool OTSIM900Link::startGPRS()
{
  char data[96];
  print(AT_START);
  print(AT_START_GPRS);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);	// unreliable
  if (*dataCut == 'O') return true;	// expected response 'OK'
  else return false;
}

/**
 * @brief	Shut GPRS connection
 * @todo	check for OK response on each set?
 * 			check syntax correct
 * @retval	returns false if shut
 */
bool OTSIM900Link::shutGPRS()
{
  char data[96];
  print(AT_START);
  print(AT_SHUT_GPRS);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);
  if (*dataCut == 'S') return false;	// expected response 'SHUT OK'
  else return true;
}

/**
 * @brief	Get IP address
 * @todo	How should I return the string?
 * @param	pointer to array to store IP address in. must be at least 16 characters long
 * @retval	return length of IP address. Return 0 if no connection
 */
uint8_t OTSIM900Link::getIP()
{
  char data[64];
  print(AT_START);
  print(AT_GET_IP);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));
  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);

  if(*dataCut == '+') return 0; // all error messages will start with a '+'
  else {
	  return dataCutLength;
  }
}

/**
 * @brief	check if UDP open
 * @todo	implement function
 * @retval	true if open
 */
bool OTSIM900Link::isOpenUDP()
{
	char data[64];
	print(AT_START);
	print(AT_STATUS);
	print(AT_END);
	timedBlockingRead(data, sizeof(data));

	Serial.println(data);

	// response stuff
	const char *dataCut;
	uint8_t dataCutLength = 0;
	dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
	if (*dataCut == 'C') return true; // expected string is 'CONNECT OK'. no other possible string begins with R
	else return false;
}

/**
 * @brief   Set verbose errors
 */
void OTSIM900Link::verbose()
{
#ifdef OTSIM900LINK_DEBUG
  char data[64];
  print(AT_START);
  print(AT_VERBOSE_ERRORS);
  print(AT_SET);
  print('0'); // 0: no error codes, 1: error codes, 2: full error descriptions
  print(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG
}

/**
 * @brief   Enter PIN code
 * @param   pin     pointer to array containing pin code
 * @param   length  length of pin
 */
void OTSIM900Link::setPIN()
{
  char data[64];
  print(AT_START);
  print(AT_PIN);
  print(AT_SET);
  print(config->PIN);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));

#ifdef OTSIM900LINK_DEBUG
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG
}

/**
 * @brief   Check if PIN required
 * @todo    return logic
 */
bool OTSIM900Link::checkPIN()
{
  char data[40];
  print(AT_START);
  print(AT_PIN);
  print(AT_QUERY);
  print(AT_END);
  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut = getResponse(dataCutLength, data, sizeof(data), ' '); // first ' ' appears right before useful part of message
  if (*dataCut == 'R') return true; // expected string is 'READY'. no other possible string begins with R
  else return false;
}

/**
 * @brief	Returns a pointer to section of response containing important data
 * 			and sets its length to a variable
 * @param	newLength	length of useful data
 * @param	data		pointer to array containing response from device
 * @param	dataLength	length of array
 * @param	startChar	ignores everything up to and including this character
 * @retval	pointer to start of useful data
 */
const char *OTSIM900Link::getResponse(uint8_t &newLength, const char *data, uint8_t dataLength, char _startChar)
{
	char startChar = _startChar;
	const char *newPtr = NULL;
	uint8_t  i = 0;	// 'AT' + command + 0x0D
	uint8_t i0 = 0; // start index
	newLength = 0;

	// Ignore echo of command
	while (*data !=  startChar) {
		data++;
		i++;
		if(i >= dataLength) return NULL;
	}
	data++;
	i++;

	// Set pointer to start of and index
	newPtr = data;
	i0 = i;

	// Find end of response
	while(*data != 0x0D) {	// find end of response
		data++;
		i++;
		if(i >= dataLength) return NULL;
	}

	newLength = i - i0;

#ifdef OTSIM900LINK_DEBUG
	Serial.print("- Response: ");
	Serial.write(newPtr, newLength);
	Serial.println();
#endif // OTSIM900LINK_DEBUG

	return newPtr;	// return length of new array
}

/**
 * @brief	Test if radio is available and set available and power flags
 * 			returns to powered off state?
 * @todo	possible to just cycle power and read return val
 * 			Lots of testing
 * @retval	true if module found and returns correct start value
 * @note	Possible states at start up:
 * 			1. no module - No response
 * 			2. module not powered - No response
 * 			3. module powered - correct response
 * 			4. wrong module - unexpected response
 */
bool OTSIM900Link::getInitState()
{
	// Test if available and set flags
	bAvailable = false;
	bPowered = false;
	char data[10];	// max expected response
	memset(data, 0 , sizeof(data));

#ifdef OTSIM900LINK_DEBUG
	Serial.println("Check for module: ");
#endif // OTSIM900LINK_DEBUG
	print(AT_START);
	print(AT_END);
	if (timedBlockingRead(data, sizeof(data)) == 0) { // state 1 or 2

#ifdef OTSIM900LINK_DEBUG
	Serial.println("- Attempt to force State 3");
#endif // OTSIM900LINK_DEBUG

		powerToggle();
		memset(data, 0, sizeof(data));
		flushUntil(0x0A);
		print(AT_START);
		print(AT_END);
		if (timedBlockingRead(data, sizeof(data)) == 0) { // state 1

#ifdef OTSIM900LINK_DEBUG
	Serial.println("-- Failed. No Module");
#endif // OTSIM900LINK_DEBUG

			bPowered = false;
			return false;
		}
	}

	if( data[0] == 'A' ) { // state 3 or 4
		bAvailable = true;
		bPowered = true;
		powerOff();
#ifdef OTSIM900LINK_DEBUG
	Serial.println("- Module Present");
#endif // OTSIM900LINK_DEBUG
		return true;	// state 3
	} else {
#ifdef OTSIM900LINK_DEBUG
	Serial.println("- Unexpected Response");
#endif // OTSIM900LINK_DEBUG
		bAvailable = false;
		bPowered = false;
		return false;	// state 4
	}
}

//const char OTSIM900Link::AT_[] = "";
const char OTSIM900Link::AT_START[3] = "AT";
const char OTSIM900Link::AT_NETWORK[6] = "+COPS";
const char OTSIM900Link::AT_REGISTRATION[6] = "+CREG"; // GSM registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION0[7] = "+CGATT"; // GPRS registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION[7] = "+CGREG"; // GPRS registration.
const char OTSIM900Link::AT_SET_APN[6] = "+CSTT";
const char OTSIM900Link::AT_START_GPRS[7] = "+CIICR";
const char OTSIM900Link::AT_GET_IP[7] = "+CIFSR";
const char OTSIM900Link::AT_PIN[6] = "+CPIN";

const char OTSIM900Link::AT_STATUS[11] = "+CIPSTATUS";
const char OTSIM900Link::AT_START_UDP[10] = "+CIPSTART";
const char OTSIM900Link::AT_SEND_UDP[9] = "+CIPSEND";
const char OTSIM900Link::AT_CLOSE_UDP[10] = "+CIPCLOSE";
const char OTSIM900Link::AT_SHUT_GPRS[9] = "+CIPSHUT";

const char OTSIM900Link::AT_VERBOSE_ERRORS[6] = "+CMEE";

// tcpdump -Avv udp and dst port 9999
} // OTSIM900Link

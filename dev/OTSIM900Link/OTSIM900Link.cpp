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

#include "OTSIM900Link.h"


/**
 * @brief	Constructor. Initializes softSerial and sets PWR_PIN
 * @todo	Can/should pinMode be replaced?
 * @param	pwrPin		SIM900 power on/off pin
 * @param	rxPin		Rx pin for software serial
 * @param	txPin		Tx pin for software serial
 */
OTSIM900Link::OTSIM900Link(uint8_t pwrPin, SoftwareSerial *_softSerial) : PWR_PIN(pwrPin)
{
  pinMode(PWR_PIN, OUTPUT);
  softSerial = _softSerial;
}

/**
 * @brief	begin software serial
 * @todo	What is this actually going to do?
 * 			- Turning module on automatically starts radio, so this does not fit desired begin behaviour
 * 			- There is a low power mode but this still requires an active connection to stay registered
 * 			- APN etc need to be set after each power up. Is this just going to store these in object?
 *          why can't I set softSerial->begin like this?
 * @param	baud	baudrate communicating with SIM900
 */
bool OTSIM900Link::begin()
{

	//powerOn();

	// perform steps that can be done without network connection
	/*								// these are broken
	if (checkPIN()) {
		setPin(PIN, sizeof(PIN));	// will probably end up being *PIN and length of pin
	}

	setAPN(APN, sizeof(APN));		// see setPin

	// block until network registered
	while(!isRegistered) {}			// FIXME: Add delay in loop

	startGPRS();
	*/

	return false;
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
 * @brief	open UDP connection to input ip address
 * @todo	clean up writes
 * @param	array containing IP address to open connection to in format xxx.xxx.xxx.xxx (keep all zeros)
 * @retval	returns true if UDP opened
 * @note	is it necessary to check if UDP open?
 */
bool OTSIM900Link::openUDP(const char *address, uint8_t addressLength, const char *port, uint8_t portLength)
{
	//if(!isOpenUDP()){
		write(AT_START, sizeof(AT_START));
		write(AT_START_UDP, sizeof(AT_START_UDP));
		write("=\"UDP\",", 7); // FIXME!
		write('\"');
		write(address, addressLength);
		write("\",\"", 3);
		write(port, portLength);
		write('\"');
		write(AT_END);
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
	if(isOpenUDP()){
		write(AT_START, sizeof(AT_START));
		write(AT_CLOSE_UDP, sizeof(AT_CLOSE_UDP));
		write(AT_END);
		return false;
	}
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
	if(isOpenUDP()){
		uint32_t time = millis();
		write(AT_START, sizeof(AT_START));
		write(AT_SEND_UDP, sizeof(AT_SEND_UDP));
		write('=');
		print(length);
		write(AT_END);

		// '>' indicates module is ready for UDP frame
		if (waitForTerm('>')) {
			write(frame, length);
			return true;	// add check here
		} else return false;
	}
}

/**
 * @brief	check if module has power
 * @todo	better check?
 * @retval	true if module is powered up
 */
bool OTSIM900Link::isPowered()
{
	char data[64];
	memset(data, 0 , sizeof(data));
	write(AT_START, sizeof(AT_START));
	write(AT_END);
	if((timedBlockingRead(data, sizeof(data)) > 0) &&
     (data[0] == 'A'))
     { return true; }
	else return false;
}

/**
 * @brief	Reads a single character from softSerial
 * @retval	returns character read, or 0 if no data to read
 */
uint8_t OTSIM900Link::read()
{
	uint8_t data;
	data = softSerial->read();
	if (data > 0) return data;
	else return 0;
}

/**
 * @brief	Enter blocking read. Fills buffer or times out after 100 ms
 * @todo	what to do with terminatingChar? Using 0 as default fails?
 * 			Overloaded for now
 * @param	data	data buffer to write to
 * @param	length	length of data buffer
 * @retval	length of data received before time out
 */
uint8_t OTSIM900Link::timedBlockingRead(char *data, uint8_t length, const char terminatingChar)
{;
  // clear buffer, get time and init i to 0
  memset(data, 0, length);
  uint8_t i = 0;

  uint32_t startTime = millis();
  // 100ms is time to fill buffer? probs got maths wrong on this.
  // May have to wait a little longer because of (eg) interactions with the network,
  // especially if a terminating char is known.
  const bool hasTerminatingChar = (0 != terminatingChar);
  const uint32_t timeoutms = 2000;//hasTerminatingChar ? 500 : 200;	FIXME return to normal
  while ((millis() - startTime) <= timeoutms) {
    if (softSerial->available() > 0) {
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
  }
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
bool OTSIM900Link::waitForTerm(uint8_t terminatingChar)
{
  //unsigned long startTime = millis();
  uint32_t endTime = millis() + 1000; // time out after a second
  while(millis() < endTime) {
    const uint8_t c = read();
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
	softSerial->write(data, length);
}

/**
 * @brief	Writes a character to software serial
 * @param	data	character to write
 */
void OTSIM900Link::write(char data)
{
	softSerial->write(data);
}

/**
 * @brief  Writes a character to software serial
 * @param data  character to write
 */
void OTSIM900Link::print(const int value)
{
  softSerial->print(value);
}

/**
 * @brief	Checks module ID
 * @todo	Implement check?
 * @param	name	pointer to array to compare name with
 * @param	length	length of array name
 * @retval	returns true if ID recovered successfully
 */
bool OTSIM900Link::checkModule(/*const char *name, uint8_t  length*/)
{
  char data[32];
  write(AT_START, sizeof(AT_START));
  write(AT_GET_MODULE);
  write(AT_END);
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
bool OTSIM900Link::checkNetwork(char *buffer, uint8_t length)
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_NETWORK, sizeof(AT_NETWORK));
  write(AT_QUERY);
  write(AT_END);
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
  write(AT_START, sizeof(AT_START));
  write(AT_REGISTRATION, sizeof(AT_REGISTRATION));
  write(AT_QUERY);
  write(AT_END);
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
bool OTSIM900Link::setAPN(const char *APN, uint8_t length)
{
  char data[96];
  write(AT_START, sizeof(AT_START));
  write(AT_SET_APN, sizeof(AT_SET_APN));
  write(AT_SET);
  write('\"');
  write(APN, length);
  write('\"');
  write(AT_END);

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
  write(AT_START, sizeof(AT_START));
  write(AT_START_GPRS, sizeof(AT_START_GPRS));
  write(AT_END);
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
  write(AT_START, sizeof(AT_START));
  write(AT_SHUT_GPRS, sizeof(AT_SHUT_GPRS));
  write(AT_END);
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
uint8_t OTSIM900Link::getIP(char *IPAddress)
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_GET_IP, sizeof(AT_GET_IP));
  write(AT_END);
  timedBlockingRead(data, sizeof(data));

  // response stuff
  const char *dataCut;
  uint8_t dataCutLength = 0;
  dataCut= getResponse(dataCutLength, data, sizeof(data), 0x0A);

  if(*dataCut == '+') return 0; // all error messages will start with a '+'
  else {
	  memcpy(IPAddress, dataCut, dataCutLength);
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
	write(AT_START, sizeof(AT_START));
	write("+CIPSTATUS", 10);
	write(AT_END);
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
  write(AT_START, sizeof(AT_START));
  write(AT_VERBOSE_ERRORS, sizeof(AT_VERBOSE_ERRORS));
  write(AT_SET);
  write('0'); // 0: no error codes, 1: error codes, 2: full error descriptions
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
#endif // OTSIM900LINK_DEBUG
}

/**
 * @brief   Enter PIN code
 * @param   pin     pointer to array containing pin code
 * @param   length  length of pin
 */
void OTSIM900Link::setPIN(const char *pin, uint8_t length)
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_PIN, sizeof(AT_PIN));
  write(AT_SET);
  write(pin, length);
  write(AT_END);
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
  write(AT_START, sizeof(AT_START));
  write(AT_PIN, sizeof(AT_PIN));
  write(AT_QUERY);
  write(AT_END);
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
	Serial.print("start ptr: ");
	Serial.print((uint16_t) newPtr);
	Serial.print("\tend ptr: ");
	Serial.print((uint16_t) data);
	Serial.print("\tlength: ");
	Serial.println(newLength);

	Serial.print("dataCut: ");
	Serial.write(newPtr, newLength);
	Serial.println();
#endif // OTSIM900LINK_DEBUG

	return newPtr;	// return length of new array
}

//const char OTSIM900Link::AT_[] = { }
const char OTSIM900Link::AT_START[2] = { 'A', 'T' };
const char OTSIM900Link::AT_NETWORK[5] = { '+', 'C', 'O', 'P', 'S'};
const char OTSIM900Link::AT_REGISTRATION[5] = { '+', 'C', 'R', 'E', 'G' }; // GSM registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION0[6] = { '+', 'C', 'G', 'A', 'T', 'T' }; // GPRS registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION[6] = { '+', 'C', 'G', 'R', 'E', 'G' }; // GPRS registration.
const char OTSIM900Link::AT_SET_APN[5] = { '+', 'C', 'S', 'T', 'T' };
const char OTSIM900Link::AT_START_GPRS[6] = { '+', 'C', 'I', 'I', 'C', 'R' };
const char OTSIM900Link::AT_SHUT_GPRS[8] = { '+', 'C', 'I', 'P', 'S', 'H', 'U', 'T' };
const char OTSIM900Link::AT_GET_IP[6] = { '+', 'C', 'I', 'F', 'S', 'R' };
const char OTSIM900Link::AT_PIN[5] = { '+', 'C', 'P', 'I', 'N' };
const char OTSIM900Link::AT_START_UDP[9] = { '+', 'C', 'I', 'P', 'S', 'T', 'A', 'R', 'T' };
const char OTSIM900Link::AT_SEND_UDP[8] = { '+', 'C', 'I', 'P', 'S', 'E', 'N', 'D' };
const char OTSIM900Link::AT_CLOSE_UDP[9] = { '+', 'C', 'I', 'P', 'C', 'L', 'O', 'S', 'E' };
#ifdef OTSIM900LINK_DEBUG
const char OTSIM900Link::AT_VERBOSE_ERRORS[5] = { '+', 'C', 'M', 'E', 'E' };
#endif // OTSIM900LINK_DEBUG

// tcpdump -Avv udp and dst port 9999

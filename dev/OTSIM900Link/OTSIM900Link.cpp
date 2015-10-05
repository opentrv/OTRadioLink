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
 * @brief	begin software serial and power up SIM module
 * @todo	work out setPin and setAPN
 *          why can't I set softSerial->begin like this?
 * @param	baud	baudrate communicating with SIM900
 */
bool OTSIM900Link::begin()
{
	//softSerial->begin(baud);	// SoftwareSerial breaks if anything up to and including this happens in class
	powerOn();

	// perform steps that can be done without network connection
	/*								// these are broken
	if (checkPIN()) {
		setPin(PIN, sizeof(PIN));	// will probably end up being *PIN and length of pin
	}

	setAPN(APN, sizeof(APN));		// see setPin

	// block until network registered
	while(!isRegistered) {/* delay function *//*}

	startGPRS();
	*/

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
 * @brief	open UDP connection to input ip address
 * @param	array containing IP address to open connection to in format xxx.xxx.xxx.xxx (keep all zeros)
 * @retval	returns true if UDP opened
 * @note	check gprs and if UDP already open
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
	//if(isOpenUDP()){
		write(AT_START, sizeof(AT_START));
		write(AT_CLOSE_UDP, sizeof(AT_CLOSE_UDP));
		write(AT_END);
		return false;
	//}
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
	//if(isOpenUDP()){
		write(AT_START, sizeof(AT_START));
		write(AT_SEND_UDP, sizeof(AT_SEND_UDP));
		write('=');
		print(length);
		write(AT_END);

		// check for correct response?
    // The ">" prompt will appear somewhere at the end of the buffer.
    waitForTerm('>');
    write(frame, length);

    return true;	// add check here
	//}
}

/**
 * @brief	check if module has power
 * @todo	better check?
 * @retval	true if module is powered up
 */
bool OTSIM900Link::isPowered()
{
	char data[9];
	memset(data, 0 , sizeof(data));
	write(AT_START, sizeof(AT_START));
	write(AT_END);
	if((timedBlockingRead(data, sizeof(data)) > 0) &&
     (data[0] == 'A'))
     { return true; }
	else return false;
}

/**
 * @brief	Enter blocking read. Fills buffer or times out after 100 ms
 * @param	data	data buffer to write to
 * @param	length	length of data buffer
 * @retval	length of data received before time out
 */
uint8_t OTSIM900Link::timedBlockingRead(char *data, uint8_t length, const char terminatingChar)
{;
  // clear buffer, get time and init i to 0
  memset(data, 0, length);
  uint8_t i = 0;

  uint16_t startTime = millis();
  // 100ms is time to fill buffer? probs got maths wrong on this.
  // May have to wait a little longer because of (eg) interactions with the network,
  // especially if a terminating char is known.
  const bool hasTerminatingChar = (0 != terminatingChar);
  const uint16_t timeoutms = hasTerminatingChar ? 500 : 200;
  while ((millis() - startTime) <= timeoutms) {
    if (softSerial->available() > 0) {
      const char c = softSerial->read();
      *data++ = c;
      if(hasTerminatingChar && (c == terminatingChar)) { break; }
      i++;
    }
    // break if receive too long.
    if (i >= length) {
      Serial.println("\n--Serial Overrun");
      // FIXME: rest of input still had to be absorbed to avoid fouling next interaction.
      break;
    }
  }
  Serial.print("\n--Buffer Length: ");
  Serial.println(i);
  return i;
}

/**
 * @brief   blocks process until module ready to take send string
 */
void OTSIM900Link::waitForTerm(uint8_t terminatingChar)
{
  unsigned int startTime = millis();
  unsigned int endTime = startTime + 1000; // time out after a second
  while(startTime < endTime) {
    const uint8_t c = softSerial->read();
    if (c == terminatingChar) return;
  }
  Serial.println("Timeout");
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
 * @todo	Implement check
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
  Serial.println(data);
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
  Serial.println(data);
  return true;
}

/**
 * @brief 	check if module connected and registered (GSM and GPRS)
 * @todo	implement check
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
  Serial.println(data);
  delay(100);
  write(AT_START, sizeof(AT_START));
  write(AT_GPRS_REGISTRATION0, sizeof(AT_GPRS_REGISTRATION0));
  write(AT_QUERY);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
  delay(100);
  write(AT_START, sizeof(AT_START));
  write(AT_GPRS_REGISTRATION, sizeof(AT_GPRS_REGISTRATION));
  write(AT_QUERY);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);

  return true;
}

/**
 * @brief	Set Access Point Name and start task
 * @param	APN		pointer to access point name
 * @param	length	length of access point name
 */
void OTSIM900Link::setAPN(const char *APN, uint8_t length)
{
  char data[128];
  write(AT_START, sizeof(AT_START));
  write(AT_SET_APN, sizeof(AT_SET_APN));
  write(AT_SET);
  write('\"');
  write(APN, length);
  write('\"');
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
}

/**
 * @brief	Start GPRS connection
 * @todo	check for OK response on each set?
 * 			check syntax correct
 * @retval	returns true if connected
 * @note	check power, check registered, check gprs active
 */
bool OTSIM900Link::startGPRS()
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_START_GPRS, sizeof(AT_START_GPRS));
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
  return true;
}

/**
 * @brief	Get IP address
 * @todo	implement returning IP address
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
  Serial.println(data);

  delay(100);
  write(AT_START, sizeof(AT_START));
  write("+CIPSTATUS", 10);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
  return 0;	// this needs to change
}

/**
 * @brief	check if UDP open
 * @todo	implement function
 * @retval	true if open
 */
bool OTSIM900Link::isOpenUDP()
{

}

/**
 * @brief   Set verbose errors
 */
void OTSIM900Link::verbose()
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_VERBOSE_ERRORS, sizeof(AT_VERBOSE_ERRORS));
  write(AT_SET);
  write('2'); // 0: no error codes, 1: error codes, 2: full error descriptions
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);
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
  Serial.println(data);
}

/**
 * @brief   Check if PIN required
 * @todo    return logic
 */
bool OTSIM900Link::checkPIN()
{
  char data[64];
  write(AT_START, sizeof(AT_START));
  write(AT_PIN, sizeof(AT_PIN));
  write(AT_QUERY);
  write(AT_END);
  timedBlockingRead(data, sizeof(data));
  Serial.println(data);

  return false;
}

//const char OTSIM900Link::AT_[] = { }
const char OTSIM900Link::AT_START[2] = { 'A', 'T' };
const char OTSIM900Link::AT_NETWORK[5] = { '+', 'C', 'O', 'P', 'S'};
const char OTSIM900Link::AT_REGISTRATION[5] = { '+', 'C', 'R', 'E', 'G' }; // GSM registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION0[6] = { '+', 'C', 'G', 'A', 'T', 'T' }; // GPRS registration.
const char OTSIM900Link::AT_GPRS_REGISTRATION[6] = { '+', 'C', 'G', 'R', 'E', 'G' }; // GPRS registration.
const char OTSIM900Link::AT_SET_APN[5] = { '+', 'C', 'S', 'T', 'T' };
const char OTSIM900Link::AT_START_GPRS[6] = { '+', 'C', 'I', 'I', 'C', 'R' };
const char OTSIM900Link::AT_GET_IP[6] = { '+', 'C', 'I', 'F', 'S', 'R' };
const char OTSIM900Link::AT_VERBOSE_ERRORS[5] = { '+', 'C', 'M', 'E', 'E' };
const char OTSIM900Link::AT_PIN[5] = { '+', 'C', 'P', 'I', 'N' };
const char OTSIM900Link::AT_START_UDP[9] = { '+', 'C', 'I', 'P', 'S', 'T', 'A', 'R', 'T' };
const char OTSIM900Link::AT_SEND_UDP[8] = { '+', 'C', 'I', 'P', 'S', 'E', 'N', 'D' };
const char OTSIM900Link::AT_CLOSE_UDP[9] = { '+', 'C', 'I', 'P', 'C', 'L', 'O', 'S', 'E' };

// tcpdump -Avv udp and dst port 9999

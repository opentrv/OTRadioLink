# OTSoftSerial2 Implementation Notes
## Todo:
- [x] Rewrite to use stream interface Arduino libs (arduino-1.6.7/hardware/arduino/avr/cores/arduino/Stream.h)
- [ ] Move read into interrupt (although still blocking).
- [ ] Make non-blocking.
- [ ] Template the class to speed it up.

## Current Problems (20160415):
1. Have to block for long periods of time while waiting for Rx.
2. Uses slow Arduino digitalWrite functions as pins are set at runtime.
3. CPU clock rate no longer reliable (might change during runtime).

## Solutions:
1. Optionally start Rx of each byte with an interrupt.
    - Interrupt on pin change to low.
    - Block until byte Rxed and then release.
    - Separate byte start detection logic from byte Rx logic so that it's easier to do both blocking and interrupt based Rx.
    - Need to think about state of interrupt registers on exit.
    - Need buffer to hold Rx message.
2. Template OTSoftSerial so that pins set at compile time.
    - This will allow fastDigitalWrite to use fast macros.
    - Previously not done due to unfortunate optimisations when templating.
    - Solving 1. will avoid this in our use cases (still might cause problems in non interrupt case)
3. Check clock speed before each Tx and Rx. Three options:
    - Force clock to something set at compile time.
    - Return error.
    - Adjust constants to compensate.

## Assumptions & interface additions:
1. Lowest clock speed is fast enough to accurately sample Rx line.
2. We have time to service interrupts between bytes.
3. Nothing will block too long between bytes.
    - May need to detect this case and retry.
4. Added 'sendBreak()' as we need it for the RN2483.
    
## Notes:
- Basic read char and write functions work with FTDI. Still need to test with OTSIM900Link.
- In order to receive multiple bytes:

As the library is not async and we are trying to preserve the Arduino Serial interface, read() must be called from a loop when receiving multiple bytes.
Example function that reads multiple characters into an array (this example is adapted from OTSIM900Link, which expects the function to return the first time it times out.):
/**
 * @brief    Enter blocking read. Fills buffer or times out after 100 ms
 * @param    data    data buffer to write to
 * @param    length  length of data buffer
 * @retval   number of characters received before time out
 */
uint8_t readLotsOfChars(char *data, uint8_t length)
{
    // clear buffer, get time and init i to 0
    uint8_t counter = 0;
    uint8_t len = length;
    char *pdata = data;
    
    // Loop while reading characters.
    while(len--) {
        char c = softSerial2.read();
        if(c == -1) break;  // softSerial2 return -1 on timeout, so we break out of the loop.
        *pdata++ = c;  // only want to write to the array or increment the counter if not timed out.
        counter++;
    }
  return counter;
}
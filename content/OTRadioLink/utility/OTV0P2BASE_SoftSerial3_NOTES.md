# OTSoftSerial2 Implementation Notes
## Todo:
- [x] Get initial interrupt read working.
- [ ] Fix issue with first read always failing.
- [ ] Fix general reliability problem (occasionally returns wrong values).
- [ ] Some way of disabling read interrupt when not needed.

## Current Problems (20160519):
1. First read set of values read **always** fail.
2. CPU clock rate no longer reliable (might change during runtime).
3. It's difficult to set how long read times are at higher speeds.
4. AVR has no barrel shift so read time increases with each bit read (only important at high baud).
5. Occasionally values read are wrong (e.g. 0x0A0D is read as 0x0C0B).
6. How to disable read interrupt without adding to interface?

## Solutions:
1. ???
2. Check clock speed before each Tx and Rx. Three options:
    - Force clock to something set at compile time.
    - Return error.
    - Adjust constants to compensate.
3. Made a bit easier by combining initial half-bit delay with first full bit delay.
4. N/A
5. ???
6. ???
    - add timeout in read() routine?

## Assumptions & interface additions:
1. Lowest clock speed is fast enough to accurately sample Rx line.
2. We have time to service interrupts between bytes.
3. Nothing will block too long between bytes.
    - May need to detect this case and retry.
4. Added 'sendBreak()' as we need it for the RN2483.
    
## Notes:
- Not using a circular buffer as not needed for this application:
    - Any write to serial resets buffer.
    - As values written to buffer, tail is incremented until it reaches the end of the buffer.
    - As values read from buffer, head is incremented until it reaches the tail.
- This means characters are discarded after the read buffer is full.

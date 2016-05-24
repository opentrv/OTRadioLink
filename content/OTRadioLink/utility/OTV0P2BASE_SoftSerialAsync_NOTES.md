# OTSoftSerialAsync Implementation Notes
**NOTE:** The header file 'OTV0P2BASE_SoftSerialAsync.h' is not currently included in 'OTV0p2Base.h' and must be added in to use this library.
## Todo:
- [x] Get initial interrupt read working.
- [ ] Fix issue with first read always failing.
- [ ] Fix general reliability problem (occasionally returns wrong values).
- [ ] Some way of disabling read interrupt when not needed.
- [ ] Implement circular buffer (not required for V0p2 but nice to have).
- [ ] Get it running fast enough to read all 8 bits at 9600 baud.

## Current Problems (20160523):
1. First read **always** fails due to time taken to enter first interrupt.
2. Problem with interrupt timings at 9600 baud:
    1. Cannot enter and exit interrupts fast enough to read a full octet. **We currently only read the first SEVEN bits of a packet**
    2. PCINT interrupts trigger on **both** pin change edges. This means we have a spurious interrupt triggered at the end of each incoming byte.
    3. Other interrupts cause handle_interrupt to be called too late to catch the first bit of an octet. This can foul the entire packet (in part due to problem above).

## Possible Solutions:
1. Trigger a read before actual use.
2. ???

## Assumptions & interface additions/changes:
1. Lowest clock speed is fast enough to accurately sample Rx line.
2. ~~We have time to service interrupts between bytes.~~ See problem 2.
3. ~~Nothing will block too long between bytes.~~ See problem 2.
4. Added 'sendBreak()' as we need it for the RN2483.
5. rxPin, txPin and speed are passed in as template parameters. begin can still be called as usual but the value passed to it will be ignored.
    
## Notes:
- Not using a circular buffer as not needed for this application:
    - Any write to serial resets buffer.
    - As values written to buffer, tail is incremented until it reaches the end of the buffer.
    - As values read from buffer, head is incremented until it reaches the tail.
- This means characters are discarded after the read buffer is full.
- Ignoring final bit as otherwise it doesn't reenter the interrupt quickly enough.

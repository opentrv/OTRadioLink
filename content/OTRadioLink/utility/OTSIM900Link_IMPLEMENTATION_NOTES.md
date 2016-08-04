# OTSIM900Link Implementation Notes

## Todo
- [ ] Async read to prevent long periods of blocking.
- [ ] Sort out getting config from EEPROM.
- [ ] Move implementation into source file without breaking templating.

## Problems
1. Blocks for long periods of time while waiting for response.
1. Undocumented state where SIM900 gets stuck in PDP-DEACT.
1. SIM900 stops sending packets, although it behaves as if it is connected normally.

## Notes
- The current version of OTSoftSerial (20160523) can run at a maximum of 9600 baud.
- Can autobaud but this will cause issues if the SIM900 is powered up when switching baud. Resetting the SIM900 will fix this.
- SIM900 shield pin out:

|Shield         | Arduino UNO | V0p2_REV10 |
| ------------- | ----------- | ---------- |
|soft serial Rx | D7          | 8          |
|soft serial Tx | D8          | 5          |
|power toggle   | D9          | A2         |
- ~~To edit setSim900Baud.ino:~~
    - ~~set initialBaud to current SIM900 baud~~
    - ~~set targetBaud to the baud you want the SIM900 to be set to.~~
- For some reason it needs getIP() (AT+CIFSR) to be called after startGPRS() in the state machine version.
- Recommend testing with "tcpdump -X udp port 9999" (-X prints both hex and ascii)
- Define OTSIM900Link_DEBUG for debug output. This can cause long (> 300 ms) blocks. In practice this hasn't caused a problem so far.

## Preparing the SIM900 for use with the REV10

1. Find the power pin jumper (JP on the geeetech SIM900 v2.0) and bridge it with a blob of solder.

2. Make sure the two serial selector jumpers are set to SWserial (J7 on the geeetech SIM900 v2.0).

3. Put a CR1220 coin cell in the SIM900 shield.

2) ~~Setting the baud on the SIM900:~~ (deprecated as of 20160523 as can now run serial faster and autobaud.)

IMPORTANT: The current V0p2 with softserial cannot reach a high enough baud to communicate with the SIM900 by default. The following procedure is only tested on the Arduino UNO (20160311).

    - Make sure the two serial selector jumpers are set to SWserial (J7 on the geeetech SIM900 v2.0).
    - Put a CR1220 coin cell in the SIM900 shield.
    - Load the sketch OTRadioLink/dev/utils/setSim900Baud/setSim900Baud.ino onto an Arduino UNO (do NOT edit the sketch!).
    - Unplug the Arduino and attach the SIM900 shield.
    - If step 1 has been carried out, the SIM900 should power up on its own. Otherwise, press and hold the power button on the SIM900 until the red light comes on and reset the arduino.
    - Expected response on success:
```
++ Setting Baud to 2400 ++
AT+IPR=2400

OK
```

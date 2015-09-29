Pre-V1.5-IDE AVR-only 'OTRadioLink' OpenTRV Radio Comms Link definitions library.

This consists of:

  * The 'content' folder with the unpackaged files exactly as they should be unpacked,
    including the top-level folder <LIBRARYNAME>

  * The zipped or otherwise bundled distribution format <LIBRARYNAME>.<format> binary.

  * The test directory containing an Arduino project performing unit/other tests on the library source.



Currently also contains other libraries, in their own namespaces,
which may be separated out.

The dependency order is:

OTRFM23BLink

OTRadioLink

OTV0P2BASE

The lowest of these nominally depends only on the Arduino run-time and the hardware.
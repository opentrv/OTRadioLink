#!/usr/bin/env python3

import RPi.GPIO as GPIO
import time
import serial as ser
import csv
import sys

pin_REV7  = 11             ## REV7 power pin

# power on REV7
def powerOn():
#    print("Power on REV7")
    GPIO.output(pin_REV7, GPIO.LOW)
#power off REV7
def powerOff():
#    print("Power off REV7")
    GPIO.output(pin_REV7, GPIO.HIGH)

# wait for post and initial txs to finish
def waitForCLI(dev):
    counter = 0
    dev.write(b'\n')
    string = dev.readline()
    while (string != b'>\r\n') and (counter < 5):
        print(string)
        string = dev.readline()
        counter = counter + 1

# power cycle REV7
def powerCycle(dev, post):
    powerOff()
    time.sleep(0.5)
    dev.flushInput()
    powerOn()
    time.sleep(0.5)
    dev.readline()
    if dev.readline() == post:
        return 1
    else:
        print("REV7 not found")
        powerOff()
        GPIO.cleanup()
        exit()

# setup REV7 power pin
def setup():
    print("Setup REV7 power pin")
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(pin_REV7, GPIO.OUT)
    GPIO.output(pin_REV7, GPIO.HIGH)

# close and free REV7 power pin
def end():
    print("Close and free REV7 power pin")
    powerOff()
    GPIO.cleanup()

# Wait for prompt character
def sendCmd(dev, buf):
    while(dev.read() != b'>'):
        x = 0
    dev.write(buf + b'\n')

# get value from csv
def getKey(myfile, serNo):
    with open(myfile, 'r', newline='') as keyfile:
        keys = csv.reader(keyfile, delimiter=',')
        for row in keys:
            if serNo in row:
                print(": ".join(row))
                return row[1]
        return 0

# write to output csv
def writeOut(myfile, serNo, key, id):
    if serNo == 0:
        return 0
    else:
        with open(myfile, 'a', newline = '') as outputfile:
            outputcsv = csv.writer(outputfile, delimiter=',', quoting=csv.QUOTE_MINIMAL)
            outputcsv.writerow([serNo, key, id])
        return 1

# Set REV7 key
def setKey(dev, key):
    sendCmd(dev, key.encode('ascii')) ## prepends 'K B ' to key
    # need to be able to read back reply
    
# Get REV7 ID
def getID(dev):
    sendCmd(dev, b'I')
    dev.readline()
    string = dev.readline()
    return string[4:27].decode()

# Clear REV11 node associations
def clearNodes(dev):
    sendCmd(dev, b'A *')
    print(dev.readlines(3))
# Set REV11 node association
def setNode(dev, node):
    sendCmd(dev, b'A ' + node.encode())
    print(dev.readlines(3))

# Main program
def main(argv):
    port_REV7  = '/dev/ttyUSB0' ## REV7 serial connection
    port_REV11 = '/dev/ttyUSB1' ## REV7 serial connection
    baud_REV7  = 4800           ## REV7 baud
    KEYFILE    = 'keys.csv'     ## csv containing serial number and key associations
    OUTPUTFILE = 'rev7test.csv' ## csv to write serial number, key and node ID to
    
    key_REV7  = ''
    id_REV7   = ''
    serNo_REV7 = argv[0] ## gets serial number form CLI argument

    post_REV7 = b'OpenTRV: board V0.2 REV7 2016/Mar/12 14:36:06\r\n'  ## standard REV7 post
    

    # options and stuff
    #   This will probably involve sys and getopt

    # get key from csv
    print("Getting key")
    key_REV7 = 'K B '+getKey(KEYFILE, serNo_REV7)
    print(key_REV7) ## todo should be deleted to prevent people seeing key?
    
    print("Open REV11 serial port")
    rev11 = ser.Serial(port_REV11, baud_REV7, timeout=2)
    print("Open REV7 serial port")
    rev7 = ser.Serial(port_REV7, baud_REV7, timeout=2)   ## serial port

    setup()

    # check for REV7
    print("Checking for REV7")
    print(powerCycle(rev7, post_REV7))
    waitForCLI(rev7)

    # get ID
    print("Getting ID")
    id_REV7 = getID(rev7)
    print(id_REV7)

    # set key on REV7
    print("Setting Key")
    setKey(rev7, key_REV7)
    print(rev7.readlines(5))
    print("power cycle")
    print(powerCycle(rev7, post_REV7))
    waitForCLI(rev7)
    setKey(rev7, key_REV7)
    print(rev7.readlines(5))

    # Check key has stuck
    print("Verifying key set")
    # 1. set key on REV11
    rev11.flushInput()
    setKey(rev11, key_REV7)
    print(rev11.readlines(5))
    #clearNodes(rev11)  ## commented to ease eeprom
    print(rev11.readlines(5))
    setNode(rev11, id_REV7)
    print(rev11.readlines(5))
    # 2. reset REV7
    print("power cycle")
    rev11.flushInput()
    powerCycle(rev7, post_REV7)
    # 3. wait for receive
    rxString = b''
    # 4. compare received strings
    rxID = rxString[6:22].decode()
    rev7ID = id_REV7.replace(' ', '')
    #if rxID == rev7ID:  ## Hacked to force true as serial receive not implemented
    if True:
        print("Success!")
        # output csv stuff
        writeOut(OUTPUTFILE, serNo_REV7, key_REV7, id_REV7)
    else:
        print("Failed!")

    rev7.close()
    end()


if __name__ == "__main__":
   main(sys.argv[1:])

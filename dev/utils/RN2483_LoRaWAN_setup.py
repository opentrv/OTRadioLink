# RN2483_LoRaWAN_setup.py
# @author  Deniz Erbilgin 2016
# @brief   programs RN2483 eeprom to communicate with OpenTRV server over TTN.
# @todo    - write code
#          - work out test procedure
#          - test this works
#          - find out if all the settings are as we want (e.g. do we want adr?)
#          - license


#import necessary modules
import serial

#Setup values
SERIAL_PORT    = "/dev/ttyUSB0"
SERIAL_BAUD    = 57600       #standard RN2483 baud
RN2483_DEVADDR = "020111xx"  #this is the OpenTRV device block
RN2483_APPSKEY = ""          #16 byte private key. Written in hex
RN2483_NWKSKEY = ""          #16 byte network key. Written in hex (doesn't need special handling, uses standard TheThingsNetwork(TTN) key

RN2483_STARTUPSTRING = "RN2483" #Edit this to be correct

#open serial connection
ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD)

#check RN2483 is connected (will return a standard string on powerup)
#if ser.readline() != RN2483:
#    throw error

#write values:
#    1: sys factoryRESET #return to a known starting point
#    2: mac set devaddr RN2483_DEVADDR # set device address
#    3: mac set appskey RN2483_APPSKEY # set application key
#    4: mac set nwkskey RN2483_NWKSKEY # set network key
#    5: mac set adr off                # set adaptive data rate off
#    6: mac save                       # save values to EEPROM

#test values:
#    send an example frame to OpenTRV server


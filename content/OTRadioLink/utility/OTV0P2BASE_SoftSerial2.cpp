/*
 * OTV0P2BASE_SoftSerial2.cpp
 *
 *  Created on: 15 Apr 2016
 *      Author: denzo
 */

#include "OTV0P2BASE_SoftSerial2.h"

namespace OTV0P2BASE
{

OTSoftSerial2::OTSoftSerial2(uint8_t _rxPin, uint8_t _txPin): rxPin(_rxPin), txPin(_txPin)
{

}

void OTSoftSerial2::begin(long speed) {
}

bool OTSoftSerial2::listen() {
}

void OTSoftSerial2::end() {
}

bool OTSoftSerial2::isListening() {
}

bool OTSoftSerial2::stopListening() {
}

bool OTSoftSerial2::overflow() {
}

int OTSoftSerial2::peek() {
}

size_t OTSoftSerial2::write(uint8_t byte) {
}

int OTSoftSerial2::read() {
}

int OTSoftSerial2::available() {
}

void OTSoftSerial2::flush() {
}

}

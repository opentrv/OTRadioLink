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

void OTV0P2BASE::OTSoftSerial2::begin(long speed) {
}

bool OTV0P2BASE::OTSoftSerial2::listen() {
}

void OTV0P2BASE::OTSoftSerial2::end() {
}

bool OTV0P2BASE::OTSoftSerial2::isListening() {
}

bool OTV0P2BASE::OTSoftSerial2::stopListening() {
}

bool OTV0P2BASE::OTSoftSerial2::overflow() {
}

int OTV0P2BASE::OTSoftSerial2::peek() {
}

size_t OTV0P2BASE::OTSoftSerial2::write(uint8_t byte) {
}

int OTV0P2BASE::OTSoftSerial2::read() {
}

int OTV0P2BASE::OTSoftSerial2::available() {
}

void OTV0P2BASE::OTSoftSerial2::flush() {
}

}

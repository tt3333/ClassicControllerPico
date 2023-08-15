/*
* Classic Controller Pico
* Copyright (c) 2023 T.T - tt3333@tt-server.net
* Copyright (c) 2011 Bruno Freitas - bruno@brunofreitas.com
*
* Wiimote Extension class based on the arduino-wiimote library (c) by
* Peter Brinkmann: https://gitorious.org/randomstuff/arduino-wiimote
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <Wire.h>
#include "WMExtension.h"
#include "WMCrypt.h"

TwoWire* WMExtension::wire;

/* Classic Controller ID */
const byte WMExtension::id[6] = { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x01 };

/*
 *  Classic Controller Calibration Data.
 * Last two bytes (checksum) will be calculated by the init() function
  */
byte WMExtension::calibration_data[16] = { 0xFC, 0x04, 0x7E, 0xFC, 0x04,
		0x7E, 0xFC, 0x04, 0x7E,	0xFC, 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00 };

/* Address requested by the I2C Master Device (i.e., the Wiimote) */
volatile byte WMExtension::address = 0;

/* Tells whether encryption was setup (enabled) or not */
volatile byte WMExtension::crypt_setup_done = 0;

/* Classic Controller 256 data registers */
byte WMExtension::registers[0x100];

/*
 * Callback function pointer that will be called after the Wiimote has requested
 * buttons status (state == 0x00 on handle_request function).
 *
 * First buttons status requested by the Wiimote will be always zeroed, which
 * don't represent a problem.
 *
 * User should provide the callback function which should then call
 * WMExtension::set_button_data function that updates the buttons_data array.
 *
 * The updated array will be provided to the Wiimote when it requests buttons
 * status.
 *
 * This pointer is set when user calls WMExtension::set_button_data_callback
 * which will usually happen during program setup, before calling
 * WMExtension::init function.
 */
WMExtension::CBackPtr WMExtension::cbPtr = NULL;

/* Sets the buttons state update callback function */
void WMExtension::set_button_data_callback(CBackPtr cb) {
	WMExtension::cbPtr = cb;
}

/* Returns 1 of the 16 possible bytes from the calibration data array */
byte WMExtension::get_calibration_byte(int b) {
	if(b >= 0 && b <= 15)
		return WMExtension::calibration_data[b];
	else
		return 0;
}

/*
 * Setup Wiimote <-> Extension I2C communication encryption, if requested by
 * the application (game/homebrew).
 */
void WMExtension::setup_encryption() {
	WMCrypt::wiimote_gen_key(WMExtension::registers + 0x40);

	WMExtension::crypt_setup_done = 1;
}

/*
 * Send 8 bytes data via Wire.write().
 * Supports Wiimote encryption, if enabled.
  */
void WMExtension::send_data(uint8_t* data, uint8_t addr) {
	static uint8_t buffer[21];
	int i, lim;

	lim = 0xFF - addr + 1;

	if(lim >= 21) {
		lim = 21;
	}

	if (WMExtension::crypt_setup_done) {
		for (i = 0; i < lim; i++) {
			buffer[i] = (data[i] - WMCrypt::wm_ft[(addr + i) % 8]) ^ WMCrypt::wm_sb[(addr + i) % 8];
		}

		WMExtension::wire->write(buffer, lim);
	} else {
		WMExtension::wire->write(data, lim);
	}
}

/* I2C slave handler for data received from the Wiimote */
void WMExtension::receive_bytes(int count) {
	byte crypt_keys_received = 0;
	byte old_crypt_key_received = 0;

	if (count == 1) {

		WMExtension::address = WMExtension::wire->read();

		return;

	} else if (count > 1) {

		byte addr = WMExtension::wire->read();

		for (int i = 1; i < count; i++) {
			byte d = WMExtension::wire->read();

			// Wii is trying to disable encryption...
			if(addr == 0xF0 && (d == 0x55 || d == 0xAA)) {
				WMExtension::crypt_setup_done = 0;
			}

			// Wii is probably trying to setup old encryption mode
			if(addr == 0x40 && d == 0x00) {
				old_crypt_key_received = 1;
			}

			if (WMExtension::crypt_setup_done) {
				// Decrypt
				WMExtension::registers[addr] = (d ^ WMCrypt::wm_sb[addr % 8]) + WMCrypt::wm_ft[addr
						% 8];
			} else {
				WMExtension::registers[addr] = d;
			}

			// Check if last crypt key setup byte was received...
			if (addr == 0x4F) {
				crypt_keys_received = 1;
			}

			addr++;
		}

	}

	// Setup encryption if requested by the Wii
	if (crypt_keys_received || old_crypt_key_received) {

		if(old_crypt_key_received)
			memset(WMExtension::registers + 0x40, 0x00, 16);

		WMExtension::setup_encryption();
	}
}

/* I2C slave handler for data request from the Wiimote */
void WMExtension::handle_request() {

	WMExtension::send_data(WMExtension::registers + WMExtension::address, WMExtension::address);

	if(WMExtension::address == 0x00) {
		if(WMExtension::cbPtr) {
			WMExtension::cbPtr();
		}
	}
}

/*
 * Takes joystick, and button values and encodes them
 * into a buffer.
 *
 * Classic Controller
 *
 * Buffer encoding details:
 * http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Classic_Controller
 */
#if 0
void WMExtension::set_button_data(int bdl, int bdr, int bdu, int bdd,
		int ba, int bb, int bx, int by, int blt, int brt, int bminus, int bplus,
		int bhome, byte lx, byte ly, byte rx, byte ry, int bzl, int bzr, int lt, int rt) {

	int buttons =
		(bzl    ? 0 : 1 << 15) |
		(bb     ? 0 : 1 << 14) |
		(by     ? 0 : 1 << 13) |
		(ba     ? 0 : 1 << 12) |
		(bx     ? 0 : 1 << 11) |
		(bzr    ? 0 : 1 << 10) |
		(bdl    ? 0 : 1 <<  9) |
		(bdu    ? 0 : 1 <<  8) |
		(bdr    ? 0 : 1 <<  7) |
		(bdd    ? 0 : 1 <<  6) |
		(blt    ? 0 : 1 <<  5) |
		(bminus ? 0 : 1 <<  4) |
		(bhome  ? 0 : 1 <<  3) |
		(bplus  ? 0 : 1 <<  2) |
		(brt    ? 0 : 1 <<  1) |
		1;

	WMExtension::set_button_data(buttons, lx, ly, rx, ry, lt, rt);
}
#endif

void WMExtension::set_button_data(int buttons)
{
	WMExtension::set_button_data(
		buttons, 
		WMExtension::calibration_data[2],
		WMExtension::calibration_data[5],
		WMExtension::calibration_data[8],
		WMExtension::calibration_data[11],
		buttons & (1 << 5) ? 0x00 : 0xFF,
		buttons & (1 << 1) ? 0x00 : 0xFF);
}

void WMExtension::set_button_data(int buttons, byte lx, byte ly, byte rx, byte ry, int lt, int rt) {
	// registers[0xFE] == 0x03: Read mode encoding used by the NES Classic Edition
	if(WMExtension::registers[0xFE] == 0x03) {
		WMExtension::registers[0] = lx;
		WMExtension::registers[1] = rx;
		WMExtension::registers[2] = ly;
		WMExtension::registers[3] = ry;
		WMExtension::registers[4] = lt;
		WMExtension::registers[5] = rt;
		WMExtension::registers[6] = lowByte(buttons);
		WMExtension::registers[7] = highByte(buttons);
		WMExtension::registers[8] = 0;
	} else if (WMExtension::registers[0xFE] == 0x02) {
		WMExtension::registers[0] = lx;
		WMExtension::registers[1] = rx;
		WMExtension::registers[2] = ly;
		WMExtension::registers[3] = ry;
		WMExtension::registers[4] = 0;
		WMExtension::registers[5] = lt;
		WMExtension::registers[6] = rt;
		WMExtension::registers[7] = lowByte(buttons);
		WMExtension::registers[8] = highByte(buttons);
	} else {
		lx = lx >> 2;
		ly = ly >> 2;
		rx = rx >> 3;
		ry = ry >> 3;
		lt = lt >> 3;
		rt = rt >> 3;

		WMExtension::registers[0] = ((rx & 0x18) << 3) | (lx & 0x3F);
		WMExtension::registers[1] = ((rx & 0x06) << 5) | (ly & 0x3F);
		WMExtension::registers[2] = ((rx & 0x01) << 7) | ((lt & 0x18) << 2) | (ry & 0x1F);
		WMExtension::registers[3] = ((lt & 0x07) << 5) | (rt & 0x1F);
		WMExtension::registers[4] = lowByte(buttons);
		WMExtension::registers[5] = highByte(buttons);
		WMExtension::registers[6] = 0;
		WMExtension::registers[7] = 0;
		WMExtension::registers[8] = 0;
	}
}

/*
 * Initializes Wiimote connection. Call this function in your
 * setup function.
 */
void WMExtension::init(TwoWire* wire) {
	byte calchecksum = 0;

	WMExtension::wire = wire;
	memset(WMExtension::registers, 0x00, 0x100);

	// Set extension id on registers
	for (int i = 0xFA; i <= 0xFF; i++) {
		WMExtension::registers[i] = WMExtension::id[i - 0xFA];
	}

	// Fix calibration data checksum, just in case...
	for(int i = 0; i < 14; i++) {
		calchecksum += WMExtension::calibration_data[i];
	}
	WMExtension::calibration_data[14] = calchecksum + 0x55;
	WMExtension::calibration_data[15] = calchecksum + 0xAA;

	// Set calibration data on registers
	for (int i = 0x20; i <= 0x2F; i++) {
		WMExtension::registers[i] = WMExtension::calibration_data[i - 0x20]; // 0x20
		WMExtension::registers[i + 0x10] = WMExtension::calibration_data[i - 0x20]; // 0x30
	}

	// Initialize buttons_data, otherwise, "Up+Right locked" bug...
	WMExtension::set_button_data(0xFFFF);

	// Join I2C bus
	wire->begin(0x52);

	wire->onReceive(WMExtension::receive_bytes);
	wire->onRequest(WMExtension::handle_request);
}




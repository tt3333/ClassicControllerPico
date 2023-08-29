#pragma once

//#define DEBUG

#define GET_BUTTONS()	(gpio_get_all() & 0x0000FFFE | 0x00000001)

#define PIN_UART_TX		16
#define PIN_UART_RX		17
#define PIN_I2C_SDA		18
#define PIN_I2C_SCL 	19
#define PIN_DETECT		20
#define PIN_3V3			21

#define UART			Serial1
#define UART_BAUDRATE	115200

#define I2C				Wire1
#define I2C_ADDRESS		0x52
#define I2C_CLOCK		400000	// 320kHz @ Wii Remote, 200kHz @ SNES Classic

#define DETECT_WAIT		100

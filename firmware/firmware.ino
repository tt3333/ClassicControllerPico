#include <Wire.h>

#include "config.h"
#include "WMExtension.h"

#ifdef DEBUG
	static unsigned long lastTime;

	#define DEBUG_SET_TX()		UART.setTX(PIN_UART_TX)
	#define DEBUG_SET_RX()		UART.setRX(PIN_UART_RX)
	#define DEBUG_BEGIN()		UART.begin(UART_BAUDRATE)
	#define DEBUG_PRINTF(...)	UART.printf(__VA_ARGS__)
	#define DEBUG_WRITE(c)		UART.write(c)
#else
	#define DEBUG_SET_TX()
	#define DEBUG_SET_RX()
	#define DEBUG_BEGIN()
	#define DEBUG_PRINTF(...)
	#define DEBUG_WRITE(c)
#endif

static bool ready = false;

static void button_data_callback()
{
	digitalWrite(LED_BUILTIN, HIGH);

	int buttons = GET_BUTTONS();
	WMExtension::set_button_data(buttons);

#ifdef DEBUG
	unsigned long now = millis();
	unsigned long elapsed = now - lastTime;
	if (elapsed > 16)
	{
		DEBUG_PRINTF("%lu\r\n", elapsed);
	}
	lastTime = now;
#endif

	digitalWrite(LED_BUILTIN, LOW);
}

void setup()
{
	// Initialize Detect pin first
	pinMode(PIN_DETECT, OUTPUT);
	pinMode(PIN_3V3, INPUT_PULLDOWN);
	pinMode(LED_BUILTIN, OUTPUT);

	for (int i = 0; i < 16; i++)
	{
		pinMode(i, INPUT_PULLUP);
	}

	DEBUG_SET_TX();
	DEBUG_SET_RX();
	DEBUG_BEGIN();

	I2C.setSDA(PIN_I2C_SDA);
	I2C.setSCL(PIN_I2C_SCL);
	I2C.setClock(I2C_CLOCK);

	WMExtension::set_button_data_callback(button_data_callback);
}

void loop()
{
	if (ready)
	{
		if (digitalRead(PIN_3V3) == LOW)
		{
			digitalWrite(PIN_DETECT, LOW);
			DEBUG_WRITE("disconnect\r\n");
			ready = false;
			I2C.end();
			delay(DETECT_WAIT);
		}
	}
	else
	{
		if (digitalRead(PIN_3V3) == HIGH)
		{
			delay(DETECT_WAIT);
			if (digitalRead(PIN_3V3) == HIGH)
			{
#ifdef DEBUG
				lastTime = millis();
#endif
				WMExtension::init(&I2C);
				ready = true;
				DEBUG_WRITE("connect\r\n");
				digitalWrite(PIN_DETECT, HIGH);
			}
		}
	}
}

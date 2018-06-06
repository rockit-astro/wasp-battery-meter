//**********************************************************************************
//  Copyright 2016, 2017 Paul Chote
//  This file is part of wasp-battery-meter, which is free software. It is made
//  available to you under version 3 (or later) of the GNU General Public License,
//  as published by the Free Software Foundation and included in the LICENSE file.
//**********************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Common/Common.h>
#include "usb_descriptors.h"

USB_ClassInfo_CDC_Device_t interface =
{
    .Config =
    {
        .ControlInterfaceNumber = INTERFACE_ID_CDC_CCI,
        .DataINEndpoint         =
        {
            .Address            = CDC_TX_EPADDR,
            .Size               = CDC_TXRX_EPSIZE,
            .Banks              = 1,
        },
        .DataOUTEndpoint        =
        {
            .Address            = CDC_RX_EPADDR,
            .Size               = CDC_TXRX_EPSIZE,
            .Banks              = 1,
        },
        .NotificationEndpoint   =
        {
            .Address            = CDC_NOTIFICATION_EPADDR,
            .Size               = CDC_NOTIFICATION_EPSIZE,
            .Banks              = 1,
        },
    },
};

#define USB_LED_UNPLUGGED PORTD &= ~_BV(PD0), PORTD &= ~_BV(PD1)
#define USB_LED_PLUGGED   PORTD |= _BV(PD0), PORTD &= ~_BV(PD1)
#define USB_LED_CONNECTED PORTD &= ~_BV(PD0), PORTD |= _BV(PD1)
#define USB_LED_INIT      DDRD |= _BV(PD0) | _BV(PD1)

#define TX_LED_DISABLED   PORTD &= ~_BV(5)
#define TX_LED_ENABLED    PORTD |= _BV(5)
#define RX_LED_DISABLED   PORTB &= ~_BV(0)
#define RX_LED_ENABLED    PORTB |= _BV(0)
#define TX_RX_LED_INIT    DDRD |= _BV(5), DDRB |= _BV(0), TX_LED_DISABLED, RX_LED_DISABLED

// Counters (in milliseconds) for blinking the TX/RX LEDs
#define TX_RX_LED_PULSE_MS 100
volatile uint8_t tx_led_pulse;
volatile uint8_t rx_led_pulse;

void usb_initialize(void)
{
    USB_LED_INIT;
    TX_RX_LED_INIT;
    USB_Init();
}

bool usb_can_read(void)
{
    return CDC_Device_BytesReceived(&interface) > 0;
}

// Read a byte from the receive buffer
// Will return negative if unable to read
int16_t usb_read(void)
{
    int16_t ret = CDC_Device_ReceiveByte(&interface);

    // Flash the RX LED
    if (ret >= 0)
    {
        RX_LED_ENABLED;
        rx_led_pulse = TX_RX_LED_PULSE_MS;
        USB_Device_EnableSOFEvents();
    }

    return ret;
}

// Add a byte to the send buffer.
// Will block if the buffer is full
void usb_write(uint8_t b)
{
    // Note: This is ignoring any errors (e.g. send failed)
    // We are only sending single byte packets, so there's no
    // real benefits to handling them properly
    if (CDC_Device_SendByte(&interface, b) != ENDPOINT_READYWAIT_NoError)
        return;

    if (CDC_Device_Flush(&interface) != ENDPOINT_READYWAIT_NoError)
        return;

    // Flash the TX LED
    TX_LED_ENABLED;
    tx_led_pulse = TX_RX_LED_PULSE_MS;
    USB_Device_EnableSOFEvents();
}

void usb_write_data(void *buf, uint16_t len)
{
    // Note: This is ignoring any errors (e.g. send failed)
    // We are only sending single byte packets, so there's no
    // real benefits to handling them properly
    if (CDC_Device_SendData(&interface, buf, len) != ENDPOINT_READYWAIT_NoError)
        return;

    if (CDC_Device_Flush(&interface) != ENDPOINT_READYWAIT_NoError)
        return;

    // Flash the TX LED
    TX_LED_ENABLED;
    tx_led_pulse = TX_RX_LED_PULSE_MS;
    USB_Device_EnableSOFEvents();
    
}

void EVENT_USB_Device_ConfigurationChanged(void)
{
    CDC_Device_ConfigureEndpoints(&interface);
}

void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
    bool connected = CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR;
    if (connected)
        USB_LED_CONNECTED;
    else
        USB_LED_PLUGGED;
}

void EVENT_USB_Device_Connect(void)
{
    USB_LED_PLUGGED;
}

void EVENT_USB_Device_Disconnect(void)
{
    USB_LED_UNPLUGGED;

    // The SOF event will not fire while the device is disconnected
    // so make sure that the TX/RX LEDs are turned off now
    TX_LED_DISABLED;
    RX_LED_DISABLED;
}

void EVENT_USB_Device_ControlRequest(void)
{
    CDC_Device_ProcessControlRequest(&interface);
}

void EVENT_USB_Device_StartOfFrame(void)
{
    // SOF event runs once per millisecond when enabled
    // Use this to count down and turn off the RX/TX LEDs.
    if (tx_led_pulse && !(--tx_led_pulse))
        TX_LED_DISABLED;
    if (rx_led_pulse && !(--rx_led_pulse))
        RX_LED_DISABLED;

    // Disable SOF event while both LEDs are disabled
    if (!tx_led_pulse && !rx_led_pulse)
        USB_Device_DisableSOFEvents();
}

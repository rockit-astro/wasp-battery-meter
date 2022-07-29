//**********************************************************************************
//  Copyright 2017 Paul Chote
//  This file is part of wasp-battery-meter, which is free software. It is made
//  available to you under version 3 (or later) of the GNU General Public License,
//  as published by the Free Software Foundation and included in the LICENSE file.
//**********************************************************************************

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "usb.h"

#define BLINKER_LED_DISABLED PORTC &= ~_BV(PC7)
#define BLINKER_LED_ENABLED  PORTC |= _BV(PC7)
#define BLINKER_LED_INIT     DDRC |= _BV(DDC7), BLINKER_LED_DISABLED

#define OPEN_ENABLED PORTF &= ~_BV(PF0)
#define OPEN_DISABLED  PORTF |= _BV(PF0)
#define OPEN_INIT     DDRF |= _BV(DDF0), OPEN_DISABLED

#define CLOSE_ENABLED PORTF &= ~_BV(PF1)
#define CLOSE_DISABLED  PORTF |= _BV(PF1)
#define CLOSE_INIT     DDRF |= _BV(DDF1), CLOSE_DISABLED

#define LIMIT_OPEN_TRIGGERED bit_is_clear(PINF, PINF6)
#define LIMIT_OPEN_INIT      DDRF &= ~_BV(DDF6), PORTF |= _BV(PF6)

#define LIMIT_CLOSED_TRIGGERED bit_is_clear(PINF, PINF7)
#define LIMIT_CLOSED_INIT      DDRF &= ~_BV(DDF7), PORTF |= _BV(PF7)

#define STATUS_UNKNOWN 0
#define STATUS_CLOSED 1
#define STATUS_OPEN 2
#define STATUS_CLOSING 3
#define STATUS_OPENING 4
#define STATUS_FORCE_CLOSING 5
#define STATUS_FORCE_CLOSED 6

// Open and close timeouts, in half-second increments
#define MAX_OPEN_STEPS 45
#define MAX_CLOSE_STEPS 110

// Number of half-seconds remaining until triggering the force-close
volatile uint8_t heartbeat = 0;

// Sticky status for whether the heartbeat has triggered and is either
// closing or has closed the roof.
volatile bool triggered = false;

volatile uint8_t close_steps_remaining = 0;
volatile uint8_t open_steps_remaining = 0;
volatile uint8_t current_status = STATUS_UNKNOWN;

// Rate limit the status reports to the host PC to 2Hz
volatile bool send_status = false;

// The value recorded by the 8-cycle ADC mean of the ground
const int16_t ground_offset = 1979;

// The relationship between ADC units and volts
const float gain = 0.01712;

volatile int16_t voltage = 0;
volatile bool led_active;
char output[20];

void tick(void)
{
    uint8_t msb, lsb;
    uint16_t sum = 0;

    // Constantly loop reading values from SPI
    // Average 16 values to measure voltage
    for (uint8_t i = 0; i < 16; i++)
    {
        PORTB &= ~_BV(PB0);

        // Read two bytes of data then assemble them into a 16 bit
        // number following Figure 6-1 from the MCP3201 data sheets
        SPDR = 0x00;
        loop_until_bit_is_set(SPSR, SPIF);
        msb = SPDR;

        SPDR = 0x00;
        loop_until_bit_is_set(SPSR, SPIF);
        lsb = SPDR;

        PORTB |= _BV(PB0);

        // Extract 12 bit value following the bit pattern described in
        // Figure 6-1 from the MCP3201 data sheet and add to the average
        sum += (((msb & 0x1F) << 8) | lsb) >> 1;
    }

    // Divide by 16 to complete the average and subtract the zero offset
    // Disable interrupts while updating to ensure data consistency
    cli();
    voltage = (int16_t)(sum >> 4) - ground_offset;
    sei();

    // Check for commands from the host PC
    while (usb_can_read())
    {
        int16_t value = usb_read();
        if (value < 0)
            break;
        
        // Values greater than 0xF0 (240) are reserved for commands
        // Values between 0-240 are treated as heartbeat pings

        // Open roof
        if (value == 0xF1)
        {
            close_steps_remaining = 0;
            open_steps_remaining = MAX_OPEN_STEPS;
        }
        
        // Close roof
        else if (value == 0xF2)
        {
            open_steps_remaining = 0;
            close_steps_remaining = MAX_CLOSE_STEPS;
        }
        
        // Stop roof movement
        else if (value == 0xF3)
        {
            if (open_steps_remaining > 1)
                open_steps_remaining = 1;
            if (close_steps_remaining > 1)
                close_steps_remaining = 1;
        }
            
        // Accept timeouts up to two minutes
        if (value > 240)
            continue;

        // Clear the sticky trigger flag when disabling the heartbeat
        // Also stops an active close
        if (value == 0)
        {
            triggered = false;
            close_steps_remaining = 0;
        }

        // Update the heartbeat countdown (disabling it if 0)
        // If the heatbeat has triggered the status must be manually
        // cleared by sending a 0 byte
        if (!triggered)
            heartbeat = value;
    }

    if (send_status)
    {
        // Send current status back to the host computer
        // Disable interrupts while updating to ensure data consistency
        cli();
        snprintf(output, 20, "%+06.2f,%1d,%02x,%02x,%02x\r\n", voltage * gain, current_status, heartbeat, close_steps_remaining, open_steps_remaining);
        sei();
        usb_write_data(output, 19);
        send_status = false;
    }
}

int main(void)
{
    // Configure timer1 to interrupt every 0.50 seconds
    OCR1A = 7812;
    TCCR1B = _BV(CS12) | _BV(CS10) | _BV(WGM12);
    TIMSK1 |= _BV(OCIE1A);

    BLINKER_LED_INIT;
    OPEN_INIT;
    CLOSE_INIT;
    LIMIT_OPEN_INIT;
    LIMIT_CLOSED_INIT;

    usb_initialize();

    // Set SS, SCK as output
    DDRB = _BV(DDB0) | _BV(DDB1);
 
    // Enable SPI Master @ 250kHz, transmit MSB first
    // Clock idle level is low, sample on falling edge
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR1) | _BV(CPHA);
    sei();
    for (;;)
        tick();
}

ISR(TIMER1_COMPA_vect)
{
    if ((led_active ^= true))
    {
        BLINKER_LED_ENABLED;
    }
    else
        BLINKER_LED_DISABLED;

    // Check whether we need to close the dome
    // This is done inside the ISR to avoid any problems with the USB connection blocking
    // from interfering with the primary job of the device
    //
    // Decrement the heartbeat counter and trigger a close if it reaches 0
    // There are two special values that are not real times:
    //   0xFF represents that the heartbeat has been tripped, and is sticky until the heartbeat is disabled
    //   0x00 represents that the heartbeat is disabled
    if (heartbeat != 0xFF && heartbeat != 0)
    {
        if (--heartbeat == 0)
        {
            triggered = true;
            open_steps_remaining = 0;
            close_steps_remaining = MAX_CLOSE_STEPS;
        }
    }

    uint8_t status = STATUS_UNKNOWN;
    if (LIMIT_CLOSED_TRIGGERED)
    {
        CLOSE_DISABLED;
        status = triggered ? STATUS_FORCE_CLOSED : STATUS_CLOSED;
        close_steps_remaining = 0;
    }
    else if (LIMIT_OPEN_TRIGGERED)
    {
        OPEN_DISABLED;
        status = STATUS_OPEN;
        open_steps_remaining = 0;
    }

    if (close_steps_remaining > 0)
    {
        CLOSE_ENABLED;
        status = triggered ? STATUS_FORCE_CLOSING : STATUS_CLOSING;
        if (--close_steps_remaining == 0)
            CLOSE_DISABLED;
    }
    else if (open_steps_remaining > 0)
    {
        OPEN_ENABLED;
        status = STATUS_OPENING;
        if (--open_steps_remaining == 0)
            OPEN_DISABLED;
    }

    current_status = status;
    send_status = true;
}
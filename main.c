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

// The value recorded by the 8-cycle ADC mean of the ground
const int16_t ground_offset = 1979;

// The relationship between ADC units and volts
const float gain = 0.01712;

volatile int16_t voltage = 0;
volatile bool led_active;
char output[9];

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
}

int main(void)
{
    // Configure timer1 to interrupt every 0.50 seconds
    OCR1A = 7812;
    TCCR1B = _BV(CS12) | _BV(CS10) | _BV(WGM12);
    TIMSK1 |= _BV(OCIE1A);

    BLINKER_LED_INIT;

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

        // Output data once per second
        snprintf(output, 9, "%+06.2f\r\n", voltage * gain);
        usb_write_data(output, 8);
    }
    else
        BLINKER_LED_DISABLED;
}
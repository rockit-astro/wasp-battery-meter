//**********************************************************************************
//  Copyright 2016, 2017 Paul Chote
//  This file is part of wasp-battery-meter, which is free software. It is made
//  available to you under version 3 (or later) of the GNU General Public License,
//  as published by the Free Software Foundation and included in the LICENSE file.
//**********************************************************************************

#include <stdarg.h>
#include <stdint.h>

#ifndef WASP_BATTERY_USB_H
#define WASP_BATTERY_USB_H

void usb_initialize(void);
bool usb_can_read(void);
int16_t usb_read(void);
void usb_write(uint8_t b);
void usb_write_data(void *buf, uint16_t len);
#endif

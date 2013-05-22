/*
*------------------------------------------------------------------------------
* Copyright (C) 2008  Ilya G. Goldberg
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*------------------------------------------------------------------------------
*/

/*------------------------------------------------------------------------------
* Written by:	Ilya G. Goldberg <igg at cathilya dot org>
*------------------------------------------------------------------------------
*/
#ifndef parser_h
#define parser_h

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>

typedef void (*handler_fp) (unsigned char c);
typedef struct {
	PGM_P      name;    // name of the command
	handler_fp handler; // routine to call when processing the cmd
} cmd_entry;
typedef cmd_entry PROGMEM *cmd_entry_P;
void UART0_read_parse (unsigned char c);
void MSR_read (unsigned char c);
int cmp_nam (const void *key0, const void *cmp0);

extern const cmd_entry cmds[] PROGMEM;
// These are indexes into the array
// They are in sort order, and must match those in parser.c
#define AUX_CMD       0
#define DRSN_CMD      1
#define FLM_CUR_CMD   2
#define FLM_MS_CMD    3
#define FLM_TOT_CMD   4
#define ICAL_CMD      5
#define IRAW_CMD      6
#define LCD1_CMD      7
#define LCD2_CMD      8
#define LCDBL_CMD     9
#define LGHT_CMD     10
#define MTN_CMD      11
#define MTN_MS_CMD   12
#define PMP_CMD      13
#define PMP_THR_CMD  14
#define RESET_CMD    15
#define RLY1_CMD     16
#define STK_CMD      17
#define VCAL_CMD     18
#define VIN_CMD      19
#define VIN_THR_CMD  20
#define VRAW_CMD     21

#endif /* parser_h */

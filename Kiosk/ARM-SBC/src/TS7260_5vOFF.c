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
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>


/* Register addresses */
#define POWER_MGMT_PAGE     0x12000000UL
                        
/* Mask for 5v power */
#define PC104_5V_EN         0x08 // Bit 3

int main(int argc, char **argv)
{
int mem_fd;

unsigned char *power_page;   // Unsigned char because offsets are in # of bytes.


	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	/* Open RAM */
	mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
	assert(mem_fd != -1);

	/* Map our GPIO page */
	power_page = (unsigned char *)mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, POWER_MGMT_PAGE);
	assert(power_page != MAP_FAILED);
	
	*power_page &= ~PC104_5V_EN; // un-set the 5v switcher bit

	munmap(power_page, getpagesize());
	close (mem_fd);

	exit(EXIT_SUCCESS);
}

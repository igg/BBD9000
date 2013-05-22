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
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <string.h>


#include "BBD9000mem.h"
// We can't expect that a dereference of an unsigned short * always
// produces a ldrh or strh since the compiler may choose to use
// a byte write instead.  Hence, we emit the peeks and pokes using
// inline assembler.  --JO
static inline unsigned short PEEK16(unsigned long addr) {
        unsigned short ret;

        asm volatile (
                "ldrh %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE16(unsigned long addr, unsigned short dat) {
        asm volatile (
                "strh %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}

static inline unsigned long PEEK32(unsigned long addr) {
        unsigned long ret;

        asm volatile (
                "ldr %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE32(unsigned long addr, unsigned long dat) {
        asm volatile (
                "str %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}


static inline unsigned char PEEK8(unsigned long addr) {
        unsigned char ret;

        asm volatile (
                "ldrb %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE8(unsigned long addr, unsigned char dat) {
        asm volatile (
                "strb %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}

#define TRUE  0x01
#define FALSE 0x00

/*Register addresses */ 
#define CHIP_SELECT_PAGE 0x80840000UL
#define SSP_PAGE         0x808A0000UL

/*Offsets*/
#define SSPCR1           0x04
#define SSPCR1_SSE       0x10
#define SSPCPSR          0x10
#define CHIP_SELECT_DATA 0x30
#define CHIP_SELECT_DDR  0x34
#define SSP_DATA         0x08
#define SSPSR            0x0c
#define SSPSR_BSY        0x10
#define SSPSR_RNE_TFE    0x05

int get_temp (BBD9000mem *shmem, unsigned char *ssp_page, unsigned char *chip_select_page) {
	unsigned long val;
	float temp;
	unsigned char isNegative = FALSE;

	/* 
	 The EP9301 Users Manual says the following algorithm must 
	 be used to configure and enable the SPI bus
	 http://www-s.ti.com/sc/ds/tmp124.pdf
	*/

	/* 1.)	Set enable bit(SSE) in register SSPCR1*/
	POKE32( (unsigned long)(ssp_page + SSPCR1), SSPCR1_SSE );

	/* 1.5) Wait until the SPI is free */
	while ((PEEK32((unsigned long)ssp_page + SSPSR) & SSPSR_BSY)) {
		usleep (10); // wait for unbusy
	}
	while ((PEEK32((unsigned long)ssp_page + SSPSR) & SSPSR_RNE_TFE) != 0x01) {
		usleep (10);   // empty FIFO's
	}
	
	/* 2.)	Write other SSP config registers(SSPCR0 & SSPCPSR)*/
	POKE32( (unsigned long)ssp_page, 0x0F ); /* 16-bit data */
	POKE32( (unsigned long)(ssp_page + SSPCPSR), 0xFE ); /* clock */

	/* 3.)	Clear the enable bit(SSE) in register SSPCR1*/
	POKE32( (unsigned long)(ssp_page + SSPCR1), 0x00 ); 
	
	/* 4.)	Set the enable bit(SSE) in register SSPCR1*/
	POKE32( (unsigned long)(ssp_page + SSPCR1), 0x10 ); 

	/* Done with configuration now lets read the current temp...*/

	//enable the chip select
	POKE32( (unsigned long)(chip_select_page + CHIP_SELECT_DDR), 0x04 ); 
	POKE32( (unsigned long)(chip_select_page + CHIP_SELECT_DATA), 0x00 ); 


	//send read temp command
	POKE32( (unsigned long)(ssp_page + SSP_DATA), 0x8000 ); 
	sleep(1);	

	//disable chip select
	POKE32( (unsigned long)(chip_select_page + CHIP_SELECT_DDR), 0x00 ); 

	//read the temp
	val = PEEK32( (unsigned long)(ssp_page + SSP_DATA) );

	//Lets check if the value is negative
	if( val <= 0xFFFF && val >= 0xE487 )
	{
		//perform two's complement
		val = ~val + 1;
		isNegative = TRUE;

	} else if( val <= 0x4B08 && val >= 0xE486 ) 
	{
		shmem->temp_c = shmem->temp_f = BAD_TEMP;
		return 2;
	}

	if( val >= 0x3E88 && val <= 0x4B07)
	{
		temp = val / 128.046;

	} else if( val >= 0xC88 && val <= 0x3E87 )
	{
		temp = val / 128.056;

	} else if( val >= 0x10 && val <= 0xC87 )
	{
		temp = val / 128.28;
	} else// => val >= 0x00 && val <= 0x0F 
	{
		temp = val / 240;
	} 

	shmem->temp_c = temp;
	//convert the temp to farenheit
	temp = (temp * 1.8) + 32;
	shmem->temp_f = temp;

	return 0;
	
}

int main () {
BBD9000mem *shmem;
int shmem_fd, mem_fd;
unsigned char *chip_select_page, *ssp_page;
struct timeval t_now;


	/* chdir to the root of the filesystem to not block dismounting */
	chdir("/");

	/* open the shared memory object */
	shmem_fd = open(BBD9000MEM, O_RDWR|O_SYNC);
	if (shmem_fd < 0) {
		fprintf (stderr,"Could not open shared POSIX memory segment %s: %s\n",BBD9000MEM, strerror (errno));
		exit (-1);
	}
	
	/* mmap our shared memory */
	shmem = (BBD9000mem *) mmap(0, SHMEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, shmem_fd, 0);
	assert(&shmem != MAP_FAILED);
	
	mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
	assert(mem_fd != -1);
	/* mmap shared memory to talk to the board */
	chip_select_page = mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, CHIP_SELECT_PAGE);
	assert(chip_select_page != MAP_FAILED);
	ssp_page = mmap(0, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, SSP_PAGE);
	assert(ssp_page != MAP_FAILED);

	// Ignoring power signals
	signal (SIGPWRALRM , SIG_IGN);     /* SIGUSR1. Voltage ALRM on BBD9000 SIGPWRALRM */
	signal (SIGPWROK   , SIG_IGN);     /* SIGUSR2. Voltage ALRM RESET on BBD9000 SIGPWROK */


	/* Success */

	/* Initialize our missed_frames counter and our start time */

	while (1) {
		get_temp (shmem, ssp_page, chip_select_page);
		gettimeofday(&t_now, NULL);
		shmem->t_update = t_now;
		usleep (shmem->smartIO_interval);
	}

	munmap(ssp_page, getpagesize());
	munmap(chip_select_page, getpagesize());
	close (mem_fd);

	munmap(shmem, SHMEM_SIZE);
	close (shmem_fd);

	exit(EXIT_SUCCESS);
}


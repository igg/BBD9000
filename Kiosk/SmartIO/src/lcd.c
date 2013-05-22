/*
* Original source:
    A guy named Peter with username danni on AVR Freaks (avrfreaks.net)
  Download original:
    http://www.avrfreaks.net/index.php?name=PNphpBB2&file=download&id=12553
  Original Post:
    http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=496971#496971

  My contribution: A light reformatting
  
  Thanks, danni/Peter (whoever you are).
*/

#include <avr/io.h>
#include <util/delay.h>
#include "lcd.h"

// F_CPU must already be defined!

#define	LCD_D4     SBIT( PORTC, 0 )
#define	LCD_DDR_D4 SBIT( DDRC, 0 )

#define	LCD_D5     SBIT( PORTC, 1 )
#define	LCD_DDR_D5 SBIT( DDRC, 1 )

#define	LCD_D6     SBIT( PORTC, 2 )
#define	LCD_DDR_D6 SBIT( DDRC, 2 )

#define	LCD_D7     SBIT( PORTC, 3 )
#define	LCD_DDR_D7 SBIT( DDRC, 3 )

#define	LCD_RS     SBIT( PORTC, 4 )
#define	LCD_DDR_RS SBIT( DDRC, 4 )

#define	LCD_E0     SBIT( PORTC, 5 )
#define	LCD_DDR_E0 SBIT( DDRC, 5 )




static void lcd_nibble( uint8_t d )
{
	LCD_D4 = 0; if( d & 1<<4 ) LCD_D4 = 1;
	LCD_D5 = 0; if( d & 1<<5 ) LCD_D5 = 1;
	LCD_D6 = 0; if( d & 1<<6 ) LCD_D6 = 1;
	LCD_D7 = 0; if( d & 1<<7 ) LCD_D7 = 1;
	
	LCD_E0 = 1;
	_delay_us( 1 );  // 1us
	LCD_E0 = 0;
}


static void lcd_byte( uint8_t d )
{
	lcd_nibble( d );
	lcd_nibble( d<<4 );
	_delay_us( 45 );			// 45us
}


void lcd_command( uint8_t d )
{
	LCD_RS = 0;
	lcd_byte( d );
	switch( d ){
		case 1:
		case 2:
		case 3: _delay_ms( 2 );		// wait 2ms
	}
}


void lcd_data( uint8_t d )
{
	LCD_RS = 1;
	lcd_byte( d );
}


void lcd_text( char *t )
{

register uint8_t *p = (uint8_t *)t;

register char nch=0;


	while (nch <= LCD_CHARS) {

		if (*p) lcd_data (*p++);

		else lcd_data (' ');

		nch++;

	}


}


void lcd_init( void )
{
	LCD_DDR_D4 = 1;
	LCD_DDR_D5 = 1;
	LCD_DDR_D6 = 1;
	LCD_DDR_D7 = 1;
	LCD_DDR_RS = 1;
	LCD_DDR_E0 = 1;
	
	LCD_E0 = 0;
	LCD_RS = 0;           // send commands
	_delay_ms( 15 );      // wait 15ms
	
	lcd_nibble( 0x30 );
	_delay_ms( 5 );       // wait >4.1ms
	
	lcd_nibble( 0x30 );
	_delay_us( 150 );     // wait >100us
	
	lcd_nibble( 0x30 );   // 8 bit mode
	_delay_us( 150 );     // wait >100us
	
	lcd_nibble( 0x20 );   // 4 bit mode
	_delay_us( 150 );     // wait >100us
	
	lcd_command( 0x28 );  // 2 lines 5*7
	lcd_command( 0x08 );  // display off
	lcd_command( 0x01 );  // display clear
	lcd_command( 0x06 );  // cursor increment
	lcd_command( 0x0C );  // on, no cursor, no blink
}


void lcd_pos( uint8_t line, uint8_t column )
{
	if( line & 1 )
		column += 0x40;
	
	lcd_command( 0x80 + column );
}

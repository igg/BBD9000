/*
  Original source:
    A guy named Peter with username danni on AVR Freaks (avrfreaks.net)
  Download original:
    http://www.avrfreaks.net/index.php?name=PNphpBB2&file=download&id=12553
  Original Post:
    http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&p=496971#496971

  My contribution: A light reformatting
  
  Thanks, danni/Peter (whoever you are).
*/
#include <stdint.h>

#define LCD_LINES 2
#define LCD_CHARS 20

/*
  These aren't used by the LCD library, but may be handy nontheless
*/
#define vu8(x)  (*(volatile uint8_t*)&(x))
#define vs8(x)  (*(volatile int8_t*)&(x))
#define vu16(x) (*(volatile uint16_t*)&(x))
#define vs16(x) (*(volatile int16_t*)&(x))
#define vu32(x) (*(volatile uint32_t*)&(x))
#define vs32(x) (*(volatile int32_t*)&(x))

/*
  This construction makes a nice intuitive bit manipulator
  Allowing to directly assign bit values.
Example:
#define	LED_BIT SBIT( PORTC, 0 )
#define	LED_DDR SBIT( DDRC, 0 )
	LED_DDR = 1;
	LED_BIT = 1;
*/
struct bits {
	uint8_t b0:1;
	uint8_t b1:1;
	uint8_t b2:1;
	uint8_t b3:1;
	uint8_t b4:1;
	uint8_t b5:1;
	uint8_t b6:1;
	uint8_t b7:1;
} __attribute__((__packed__));
#define SBIT(port,pin) ((*(volatile struct bits*)&port).b##pin)


void lcd_command( uint8_t d );
void lcd_data( uint8_t d );
void lcd_text( char *t );
void lcd_init( void );
void lcd_pos( uint8_t line, uint8_t column );

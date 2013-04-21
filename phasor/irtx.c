#include <avr/io.h>

#include "config.h"
#include <util.h>

#include "irtx.h"

#define IRTX_USART_TXPIN_bm G4_CONCAT(PIN,IRTX_USART_TXPIN,_bm)

#define _PIN(id) G4_CONCAT3(PIN,id,_bm)
#define _PINCTRL(id) G4_CONCAT3(PIN,id,CTRL)
#define _TXPIN_bm _PIN(IRTX_USART_TXPIN)
#define _TXPINCTRL _PINCTRL(IRTX_USART_TXPIN)
inline void irtx_init() {
	
	IRTX_USART_PORT.OUTCLR = _TXPIN_bm;
	IRTX_USART_PORT.DIRSET = _TXPIN_bm;
	
	IRTX_USART_PORT._TXPINCTRL |= PORT_INVEN_bm;

	IRTX_USART.BAUDCTRLA = (uint8_t)(IRTX_USART_BSEL&0x00FF);
	IRTX_USART.BAUDCTRLB = (IRTX_USART_BSCALE<<USART_BSCALE_gp) | (uint8_t)( (IRTX_USART_BSEL>>8) & 0x0F );

	IRTX_USART.CTRLA = 0;
	IRTX_USART.CTRLB = USART_TXEN_bm;
	IRTX_USART.CTRLC = USART_PMODE_DISABLED_gc | USART_CHSIZE_8BIT_gc;


}

inline void irtx_byte(uint8_t byte) {
	IRTX_USART.DATA = byte;
}

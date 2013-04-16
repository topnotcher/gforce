#ifndef PORTS_H
#define PORTS_H

#ifndef _BV
#define _BV(x) (1<<x)
#endif


//used below for utility.
#define PORT_CONCAT(a,b) a ## b
#define PORT_CONCAT3(a,b,c) a##b##c

//used to create "DDRD/B" etc from DDR, D
#define DDR_ID(id) PORT_CONCAT(PORT, id).DIR
#define PORT_ID(id) PORT_CONCAT(PORT, id).OUT
#define PIN_ID(id) PORT_CONCAT(PORT,id).IN

//e.g. PIN_HIGH(PORTB, 0)
#define PIN_HIGH(port,pin) port |= _BV(pin)
#define PIN_LOW(port,pin) port &= ~_BV(pin)
#define PIN_SET(port,pin,value) port = (value) ? ((port)|_BV(pin)) : ((port)&~_BV(pin))
#define PIN_GET(port,pin) (port&_BV(pin))



#endif

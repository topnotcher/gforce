#ifndef XMEGA_UTIL_H
#define XMEGA_UTIL_H

#define G4_CONCAT(a, b) a ## b
#define G4_CONCAT3(a, b, c) a##b##c

#define G4_PIN(id) G4_CONCAT3(PIN, id, _bm)
#define G4_PINCTRL(id) G4_CONCAT3(PIN, id, CTRL)

#endif

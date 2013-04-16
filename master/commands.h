#include <stdlib.h>
#include <stdint.h>

#ifndef COMMANDS_H
#define COMMANDS_H

#define N_COMMANDS 256

typedef struct {
	uint8_t cmd;
	uint8_t packs;
	uint8_t wtfsthis;
	uint8_t crc;
} command_t;


typedef struct {
	void ( *cb)(command_t *);
	uint8_t args;
} command_handler_t;

void cmd_register(uint8_t cmd, uint8_t args, void (*cb)(command_t *));

uint8_t cmd_dispatch(uint8_t, command_t *);
uint8_t cmd_n_args(uint8_t);
/*$_SESSION['end']['code'] = array( 0xff, 0x08, 0x7f, 0x99, 'crc' );
$_SESSION['end']['code'][2] -= $_SESSION['start']['packs'] & 0x03;


$b = array();
$b[0] = 0xff; // break
$b[1] = 0x38; // game start command
//0x7f = 0b1111111, 0x03 = 0b11
//0 => all  = 0b1111111
//1 => blu  = 0b1111110
//2 => Grn  = 0b1111101
//3 => Red  = 0b1111100
$b[2] = 0x7f - ( $_SESSION['start']['packs'] & 0x03 );
$b[3] = 0x8a; // passkey 1
$b[4] = 'crc'; // crc 1
$b[5] =  0x53; // passkey 2
$b[6] =  0x00; // game number


$packs = array(
    0 => 'All',
    1 => 'Blu',
    2 => 'Grn',
    3 => 'Red'
);
*/
#endif

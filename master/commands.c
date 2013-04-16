#include "commands.h"

command_handler_t command_table[N_COMMANDS] = {0};


inline void cmd_register(uint8_t cmd, uint8_t args, void (*cb)(command_t *)) {
	command_table[cmd].cb = cb;
	command_table[cmd].args = args;
}

inline uint8_t cmd_dispatch(uint8_t cmd, command_t * data) {
	if ( command_table[cmd].cb != NULL ) {
		command_table[cmd].cb(data);
		return 1;
	}

	return 0;
}

inline uint8_t cmd_n_args(uint8_t cmd) {
	return ( command_table[cmd].cb != NULL ) ? command_table[cmd].args : 0; 
}

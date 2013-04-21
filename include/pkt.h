#include <stdint.h>

#ifndef PKT_H
#define PKT_H

typedef struct {

	/**
	 * command?
	 */
	uint8_t cmd;


	/**
	 * Length of data
	 */
	uint8_t len;

	uint8_t saddr;

	/**
	 * data
	 */
	uint8_t * data;

	/**
	 * CRC of all data.
	 */
	uint8_t chksum;
} pkt_hdr;

typedef struct {
	/**
	 * recv timeout. when we receive data
	 * and we're expecting more data, this counter 
	 * is incremented by some amount. It is decremented by
	 * a timer. When recv_time reaches 0, a recv timeout occurs and the
	 * buffer is cleared.
	 */
	uint8_t recv_time; 


	/** 
	 * # of bytes received thus far. 
	 */
	uint8_t size;

	/** 
	 * running crc of the packet
	 */
	uint8_t crc;

	/** 
	 * packet being received
	 */
	pkt_hdr pkt; 
} pkt_proc;


#endif

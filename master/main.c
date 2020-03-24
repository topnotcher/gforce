#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <sam.h>

#include <g4config.h>
#include <mpc.h>
/* #include <mpcphasor.h> */
#include <mpctwi.h>

/* #include "sounds.h" */
#include "xbee.h"
#include "game.h"
#include "debug.h"
#include "mpcipc.h"

#include <drivers/sam/isr.h>
#include <drivers/sam/twi/slave.h>
#include <drivers/sam/twi/master.h>
#include <drivers/sam/led_spi.h>
#include <drivers/sam/port.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"

#ifndef DEBUG
#define DEBUG 1
#endif

const int LED_SERCOM = 4;
const int MPC_MASTER_SERCOM = 1;
const int MPC_SLAVE_SERCOM = 3;

// Power switch state
static bool power_on = false;

// FreeRTOS hanndlers in port.c
extern void vPortSVCHandler(void) __attribute__(( naked ));
extern void xPortSysTickHandler(void) __attribute__(( naked ));
extern void xPortPendSVHandler(void) __attribute__(( naked ));

void init_task(void *_params);

/* static void xbee_relay_mpc(const mpc_pkt *const pkt); */

static void configure_pack_on_interrupt(void) {
	// enable EIC bus clock
	MCLK->APBAMASK.reg |= MCLK_APBAMASK_EIC;

	// Reset the EIC
	EIC->CTRLA.reg = EIC_CTRLA_SWRST;
	while (EIC->SYNCBUSY.reg & EIC_SYNCBUSY_SWRST);

	// Select ULP32K oscillator (enabled by deafult)
	EIC->CTRLA.reg |= EIC_CTRLA_CKSEL;

	// Configure rising edge detection for EXTINT5
	EIC->CONFIG[0].reg &= ~EIC_CONFIG_SENSE5_Msk;
	EIC->CONFIG[0].reg |= EIC_CONFIG_SENSE5(EIC_CONFIG_SENSE5_RISE_Val);
	EIC->ASYNCH.reg |= EIC_ASYNCH_ASYNCH(1 << 5);

	// Enable EIC
	EIC->CTRLA.reg |= EIC_CTRLA_ENABLE;
	while (EIC->SYNCBUSY.reg & EIC_SYNCBUSY_ENABLE);

	// Enable the IRQ for PACK ON.
	nvic_register_isr(EIC_5_IRQn, NVIC_SystemReset);
	NVIC_EnableIRQ(EIC_5_IRQn);

	// Enable interrupt for EXTINT5
	EIC->INTENSET.reg = EIC_INTENSET_EXTINT(1 << 5);
}

int main(void) {
	configure_clocks();

	// Register the FreeRTOS ISRs.
	nvic_register_isr(SVCall_IRQn, vPortSVCHandler);
	nvic_register_isr(PendSV_IRQn, xPortPendSVHandler);
	nvic_register_isr(SysTick_IRQn, xPortSysTickHandler);

	// Critical sections in FreeRTOS work by masking interrupt priorities. By
	// default, all interrupts are priority 0 (highest), which is not maskable.
	// Thus, if we enable IRQs without setting a priorioty, there will be no
	// critical sections. Work around this by setting all IRQs to a priority
	// that FreeRTOS will mask.
	for (int i = -14; i < PERIPH_COUNT_IRQn; ++i) {
		// Except for SVC, which is used to start the scheduler.
		if (i != SVCall_IRQn)
			NVIC_SetPriority(i, configMAX_SYSCALL_INTERRUPT_PRIORITY);
	}


	xTaskCreate(init_task, "init", 128, NULL, tskIDLE_PRIORITY + 1, NULL);
	vTaskStartScheduler();
}

void init_task(void *_params) {
	configure_pinmux();

	// Set PB05 (pack on switch input) to input
	pin_set_input(PIN_PACK_ON);
	pin_set_config(PIN_PACK_ON, PORT_PINCFG_INEN);

	// set ~power off signal to output and set high. There is a bug in the
	// board that prevents setting this low.
	pin_set_output(PIN_PACK_POWER_OFF);
	pin_set(PIN_PACK_POWER_OFF, true);

	power_on = pin_get(PIN_PACK_ON);

	if (!power_on) {
		// if the pack is off, configure an interrupt for when it is turned on.
		configure_pack_on_interrupt();
	} else {
		led_init();
		mpc_init();
		xbee_init();
		game_init();
		/* sound_init(); */
	}

	//ping hack: master receives a ping reply
	//send it to the xbee.
	/* mpc_register_cmd(MPC_CMD_DIAG_RELAY, xbee_relay_mpc); */

	//relay data for debugging
	/* mpc_register_cmd(MPC_CMD_DIAG_DEBUG, xbee_relay_mpc); */

	// memory usage reply...
	/* mpc_register_cmd(MPC_CMD_DIAG_MEM_USAGE, xbee_relay_mpc); */

	while (1) {
		vTaskSuspend(NULL);
	}
}

void mpc_register_drivers(void) {
	const uint8_t twi_tx_mask = MPC_ADDR_RS | MPC_ADDR_LS | MPC_ADDR_CHEST;
	twi_master_t *twim = twi_master_init(MPC_MASTER_SERCOM, 14);
	twi_slave_t *twis = twi_slave_init(MPC_SLAVE_SERCOM, MPC_ADDR_BOARD, twi_tx_mask);

	mpc_register_driver(mpctwi_init(twim, twis, MPC_ADDR_BOARD, twi_tx_mask));
	mpc_register_driver(mpc_loopback_init(MPC_ADDR_BOARD, MPC_ADDR_BACK));

	/* mpc_register_driver(mpc_phasor_init()); */
}

/* static void xbee_relay_mpc(const mpc_pkt *const pkt) { */
/* 	xbee_send_pkt(pkt); */
/* } */



led_spi_dev *led_init_driver(void) {
	pin_set_output(PIN_LED_CONTROLLER_OFF);
	pin_set(PIN_LED_CONTROLLER_OFF, false);

	return led_spi_init(LED_SERCOM, 0, 1);
}

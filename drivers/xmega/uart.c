#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <malloc.h>
#include <drivers/xmega/uart.h>
#include <drivers/xmega/dma.h>

// This only applies to XMEGA A series... And not the a*b models...
#if defined(USARTF1)
#define USART_NUM_INST 8
#elif defined(USARTF0)
#define USART_NUM_INST 7
#elif defined(USARTE0)
#define USART_NUM_INST 5
#else
#error "Unable to determine number of USART modules!"
#endif

#define USARTC0_INST_IDX 0
#define USARTC1_INST_IDX 1
#define USARTD0_INST_IDX 2
#define USARTD1_INST_IDX 3
#define USARTE0_INST_IDX 4
#define USARTE1_INST_IDX 5
#define USARTF0_INST_IDX 6
#define USARTF1_INST_IDX 7

struct _uart_dev_t {
	QueueHandle_t rx_queue;
	QueueHandle_t tx_queue;

	USART_t *uart;
	DMA_CH_t *dma;

	// only touchy from interrupt context.
	struct _uart_tx_vec tx;
	uint8_t tx_pos;
};

static struct {
	void (*vectors[UART_NUM_VEC])(void *);
	void *ins;
} usart_vector_table[USART_NUM_INST];

static void uart_tx_interrupt_disable(const uart_dev_t *) __attribute__((nonnull(1)));
static void uart_tx_interrupt_enable(const uart_dev_t *) __attribute__((nonnull(1)));
static void uart_dma_interrupt_disable(const uart_dev_t *) __attribute((nonnull(1)));
static void uart_dma_interrupt_enable(const uart_dev_t *) __attribute((nonnull(1)));

static bool uart_init_rx(uart_dev_t *, uart_config_t *) __attribute__((nonnull(1, 2)));
static bool uart_init_tx(uart_dev_t *, uart_config_t *) __attribute__((nonnull(1, 2)));
static void uart_config_chsize(uart_config_t *) __attribute__((nonnull(1)));
static void uart_config_stop_bits(uart_config_t *) __attribute__((nonnull(1)));
static void uart_config_parity(uart_config_t *) __attribute__((nonnull(1)));

static inline void usart_handler(uint8_t, uint8_t) __attribute__((always_inline));

static void uart_rx_isr(void *);
static void uart_tx_isr(void *);
static void uart_dma_tx_isr(void *);

uart_dev_t *uart_init(USART_t *uart_hw, uart_config_t *const config) {
	uart_dev_t *uart;
	bool initialized = false;

	config->port_info = uart_port_info(uart_hw);
	config->ctrla = 0;
	config->ctrlb = 0;
	config->ctrlc = 0;

	if ((uart = smalloc(sizeof *uart))) {
		memset(uart, 0, sizeof(*uart));

		uart->uart = uart_hw;

		if (config->rx_queue_size)
			initialized |= uart_init_rx(uart, config);

		if (config->tx_queue_size)
			initialized |= uart_init_tx(uart, config);

		if (initialized) {
			uart->tx_pos = 0;
			uart->tx.len = 0;

			config->bits = 8;
			uart_config_chsize(config);
			uart_config_stop_bits(config);
			uart_config_parity(config);

			uart_hw->BAUDCTRLA = (uint8_t)(config->bsel & 0x00FF);
			uart_hw->BAUDCTRLB = (config->bscale << USART_BSCALE_gp) | (uint8_t)((config->bsel >> 8) & 0x0F);

			uart_hw->CTRLC = config->ctrlc;
			uart_hw->CTRLA = config->ctrla;
			uart_hw->CTRLB = config->ctrlb;
		}
	}

	return uart;
}

void uart_config_default(uart_config_t *const config) {
	memset(config, 0, sizeof(*config));

	config->enable_tx_dma = false;
	config->bits = 8;
	config->parity = UART_PARITY_NONE;
	config->stop_bits = 1;
}

static void uart_config_chsize(uart_config_t *config) {
	switch (config->bits) {
	case 5:
		config->ctrlc |= USART_CHSIZE_5BIT_gc;
		break;

	case 6:
		config->ctrlc |= USART_CHSIZE_6BIT_gc;
		break;

	case 7:
		config->ctrlc |= USART_CHSIZE_7BIT_gc;
		break;

	case 9:
		config->ctrlc |= USART_CHSIZE_9BIT_gc;
		break;

	case 8:
	default:
		config->ctrlc = USART_CHSIZE_8BIT_gc;
		break;
	}
}

static void uart_config_stop_bits(uart_config_t *config) {
	if (config->stop_bits == 2)
		config->ctrlc |= USART_SBMODE_bm;
}

static void uart_config_parity(uart_config_t *config) {
	switch (config->parity) {
	case UART_PARITY_EVEN:
		config->ctrlc |= USART_PMODE_EVEN_gc;
		break;

	case UART_PARITY_ODD:
		config->ctrlc |= USART_PMODE_ODD_gc;
		break;
	default:
	case UART_PARITY_NONE:
		config->ctrlc |= USART_PMODE_DISABLED_gc;
		break;
	}
}

static bool uart_init_rx(uart_dev_t *const uart, uart_config_t *const config) {
	int8_t usart_index = uart_get_index(uart->uart);

	if (usart_index >= 0) {
		uint8_t size = (config->bits == 9) ? sizeof(uint16_t) : sizeof(uint8_t);
		uart->rx_queue = xQueueCreate(config->rx_queue_size, size);

		uart_register_handler(usart_index, UART_RXC_VEC, uart_rx_isr, uart);

		config->ctrla |= USART_RXCINTLVL_MED_gc;
		config->ctrlb |= USART_RXEN_bm;

		// Manual, page 237.
		config->port_info.port->DIRCLR = 1 << config->port_info.rx_pin;

		return true;
	} else {
		return false;
	}
}

static bool uart_init_tx(uart_dev_t *const uart, uart_config_t *const config) {
	bool success = false;

	if (config->enable_tx_dma) {
		uart->dma = dma_channel_alloc();
		int8_t chan_index = dma_get_channel_index(uart->dma);

		if (uart->dma != NULL && chan_index >= 0) {
			uart->dma->ADDRCTRL =
				DMA_CH_DESTRELOAD_TRANSACTION_gc |
				DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTDIR_FIXED_gc;

				uart->dma->TRIGSRC = usart_dma_get_trigsrc(uart->uart);
				uart->dma->CTRLA = DMA_CH_SINGLE_bm;

				dma_chan_set_destaddr(uart->dma, (void*)&uart->uart->DATA);
				dma_register_handler(chan_index, uart_dma_tx_isr, uart);

			success = true;
		}
	} else {
		int8_t usart_index = uart_get_index(uart->uart);

		if (usart_index >= 0) {
			uart_register_handler(usart_index, UART_DRE_VEC, uart_tx_isr, uart);
			success = true;
		}
	}

	if (success) {
		// TODO: implement 9 bit mode!
		uart->tx_queue = xQueueCreate(config->tx_queue_size, sizeof(struct _uart_tx_vec));
		config->ctrlb |= USART_TXEN_bm;

		// Manual, page 237.
		config->port_info.port->OUTSET = 1 << config->port_info.tx_pin;
		config->port_info.port->DIRSET = 1 << config->port_info.tx_pin;
	}

	return success;
}

uint8_t usart_dma_get_trigsrc(const USART_t *const usart) {
#ifdef USARTC0
	if (usart == &USARTC0)
		return DMA_CH_TRIGSRC_USARTC0_DRE_gc;
#endif
#ifdef USARTC1
	if (usart == &USARTC1)
		return DMA_CH_TRIGSRC_USARTC1_DRE_gc;
#endif
#ifdef USARTD0
	if (usart == &USARTD0)
		return DMA_CH_TRIGSRC_USARTD0_DRE_gc;
#endif
#ifdef USARTD1
	if (usart == &USARTD1)
		return DMA_CH_TRIGSRC_USARTD1_DRE_gc;
#endif
#ifdef USARTE0
	if (usart == &USARTE0)
		return DMA_CH_TRIGSRC_USARTE0_DRE_gc;
#endif
#ifdef USARTE1
	if (usart == &USARTE1)
		return DMA_CH_TRIGSRC_USARTE1_DRE_gc;
#endif
#ifdef USARTF0
	if (usart == &USARTF0)
		return DMA_CH_TRIGSRC_USARTF0_DRE_gc;
#endif
#ifdef USARTF1
	if (usart == &USARTF1)
		return DMA_CH_TRIGSRC_USARTF1_DRE_gc;
#endif
		return 0;
}


uart_port_desc uart_port_info(const USART_t *const uart) {
	uart_port_desc desc;

#ifdef USARTC0
	if (uart == &USARTC0) {
		desc.port = &PORTC;
		desc.tx_pin = 3;
		desc.rx_pin = 2;
		desc.xck_pin = 1;
	}
#endif
#ifdef USARTC1
	if (uart == &USARTC1) {
		desc.port = &PORTC;
		desc.tx_pin = 7;
		desc.rx_pin = 6;
		desc.xck_pin = 5;
	}
#endif
#ifdef USARTD0
	if (uart == &USARTD0) {
		desc.port = &PORTD;
		desc.tx_pin = 3;
		desc.rx_pin = 2;
		desc.xck_pin = 1;
	}
#endif
#ifdef USARTD1
	if (uart == &USARTD1) {
		desc.port = &PORTD;
		desc.tx_pin = 7;
		desc.rx_pin = 6;
		desc.xck_pin = 5;
	}
#endif
#ifdef USARTE0
	if (uart == &USARTE0) {
		desc.port = &PORTE;
		desc.tx_pin = 3;
		desc.rx_pin = 2;
		desc.xck_pin = 1;
	}
#endif
#ifdef USARTE1
	if (uart == &USARTE1) {
		desc.port = &PORTE;
		desc.tx_pin = 7;
		desc.rx_pin = 6;
		desc.xck_pin = 5;
	}
#endif
#ifdef USARTF0
	if (uart == &USARTF0) {
		desc.port = &PORTF;
		desc.tx_pin = 3;
		desc.rx_pin = 2;
		desc.xck_pin = 1;
	}
#endif
#ifdef USARTF1
	if (uart == &USARTF1) {
		desc.port = &PORTF;
		desc.tx_pin = 7;
		desc.rx_pin = 6;
		desc.xck_pin = 5;
	}
#endif

	return desc;
}

// This is only valid for regular xmega A variants (not e.g. a*b)
int8_t uart_get_index(const USART_t *const uart) {
	int8_t idx = -1;

#ifdef USARTC0
	if (uart == &USARTC0) {
		idx = USARTC0_INST_IDX;
	}
#endif
#ifdef USARTC1
	if (uart == &USARTC1) {
		idx = USARTC1_INST_IDX;
	}
#endif
#ifdef USARTD0
	if (uart == &USARTD0) {
		idx = USARTD0_INST_IDX;
	}
#endif
#ifdef USARTD1
	if (uart == &USARTD1) {
		idx = USARTD1_INST_IDX;
	}
#endif
#ifdef USARTE0
	if (uart == &USARTE0) {
		idx = USARTE0_INST_IDX;
	}
#endif
#ifdef USARTE1
	if (uart == &USARTE1) {
		idx = USARTE1_INST_IDX;
	}
#endif
#ifdef USARTF0
	if (uart == &USARTF0) {
		idx = USARTF0_INST_IDX;
	}
#endif
#ifdef USARTF1
	if (uart == &USARTF1) {
		idx = USARTF1_INST_IDX;
	}
#endif

	return idx;
}

void uart_write(uart_dev_t *const dev, const uint8_t *const data, const uint8_t len, void (*complete)(void *buf)) {
	struct _uart_tx_vec vec = {
		.buf = data,
		.len = len,
		.complete = complete
	};

	if (dev->dma) {
		bool queued;

		if (!(queued = xQueueSend(dev->tx_queue, &vec, 0)))
			complete((void*)data);

		vTaskSuspendAll();
		uart_dma_interrupt_disable(dev);

		// if dev->tx_pos is set, the next transfer will be polled from the
		// Queue in the TX complete interrupt. If it's not set, there's no
		// ongoing transfer, so one should be started now.
		// This is a bit stupid - we queue and then immediately dequeue the data.
		if (queued && !dev->tx_pos && xQueueReceive(dev->tx_queue, &dev->tx, 0)) {
			dev->tx_pos = 1;
			dma_chan_set_srcaddr(dev->dma, (void *)dev->tx.buf);
			dma_chan_set_transfer_count(dev->dma, dev->tx.len);

			dma_chan_enable(dev->dma);
		}

		uart_dma_interrupt_enable(dev);
		xTaskResumeAll();
	} else {
		if (xQueueSend(dev->tx_queue, &vec, 0))
			uart_tx_interrupt_enable(dev);
		else
			complete((void*)data);
	}
}

uint8_t uart_getchar(const uart_dev_t *const dev) {
	uint8_t data;
	xQueueReceive(dev->rx_queue, &data, portMAX_DELAY);

	return data;
}

static void uart_send_byte(uart_dev_t *const dev) {
	dev->uart->DATA = dev->tx.buf[dev->tx_pos++];

	if (dev->tx_pos == dev->tx.len && dev->tx.complete)
		dev->tx.complete((void*)dev->tx.buf);
}

static void uart_tx_isr(void *const _dev) {
	uart_dev_t *const dev = _dev;

	if (dev->tx_pos < dev->tx.len) {
		uart_send_byte(dev);

	} else if (xQueueReceiveFromISR(dev->tx_queue, &dev->tx, NULL)) {
		dev->tx_pos = 0;
		uart_send_byte(dev);

	// this actually triggers the DRE interrupt an extra time
	} else {
		uart_tx_interrupt_disable(dev);
	}
}

static void uart_dma_tx_isr(void *const _dev) {
	uart_dev_t *const dev = _dev;
	dev->dma->CTRLB |= DMA_CH_TRNIF_bm;

	dev->tx.complete((void*)dev->tx.buf);

	if (xQueueReceiveFromISR(dev->tx_queue, &dev->tx, NULL)) {
		dma_chan_set_srcaddr(dev->dma, (void *)dev->tx.buf);
		dma_chan_set_transfer_count(dev->dma, dev->tx.len);

		dma_chan_enable(dev->dma);

	} else {
		dev->tx_pos = 0;
	}
}

void uart_register_handler(uint8_t uart_index, enum uart_vector vector, void (*handler)(void *), void *ins) {
	usart_vector_table[uart_index].vectors[vector] = handler;
	usart_vector_table[uart_index].ins = ins;
}

static void uart_rx_isr(void *const _dev) {
	uart_dev_t *const dev = _dev;

	uint8_t data = dev->uart->DATA;
	xQueueSendFromISR(dev->rx_queue, &data, NULL);
}

static void uart_dma_interrupt_disable(const uart_dev_t *const dev) {
	dev->dma->CTRLB &= ~DMA_CH_TRNINTLVL_LO_gc;
}

static void uart_dma_interrupt_enable(const uart_dev_t *const dev) {
	dev->dma->CTRLB |= DMA_CH_TRNINTLVL_LO_gc;
}

static void uart_tx_interrupt_disable(const uart_dev_t *const dev) {
	dev->uart->CTRLA &= ~USART_DREINTLVL_MED_gc;
}

static void uart_tx_interrupt_enable(const uart_dev_t *const dev) {
	dev->uart->CTRLA |= USART_DREINTLVL_MED_gc;
}

static inline void usart_handler(uint8_t usart_index, uint8_t vector) {
	usart_vector_table[usart_index].vectors[vector](usart_vector_table[usart_index].ins);
}

#define CONCAT(a, b, c) a ## b ## c
#define USART_HANDLERS(x) \
void __attribute__((signal)) USART ## x ## _RXC_vect(void) { \
	usart_handler(CONCAT(USART, x, _INST_IDX), UART_RXC_VEC); \
} \
void __attribute__((signal)) USART ## x ## _TXC_vect(void) { \
	usart_handler(CONCAT(USART, x, _INST_IDX), UART_TXC_VEC); \
} \
void __attribute__((signal)) USART ## x ## _DRE_vect(void) { \
	usart_handler(CONCAT(USART, x, _INST_IDX), UART_DRE_VEC); \
} \

#ifdef USARTC0
USART_HANDLERS(C0)
#endif

#ifdef USARTC1
USART_HANDLERS(C1)
#endif

#ifdef USARTD0
USART_HANDLERS(D0)
#endif

#ifdef USARTD1
USART_HANDLERS(D1)
#endif

#ifdef USARTE0
USART_HANDLERS(E0)
#endif

#ifdef USARTE1
USART_HANDLERS(E1)
#endif

#ifdef USARTF0
USART_HANDLERS(F0)
#endif

#ifdef USARTF1
USART_HANDLERS(F1)
#endif

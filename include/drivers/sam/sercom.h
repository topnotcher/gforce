#include <sam.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#ifndef SERCOM_H
#define SERCOM_H

#define from_sercom(var, ptr, member) (typeof(*(var))*)(\
	((typeof(ptr))(0) == (typeof((var)->member)*)(0)) ? \
	(void*)(ptr - offsetof(typeof(*(var)), member)) : \
	NULL \
)

#if defined(__SAMD51N20A__)
	#define SERCOM_IRQ_BASE SERCOM0_0_IRQn
	#define SERCOM_IRQ_NUM 4
	#define SERCOM_INT_NUM 8
#elif defined(__SAML21E17B__)
	#define SERCOM_IRQ_BASE SERCOM0_IRQn
	#define SERCOM_IRQ_NUM 1
	#define SERCOM_INT_NUM 8
#else
	#error "Part not supported!"
#endif

struct _sercom_s;
typedef struct _sercom_s {
	Sercom *hw;
	void (*isr[SERCOM_INT_NUM])(struct _sercom_s *);
	int index;
} sercom_t;

bool sercom_init(const int, sercom_t *const);

int sercom_get_index(const Sercom *);
void sercom_register_handler(sercom_t *, uint8_t, void (*)(sercom_t *));
void sercom_enable_irq(const sercom_t *const, uint8_t int_num) __attribute__((nonnull(1)));
void sercom_disable_irq(const sercom_t *const, uint8_t int_num) __attribute__((nonnull(1)));
void sercom_enable_pm(const sercom_t *const) __attribute__((nonnull(1)));
void sercom_set_gclk_core(const sercom_t *const, int) __attribute__((nonnull(1)));
void sercom_set_gclk_slow(const sercom_t *const, int) __attribute__((nonnull(1)));

int sercom_dma_rx_trigsrc(const sercom_t *const) __attribute__((nonnull(1)));
int sercom_dma_tx_trigsrc(const sercom_t *const) __attribute__((nonnull(1)));
#endif

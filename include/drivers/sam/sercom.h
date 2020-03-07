#include <sam.h>
#include <stddef.h>

#ifndef SERCOM_H
#define SERCOM_H

// prevents Wstrict-aliasing and Wcast-align (if difference is in the macro)
static inline void* _from_sercom(void *ptr, ptrdiff_t offset, int _) {
	return (uint8_t*)ptr - offset;
}

#define from_sercom(var, ptr, member) \
	(typeof(*var)*)_from_sercom(ptr, offsetof(typeof(*(var)), member), (typeof(ptr))(0) == (typeof((var)->member))(0))

struct _sercom_s;
typedef struct _sercom_s {
	Sercom *hw;
	void (*isr)(struct _sercom_s *);
	int index;
} sercom_t;

sercom_t *sercom_init(const int);

int sercom_get_index(const Sercom *);
void sercom_register_handler(sercom_t *, void (*)(sercom_t *));
void sercom_enable_irq(const sercom_t *const) __attribute__((nonnull(1)));
void sercom_disable_irq(const sercom_t *const) __attribute__((nonnull(1)));
void sercom_enable_pm(const sercom_t *const) __attribute__((nonnull(1)));
void sercom_set_gclk_core(const sercom_t *const, int) __attribute__((nonnull(1)));
void sercom_set_gclk_slow(const sercom_t *const, int) __attribute__((nonnull(1)));

int sercom_dma_rx_trigsrc(const sercom_t *const) __attribute__((nonnull(1)));
int sercom_dma_tx_trigsrc(const sercom_t *const) __attribute__((nonnull(1)));
#endif

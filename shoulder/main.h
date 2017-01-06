#ifndef MAIN_H
#define MAIN_H

#define bsp_init_func(fn) void fn(void)
#define bsp_init_stub(fn) bsp_init_func(fn) __attribute__((weak, alias("_nopfn")))

extern bsp_init_func(_nopfn);

/**
 * Initialize system block, low level board hardware, etc.
 */
extern bsp_init_func(system_board_init);

/**
 * Configure the interrupt controller. Leave interrutps disabled - they will be
 * enabled when a task starts.
 */
extern bsp_init_func(system_configure_interrupts);

/**
 * Configure the OS timer interrupt and associated clock source.
 */
extern bsp_init_func(system_configure_os_ticks);

#endif

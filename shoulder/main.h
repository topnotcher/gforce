#ifndef MAIN_H
#define MAIN_H

#define system_init_func(fn) void fn(void)
#define system_init_stub(fn) system_init_func(fn) __attribute__((weak, alias("_nopfn")))

system_init_func(_nopfn);

/**
 * Initialize system block, low level board hardware, etc.
 */
system_init_func(system_board_init);

/**
 * Configure the interrupt controller. Leave interrutps disabled - they will be
 * enabled when a task starts.
 */
system_init_func(system_configure_interrupts);

/**
 * Initialize software components - called immediately before starting the scheduler.
 */
system_init_func(system_software_init);

#endif

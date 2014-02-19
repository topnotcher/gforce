#ifndef CHARGER_H
#define CHARGER_H

/* Parts of this borrowed:
 * https://android.googlesource.com/kernel/tegra/+/android-tegra-3.10%5E/include/linux/power/bq2419x-charger.h*/

#define BQ24193_INPUT_SRC_REG           0x00
#define BQ24193_PWR_ON_REG              0x01
#define BQ24193_CHRG_CTRL_REG           0x02
#define BQ24193_CHRG_TERM_REG           0x03
#define BQ24193_VOLT_CTRL_REG           0x04
#define BQ24193_TIME_CTRL_REG           0x05
#define BQ24193_THERM_REG               0x06
#define BQ24193_MISC_OPER_REG           0x07
#define BQ24193_SYS_STAT_REG            0x08
#define BQ24193_FAULT_REG               0x09
#define BQ24193_REVISION_REG            0x0a



void charger_init(void);

#endif

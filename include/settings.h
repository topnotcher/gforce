#ifndef SETTINGS_H
#define SETTINGS_H
enum pack_setting {
	SETTING_ACTIVE_COLOR,
	SETTING_BRIGHTNESS,
	SETTING_SHOT_RATE,
};

enum pack_setting_type {
	SETTING_UINT8,
	SETTING_UINT16,
	SETTING_UINT32,
};

void settings_init(void);
void register_setting(enum pack_setting, enum pack_setting_type, void (*)(const void *));
#endif

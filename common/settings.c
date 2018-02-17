#include <stdlib.h>
#include <string.h>

#include <mpc.h>

#include "settings.h"
#include "malloc.h"

struct setting_node_s;
typedef struct setting_node_s {
	enum pack_setting_type setting_type;
	enum pack_setting setting;
	void (* set_callback)(const void *);
	struct setting_node_s *next;
} setting_node;

static setting_node *head;

static void process_settings(const mpc_pkt *constpkt);
static void process_setting(const uint8_t **const, const uint8_t *const);
uint8_t get_setting_size(const enum pack_setting_type);

void settings_init(void) {
	mpc_register_cmd(MPC_CMD_CONFIG, process_settings);
}

void register_setting(enum pack_setting setting, enum pack_setting_type setting_type, void (*set_callback)(const void *)) {
	setting_node *node = smalloc(sizeof *node);

	if (node) {
		node->setting_type = setting_type;	
		node->setting = setting;
		node->set_callback = set_callback;

		node->next = head;
		head = node;
	}
}

static void process_settings(const mpc_pkt *const pkt) {
	const uint8_t *ptr = (const uint8_t*)pkt->data;
	const uint8_t *const end = (uint8_t*)pkt->data + pkt->len - 1;

	while (ptr < end)
		process_setting(&ptr, end);
}

static void process_setting(const uint8_t **const spec, const uint8_t *const end) {
	// already parsed beyond end of message.
	if (*spec >= end)
		return;

	// maximum size of a setting.
	uint8_t setting_value[4] __attribute__((aligned));

	enum pack_setting setting = (**(uint8_t**)spec) & 0xFF;
	(*spec)++;

	for (setting_node *cur = head; cur; cur = cur->next) {
		if (cur->setting == setting) {
			uint8_t setting_size = get_setting_size(cur->setting_type);
			
			if ((*spec) + setting_size - 1 <= end && cur->set_callback != NULL) {
				memcpy(setting_value, *spec, setting_size);
				cur->set_callback((const void *)setting_value);
			}

			*spec += setting_size;
		}
	}
}

uint8_t get_setting_size(const enum pack_setting_type setting_type) {

	switch (setting_type) {
	case SETTING_UINT8:
		return 1;
	case SETTING_UINT16:
		return 2;
	case SETTING_UINT32:
		return 4;
	default:
		return 255;
	}
}

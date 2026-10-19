/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrfx.h>

#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ficr_mac_print, CONFIG_LOG_DEFAULT_LEVEL);

#define MACADDR_BLANK 0xFFFFFFFFu
#define MACADDR_SLOTS 2

static bool mac_slot_empty(uint32_t low, uint32_t high)
{
	return (low == MACADDR_BLANK) || (high == MACADDR_BLANK);
}

static void log_mac_slot(const char *block, unsigned int slot, uint32_t low, uint32_t high)
{
	if (mac_slot_empty(low, high)) {
		LOG_INF("%s MACADDR[%u]: not programmed", block, slot);
		return;
	}

	LOG_INF("%s MACADDR[%u]: %02X:%02X:%02X:%02X:%02X:%02X",
		block, slot,
		(uint8_t)(high >> 16), (uint8_t)(high >> 8), (uint8_t)high,
		(uint8_t)(low >> 16), (uint8_t)(low >> 8), (uint8_t)low);
}

static int ficr_mac_print_init(void)
{
	for (unsigned int slot = 0; slot < MACADDR_SLOTS; slot++) {
		log_mac_slot("FICR", slot,
			     NRF_FICR->MACADDR[slot].LOW,
			     NRF_FICR->MACADDR[slot].HIGH);
		log_mac_slot("UICR", slot,
			     NRF_UICR->MACADDR[slot].LOW,
			     NRF_UICR->MACADDR[slot].HIGH);
	}

	return 0;
}

BUILD_ASSERT(92 > CONFIG_SHELL_BACKEND_SERIAL_INIT_PRIORITY,
	     "ficr_mac_print must run after the shell UART backend init");
SYS_INIT(ficr_mac_print_init, POST_KERNEL, 92);

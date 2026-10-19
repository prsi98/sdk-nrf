/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <nrfx.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <vtf_monitoring/vtf_monitoring.h>

LOG_MODULE_REGISTER(xo_vtf, CONFIG_VTF_LOG_LEVEL);

#define XTALCOMP_BLANK 0xFFFFFFFFu

static int32_t xtalcomp_offset_read(void)
{
	uint32_t raw = NRF_FICR->XTALCOMP.OFFSET;

	if (raw == XTALCOMP_BLANK) {
		raw = NRF_UICR->XTALCOMP.OFFSET;
	}

	return (raw == XTALCOMP_BLANK) ? 0 : (int32_t)raw;
}

static int xo_vtf_init(void)
{
	int32_t offset = xtalcomp_offset_read();

	vtf_snapshots[VTF_CH_FREQ_OFFSET].i32 = offset;
	LOG_INF("XO offset from XTALCOMP: %d ppm", offset);

	return 0;
}

static int xo_vtf_sample(struct vtf_sample *out)
{
	out->type = VTF_SAMPLE_TYPE_INT;
	out->value.i32 = xtalcomp_offset_read();
	out->timestamp_ms = k_uptime_get();
	out->status = VTF_STATUS_OK;

	return 0;
}

VTF_CHANNEL_DEFINE(xo_vtf_channel, VTF_CH_FREQ_OFFSET, xo_vtf_sample, xo_vtf_init,
		   VTF_SAMPLE_TYPE_INT, i32, CONFIG_VTF_FREQ_OFFSET_DEFAULT_VALUE);

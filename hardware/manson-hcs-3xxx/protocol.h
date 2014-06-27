/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef LIBSIGROK_HARDWARE_MANSON_HCS_3XXX_PROTOCOL_H
#define LIBSIGROK_HARDWARE_MANSON_HCS_3XXX_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

#define LOG_PREFIX "manson-hcs-3xxx"

enum {
	MANSON_HCS_3200,
	MANSON_HCS_3202,
	MANSON_HCS_3204,
};

struct hcs_model {
	int model_id;
	char *name;
	double voltage[3]; /* Min, max, step */
	double current[3]; /* Min, max, step */
};

/** Private, per-device-instance driver context. */
struct dev_context {
	struct hcs_model *model;

	uint64_t limit_samples;
	uint64_t limit_msec;
	uint64_t num_samples;
	int64_t starttime;
	int64_t req_sent_at;
	gboolean reply_pending;

	void *cb_data;

	float voltage;
	float current;
	gboolean cc_mode;

	char buf[50];
	int buflen;
};

SR_PRIV int hcs_receive_data(int fd, int revents, void *cb_data);

#endif
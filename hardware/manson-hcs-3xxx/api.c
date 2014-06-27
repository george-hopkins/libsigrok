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

#include "protocol.h"

static const int32_t hwopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const int32_t devopts[] = {
	SR_CONF_POWER_SUPPLY,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_LIMIT_MSEC,
	SR_CONF_CONTINUOUS,
	SR_CONF_OUTPUT_VOLTAGE,
	SR_CONF_OUTPUT_CURRENT,
};

/* Note: All models have one power supply output only. */
static struct hcs_model models[] = {
	{ MANSON_HCS_3200, "HCS-3200", { 1, 18, 0.1 }, { 0, 20, 0.01 } },
	{ MANSON_HCS_3202, "HCS-3202", { 1, 36, 0.1 }, { 0, 10, 0.01 } },
	{ MANSON_HCS_3204, "HCS-3204", { 1, 60, 0.1 }, { 0,  5, 0.01 } },
	{ 0, NULL, { 0, 0, 0 }, { 0, 0, 0 }, },
};

SR_PRIV struct sr_dev_driver manson_hcs_3xxx_driver_info;
static struct sr_dev_driver *di = &manson_hcs_3xxx_driver_info;

static int dev_clear(void)
{
	return std_dev_clear(di, NULL);
}

static int init(struct sr_context *sr_ctx)
{
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *scan(GSList *options)
{
	int i, model_id;
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_config *src;
	struct sr_channel *ch;
	GSList *devices, *l;
	const char *conn, *serialcomm;
	struct sr_serial_dev_inst *serial;
	char reply[50], **tokens;

	drvc = di->priv;
	drvc->instances = NULL;
	devices = NULL;
	conn = NULL;
	serialcomm = NULL;

	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		default:
			sr_err("Unknown option %d, skipping.", src->key);
			break;
		}
	}

	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = "9600/8n1";

	if (!(serial = sr_serial_dev_inst_new(conn, serialcomm)))
		return NULL;

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	serial_flush(serial);

	sr_info("Probing serial port %s.", conn);

	/* Get the device model. */
	memset(&reply, 0, sizeof(reply));
	serial_write_blocking(serial, "GMOD\r", 5);
	serial_read_blocking(serial, &reply, 8);
	tokens = g_strsplit((const gchar *)&reply, "\r", 2);

	model_id = -1;
	for (i = 0; models[i].name != NULL; i++) {
		if (!strcmp(models[i].name + 4, tokens[0]))
			model_id = i;
	}
	if (model_id < 0) {
		sr_err("Unknown model '%s' detected, aborting.", tokens[0]);
		return NULL;
	}

	if (!(sdi = sr_dev_inst_new(0, SR_ST_INACTIVE, "Manson",
				    models[model_id].name, NULL))) {
		sr_err("Failed to create device instance.");
		return NULL;
	}

	g_strfreev(tokens);

	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->driver = di;

	if (!(ch = sr_channel_new(0, SR_CHANNEL_ANALOG, TRUE, "CH1"))) {
		sr_err("Failed to create channel.");
		return NULL;
	}
	sdi->channels = g_slist_append(sdi->channels, ch);

	devc = g_malloc0(sizeof(struct dev_context));
	devc->model = &models[model_id];

	sdi->priv = devc;

	drvc->instances = g_slist_append(drvc->instances, sdi);
	devices = g_slist_append(devices, sdi);

	serial_close(serial);
	if (!devices)
		sr_serial_dev_inst_free(serial);

	return devices;
}

static GSList *dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static int cleanup(void)
{
	return dev_clear();
}

static int config_get(int key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
	case SR_CONF_OUTPUT_VOLTAGE:
		*data = g_variant_new_double(devc->voltage);
		break;
	case SR_CONF_OUTPUT_CURRENT:
		*data = g_variant_new_double(devc->current);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(int key, GVariant *data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
		if (g_variant_get_uint64(data) == 0)
			return SR_ERR_ARG;
		devc->limit_msec = g_variant_get_uint64(data);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		if (g_variant_get_uint64(data) == 0)
			return SR_ERR_ARG;
		devc->limit_samples = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	(void)sdi;
	(void)cg;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
			hwopts, ARRAY_SIZE(hwopts), sizeof(int32_t));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
			devopts, ARRAY_SIZE(devopts), sizeof(int32_t));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;
	devc->cb_data = cb_data;

	/* Send header packet to the session bus. */
	std_session_send_df_header(cb_data, LOG_PREFIX);

	devc->starttime = g_get_monotonic_time();
	devc->num_samples = 0;
	devc->reply_pending = FALSE;
	devc->req_sent_at = 0;

	/* Poll every 10ms, or whenever some data comes in. */
	serial = sdi->conn;
	serial_source_add(serial, G_IO_IN, 10, hcs_receive_data, (void *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	return std_serial_dev_acquisition_stop(sdi, cb_data,
			std_serial_dev_close, sdi->conn, LOG_PREFIX);
}

SR_PRIV struct sr_dev_driver manson_hcs_3xxx_driver_info = {
	.name = "manson-hcs-3xxx",
	.longname = "Manson HCS-3xxx",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
/*
 * This file is part of the sigrok project.
 *
 * Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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

#include <stdlib.h>
#include <string.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"
#include "protocol.h"

/* Note: The order here must match the DMM/device enum in protocol.h. */
static const char *dev_names[] = {
	"UNI-T UT61D",
	"Voltcraft VC-820",
};

static const int hwcaps[] = {
	SR_HWCAP_MULTIMETER,
	SR_HWCAP_LIMIT_SAMPLES,
	SR_HWCAP_LIMIT_MSEC,
	SR_HWCAP_CONTINUOUS,
	0,
};

static const char *probe_names[] = {
	"Probe",
	NULL,
};

SR_PRIV struct sr_dev_driver uni_t_ut61d_driver_info;
static struct sr_dev_driver *di_ut61d = &uni_t_ut61d_driver_info;

SR_PRIV struct sr_dev_driver voltcraft_vc820_driver_info;
static struct sr_dev_driver *di_vc820 = &voltcraft_vc820_driver_info;

static int open_usb(struct sr_dev_inst *sdi)
{
	libusb_device **devlist;
	struct libusb_device_descriptor des;
	struct dev_context *devc;
	int ret, tmp, cnt, i;

	/* TODO: Use common code later, refactor. */

	devc = sdi->priv;

	if ((cnt = libusb_get_device_list(NULL, &devlist)) < 0) {
		sr_err("Error getting USB device list: %d.", cnt);
		return SR_ERR;
	}

	ret = SR_ERR;
	for (i = 0; i < cnt; i++) {
		if ((tmp = libusb_get_device_descriptor(devlist[i], &des))) {
			sr_err("Failed to get device descriptor: %d.", tmp);
			continue;
		}

		if (libusb_get_bus_number(devlist[i]) != devc->usb->bus
			|| libusb_get_device_address(devlist[i]) != devc->usb->address)
			continue;

		if ((tmp = libusb_open(devlist[i], &devc->usb->devhdl))) {
			sr_err("Failed to open device: %d.", tmp);
			break;
		}

		sr_info("Opened USB device on %d.%d.",
			devc->usb->bus, devc->usb->address);
		ret = SR_OK;
		break;
	}
	libusb_free_device_list(devlist, 1);

	return ret;
}

static GSList *connect_usb(const char *conn, int dmm)
{
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_probe *probe;
	libusb_device **devlist;
	struct libusb_device_descriptor des;
	GSList *devices;
	int vid, pid, devcnt, err, i;

	(void)conn;

	/* TODO: Use common code later, refactor. */

	if (dmm == UNI_T_UT61D)
		drvc = di_ut61d->priv;
	else if (dmm == VOLTCRAFT_VC820)
		drvc = di_vc820->priv;

	/* Hardcoded for now. */
	vid = UT_D04_CABLE_USB_VID;
	pid = UT_D04_CABLE_USB_DID;

	devices = NULL;
	libusb_get_device_list(NULL, &devlist);
	for (i = 0; devlist[i]; i++) {
		if ((err = libusb_get_device_descriptor(devlist[i], &des))) {
			sr_err("Failed to get device descriptor: %d", err);
			continue;
		}

		if (des.idVendor != vid || des.idProduct != pid)
			continue;

		if (!(devc = g_try_malloc0(sizeof(struct dev_context)))) {
			sr_err("Device context malloc failed.");
			return NULL;
		}

		devcnt = g_slist_length(drvc->instances);
		if (!(sdi = sr_dev_inst_new(devcnt, SR_ST_INACTIVE,
					    dev_names[dmm], NULL, NULL))) {
			sr_err("sr_dev_inst_new returned NULL.");
			return NULL;
		}
		sdi->priv = devc;
		if (!(probe = sr_probe_new(0, SR_PROBE_ANALOG, TRUE, "P1")))
			return NULL;
		sdi->probes = g_slist_append(sdi->probes, probe);
		devc->usb = sr_usb_dev_inst_new(
				libusb_get_bus_number(devlist[i]),
				libusb_get_device_address(devlist[i]), NULL);
		devices = g_slist_append(devices, sdi);
	}
	libusb_free_device_list(devlist, 1);

	return devices;
}

static int clear_instances(void)
{
	/* TODO: Use common code later. */

	return SR_OK;
}

static int hw_init(int dmm)
{
	int ret;
	struct drv_context *drvc;

	if (!(drvc = g_try_malloc0(sizeof(struct drv_context)))) {
		sr_err("Driver context malloc failed.");
		return SR_ERR_MALLOC;
	}

	if ((ret = libusb_init(NULL)) < 0) {
		sr_err("Failed to initialize libusb: %s.",
		       libusb_error_name(ret));
		return SR_ERR;
	}

	if (dmm == UNI_T_UT61D)
		di_ut61d->priv = drvc;
	else if (dmm == VOLTCRAFT_VC820)
		di_vc820->priv = drvc;

	return SR_OK;
}

static int hw_init_ut61d(void)
{
	return hw_init(UNI_T_UT61D);
}

static int hw_init_vc820(void)
{
	return hw_init(VOLTCRAFT_VC820);
}

static GSList *hw_scan(GSList *options, int dmm)
{
	GSList *l, *devices;
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;

	(void)options;

	if (dmm == UNI_T_UT61D)
		drvc = di_ut61d->priv;
	else if (dmm == VOLTCRAFT_VC820)
		drvc = di_vc820->priv;

	if (!(devices = connect_usb(NULL, dmm)))
		return NULL;

	for (l = devices; l; l = l->next) {
		sdi = l->data;

		if (dmm == UNI_T_UT61D)
			sdi->driver = di_ut61d;
		else if (dmm == VOLTCRAFT_VC820)
			sdi->driver = di_vc820;

		drvc->instances = g_slist_append(drvc->instances, l->data);
	}

	return devices;
}

static GSList *hw_scan_ut61d(GSList *options)
{
	return hw_scan(options, UNI_T_UT61D);
}

static GSList *hw_scan_vc820(GSList *options)
{
	return hw_scan(options, VOLTCRAFT_VC820);
}

static GSList *hw_dev_list(int dmm)
{
	struct drv_context *drvc;

	if (dmm == UNI_T_UT61D)
		drvc = di_ut61d->priv;
	else if (dmm == VOLTCRAFT_VC820)
		drvc = di_vc820->priv;

	return drvc->instances;
}

static GSList *hw_dev_list_ut61d(void)
{
	return hw_dev_list(UNI_T_UT61D);
}

static GSList *hw_dev_list_vc820(void)
{
	return hw_dev_list(VOLTCRAFT_VC820);
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
	return open_usb(sdi);
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO */

	return SR_OK;
}

static int hw_cleanup(void)
{
	clear_instances();

	// libusb_exit(NULL);

	return SR_OK;
}

static int hw_info_get(int info_id, const void **data,
		       const struct sr_dev_inst *sdi)
{
	(void)sdi;

	sr_spew("Backend requested info_id %d.", info_id);

	switch (info_id) {
	case SR_DI_HWCAPS:
		*data = hwcaps;
		sr_spew("%s: Returning hwcaps.", __func__);
		break;
	case SR_DI_NUM_PROBES:
		*data = GINT_TO_POINTER(1);
		sr_spew("%s: Returning number of probes.", __func__);
		break;
	case SR_DI_PROBE_NAMES:
		*data = probe_names;
		sr_spew("%s: Returning probe names.", __func__);
		break;
	case SR_DI_SAMPLERATES:
		/* TODO: Get rid of this. */
		*data = NULL;
		sr_spew("%s: Returning samplerates.", __func__);
		return SR_ERR_ARG;
		break;
	case SR_DI_CUR_SAMPLERATE:
		/* TODO: Get rid of this. */
		*data = NULL;
		sr_spew("%s: Returning current samplerate.", __func__);
		return SR_ERR_ARG;
		break;
	default:
		sr_err("%s: Unknown info_id %d.", __func__, info_id);
		return SR_ERR_ARG;
		break;
	}

	return SR_OK;
}

static int hw_dev_config_set(const struct sr_dev_inst *sdi, int hwcap,
			     const void *value)
{
	struct dev_context *devc;

	devc = sdi->priv;

	switch (hwcap) {
	case SR_HWCAP_LIMIT_MSEC:
		/* TODO: Not yet implemented. */
		if (*(const uint64_t *)value == 0) {
			sr_err("Time limit cannot be 0.");
			return SR_ERR;
		}
		devc->limit_msec = *(const uint64_t *)value;
		sr_dbg("Setting time limit to %" PRIu64 "ms.",
		       devc->limit_msec);
		break;
	case SR_HWCAP_LIMIT_SAMPLES:
		if (*(const uint64_t *)value == 0) {
			sr_err("Sample limit cannot be 0.");
			return SR_ERR;
		}
		devc->limit_samples = *(const uint64_t *)value;
		sr_dbg("Setting sample limit to %" PRIu64 ".",
		       devc->limit_samples);
		break;
	default:
		sr_err("Unknown capability: %d.", hwcap);
		return SR_ERR;
		break;
	}

	return SR_OK;
}

static int hw_dev_acquisition_start(const struct sr_dev_inst *sdi,
				    int dmm, void *cb_data)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_header header;
	struct sr_datafeed_meta_analog meta;
	struct dev_context *devc;

	devc = sdi->priv;

	sr_dbg("Starting acquisition.");

	devc->cb_data = cb_data;

	/* Send header packet to the session bus. */
	sr_dbg("Sending SR_DF_HEADER.");
	packet.type = SR_DF_HEADER;
	packet.payload = (uint8_t *)&header;
	header.feed_version = 1;
	gettimeofday(&header.starttime, NULL);
	sr_session_send(devc->cb_data, &packet);

	/* Send metadata about the SR_DF_ANALOG packets to come. */
	sr_dbg("Sending SR_DF_META_ANALOG.");
	packet.type = SR_DF_META_ANALOG;
	packet.payload = &meta;
	meta.num_probes = 1;
	sr_session_send(devc->cb_data, &packet);

	if (dmm == UNI_T_UT61D) {
		sr_source_add(0, 0, 10 /* poll_timeout */,
			      uni_t_ut61d_receive_data, (void *)sdi);
	} else if (dmm == VOLTCRAFT_VC820) {
		sr_source_add(0, 0, 10 /* poll_timeout */,
			      voltcraft_vc820_receive_data, (void *)sdi);
	}

	return SR_OK;
}

static int hw_dev_acquisition_start_ut61d(const struct sr_dev_inst *sdi,
					  void *cb_data)
{
	return hw_dev_acquisition_start(sdi, UNI_T_UT61D, cb_data);
}

static int hw_dev_acquisition_start_vc820(const struct sr_dev_inst *sdi,
					  void *cb_data)
{
	return hw_dev_acquisition_start(sdi, VOLTCRAFT_VC820, cb_data);
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi,
				   void *cb_data)
{
	struct sr_datafeed_packet packet;

	(void)sdi;

	sr_dbg("Stopping acquisition.");

	/* Send end packet to the session bus. */
	sr_dbg("Sending SR_DF_END.");
	packet.type = SR_DF_END;
	sr_session_send(cb_data, &packet);

	/* TODO? */
	sr_source_remove(0);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver uni_t_ut61d_driver_info = {
	.name = "uni-t-ut61d",
	.longname = "UNI-T UT61D",
	.api_version = 1,
	.init = hw_init_ut61d,
	.cleanup = hw_cleanup,
	.scan = hw_scan_ut61d,
	.dev_list = hw_dev_list_ut61d,
	.dev_clear = clear_instances,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
	.info_get = hw_info_get,
	.dev_config_set = hw_dev_config_set,
	.dev_acquisition_start = hw_dev_acquisition_start_ut61d,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};

SR_PRIV struct sr_dev_driver voltcraft_vc820_driver_info = {
	.name = "voltcraft-vc820",
	.longname = "Voltcraft VC-820",
	.api_version = 1,
	.init = hw_init_vc820,
	.cleanup = hw_cleanup,
	.scan = hw_scan_vc820,
	.dev_list = hw_dev_list_vc820,
	.dev_clear = clear_instances,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
	.info_get = hw_info_get,
	.dev_config_set = hw_dev_config_set,
	.dev_acquisition_start = hw_dev_acquisition_start_vc820,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};

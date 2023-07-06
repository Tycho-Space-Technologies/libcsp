

#include <csp/interfaces/csp_if_gen.h>
#include <csp/csp_buffer.h>
#include <csp/csp_id.h>
#include <zmq.h>

#include <stddef.h>
#include <sys/types.h>

#include <assert.h>

static size_t header_size(void) {
	if (csp_conf.version == 2) {
		return 6;
	} else {
		return 4;
	}
}

static int gen_nexthop(csp_iface_t * iface, uint16_t via, csp_packet_t * packet,
					   int from_me) {
	struct csp_if_gen_data_s * ifdata = iface->interface_data;

	if (csp_queue_enqueue(ifdata->queue, packet, 0) != CSP_QUEUE_OK) {
		return CSP_ERR_NOMEM;
	}

	csp_buffer_free(packet);

	return CSP_ERR_NONE;
}

/* TODO: via */
static ssize_t send_packet_(csp_packet_t * packet, csp_if_gen_driver_t * driver) {
	csp_id_prepend(packet);

	return driver->write(driver, packet->frame_begin, packet->frame_length);
}

static int handle_incoming_(void * buf, size_t size, csp_iface_t * iface) {
	if (size < header_size()) {
		return CSP_ERR_INVAL;
	}

	/* The size of the id is subtracted from frame_length when calling
	 * csp_id_strip, which is why we dont need to include it in the data
	 * field. */
	csp_packet_t * packet = csp_buffer_get(size - header_size());
	if (packet == NULL) {
		return CSP_ERR_NOBUFS;
	}

	csp_id_setup_rx(packet);
	memcpy(packet->frame_begin, buf, size);
	packet->frame_length = size;

	if (csp_id_strip(packet) != 0) {
		csp_buffer_free(packet);
		return CSP_ERR_INVAL;
	}

	csp_qfifo_write(packet, iface, NULL);

	return CSP_ERR_NONE;
}

/* Worker thread that listens for incoming messages and adds them to the CSP
 * queue.
 */
void * csp_if_gen_task(void * param) {
	csp_iface_t * iface = param;
	csp_if_gen_data_t * ifdata = iface->interface_data;
	csp_if_gen_driver_t * driver = iface->driver_data;

	for (;;) {
		/* Packets ready in the TX queue, send to connected peer */
		if (csp_queue_dequeue(ifdata->queue, ifdata->buf, 0) == CSP_QUEUE_OK) {
			ssize_t status = send_packet_(ifdata->buf, driver);

			if (status < 0) {
				csp_print("Unable to send packet (error %i)\n", -status);
			}
		}

		/* Incoming packets, add them to CSP system */
		size_t recv_size = driver->read(driver, ifdata->buf, csp_buffer_size());
		if (recv_size > 0) {
			if (handle_incoming_(ifdata->buf, recv_size, iface) != CSP_ERR_NONE) {
				iface->rx_error++;
			}
		}
	}

	return NULL;
}

int csp_if_gen_init(const char * name, csp_iface_t * iface,
					csp_if_gen_data_t * data,
					csp_if_gen_driver_t * driver) {
	if (driver == NULL) {
		return CSP_ERR_INVAL;
	}

	*iface = (csp_iface_t){
		.driver_data = driver,
		.interface_data = data,
		.name = name,
		.nexthop = gen_nexthop,
	};

	csp_iflist_add(iface);

	return CSP_ERR_NONE;
}

void csp_if_gen_deinit(csp_iface_t * iface) {
	csp_if_gen_driver_t * driver = iface->driver_data;
	if (driver != NULL) {
		driver->cleanup(driver);
	}
	csp_iflist_remove(iface);
}
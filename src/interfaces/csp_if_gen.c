

#include <csp/interfaces/csp_if_gen.h>
#include <csp/csp_buffer.h>
#include <zmq.h>

#include <stddef.h>
#include <sys/types.h>

#include <assert.h>

static size_t pck_size(csp_packet_t * packet) {
	return CSP_BUFFER_PACKET_OVERHEAD + packet->length;
}

/* TODO: via */
static int gen_nexthop(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {
	struct csp_if_gen_data_s * ifdata = iface->interface_data;

	if (csp_queue_enqueue(ifdata->queue, packet, 0) != CSP_QUEUE_OK) {
		return CSP_ERR_NOMEM;
	}

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
		if (csp_queue_dequeue(ifdata->queue, &ifdata->packet_buffer, 0) == CSP_QUEUE_OK) {
			size_t size = pck_size(&ifdata->packet_buffer);
			ssize_t status = driver->write(driver, &ifdata->packet_buffer, size);

			if (status < 0) {
				csp_print("Unable to send packet (error %i)\n", -status);
			}
		}

		/* Incoming packets, add them to CSP system */
		size_t recv_size = driver->read(driver, &ifdata->packet_buffer, csp_buffer_size());
		if (recv_size > 0) {
			csp_packet_t * copy = csp_buffer_clone(&ifdata->packet_buffer);

			if (copy == NULL) {
				csp_print("Unable to clone incoming packet\n");
			} else {
				csp_qfifo_write(copy, iface, NULL);
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
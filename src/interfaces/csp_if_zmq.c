

#include <csp/csp.h>
#include <zmq.h>

#include <csp/arch/csp_queue.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>

#include <assert.h>

extern csp_conf_t csp_conf;

struct zmq_driver {
	ssize_t (*write)(struct zmq_driver *, void * src, size_t size);
	ssize_t (*read)(struct zmq_driver *, void * dst, size_t size);
	void (*cleanup)(struct zmq_driver *);

	void * data;
};

struct iface_data {
	csp_queue_handle_t queue;
	pthread_t worker;

	uint8_t * buffer;
};

void * zmq_worker(void *);

static int iface_nexthop_(const csp_route_t * ifroute, csp_packet_t * packet) {
	struct iface_data * ifdata = ifroute->iface->interface_data;

	if (csp_queue_enqueue(ifdata->queue, packet, 0) != CSP_QUEUE_OK) {
		return CSP_ERR_NOMEM;
	}

	return CSP_ERR_NONE;
}

int csp_zmq_init(struct zmq_driver * driver, csp_iface_t * iface) {
	if (driver == NULL) {
		return CSP_ERR_INVAL;
	}

	*iface = (csp_iface_t){
		.driver_data = driver,
		.name = "zmq_ipc",
		.nexthop = iface_nexthop_,
	};

	pthread_create(&driver->worker, NULL, zmq_worker, iface);

	return CSP_ERR_NONE;
}

/* Worker thread that listens for incoming messages and adds them to the CSP
 * queue.
 */
void * zmq_worker(void * param) {
	(void)param;

	csp_iface_t * iface = param;
	struct iface_data * ifdata = iface->interface_data;
	struct zmq_driver * driver = iface->driver_data;

	for (;;) {
		/* Packets ready in the TX queue, send to connected peer */
		if (csp_queue_dequeue(ifdata->queue, ifdata->buffer, 0) == CSP_QUEUE_OK) {
			if (driver->write(driver, ifdata->buffer,
							  csp_buffer_size()) != 0) {
				csp_log_error("Unable to send\n");
			}
		}

		/* Incoming packets, add them to CSP system */
		size_t recv_size = driver->read(driver, ifdata->buffer, csp_buffer_size());
		if (recv_size > 0) {
			csp_packet_t * copy = csp_buffer_clone(ifdata->buffer);
			if (copy == NULL) {
				csp_log_error("Unable to clone incoming packet\n");
			} else {
				csp_qfifo_write(copy, iface, NULL);
			}
		}
	}

	return NULL;
}

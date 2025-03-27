#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <nng/nng.h>

#include "csp/csp.h"
#include "csp/csp_endian.h"
#include "csp/csp_interface.h"
#include "csp/interfaces/csp_if_nng.h"

/**
 * @brief Send function called by CSP stack
 *
 * @param route
 * @param packet
 * @return int
 */

static int send(const csp_route_t *ifroute, csp_packet_t *packet)
{
	csp_iface_t * interface = ifroute->iface;
	if (interface == NULL || interface->driver_data == NULL)
		return CSP_ERR_DRIVER;

	// Pointer to socket is attached as CSP interface driver
	nng_socket *socket = (nng_socket *)interface->driver_data;

	/* Convert header from host to network order. */
	packet->id.ext = csp_hton32(packet->id.ext);

	if (nng_send(*socket, (void *)&(packet->id),
		     sizeof(csp_id_t) + packet->length, 0) != 0) {
		csp_log_warn("Failed to send NNG message");
		return CSP_ERR_DRIVER;
	}

	csp_buffer_free(packet);

	return CSP_ERR_NONE;
}

/**
 * @brief NNG receiver thread
 *
 * @param interface handle for the identification
 * @param socket to read messages from
 */
static void rx_thread(csp_iface_t *interface)
{
	nng_socket *socket = (nng_socket *)interface->driver_data;
	while (true) {
		int rv;
		char *buf = NULL;
		size_t full_size = 0;

		if ((rv = nng_recv(*socket, &buf, &full_size,
				   NNG_FLAG_ALLOC)) != 0) {
			if (rv == NNG_ETIMEDOUT) {
				csp_log_warn("Failed to receive NNG message");
			}
			sleep(10);
		} else {
			if (full_size < sizeof(csp_id_t)) {
				csp_log_warn("Invalid NNG message size");
			} else {
				size_t data_size = full_size - sizeof(csp_id_t);
				csp_packet_t *packet =
					csp_buffer_get(data_size);
				if (packet != NULL) {
					// Copy header
					memcpy(&packet->id, buf,
					       sizeof(csp_id_t));

					// Copy data
					memcpy((void *)&packet->data[0],
					       (void *)(buf + sizeof(csp_id_t)),
					       data_size);

					/* Convert packet header from network to host order. */
					packet->id.ext = csp_ntoh32(packet->id.ext);

					// Forward to the CSP stack
					packet->length = data_size;
					csp_qfifo_write(packet, interface, NULL);
				}
			}
			nng_free(buf, full_size);
		}
	}
}

void csp_nng_init(csp_iface_t *csp_interface, nng_socket *nng_sock,
		  uint16_t mtu, const char *name)
{
	// Set pointer to the socket as CSP interface driver that send function
	// could know where to send data
	csp_interface->driver_data = (void *)nng_sock;
	csp_interface->mtu = mtu;
	csp_interface->nexthop = send;
	csp_interface->name = name;

	// Create RX thread
	pthread_t rx_handle;
	pthread_create(&rx_handle, NULL, (void *)rx_thread, csp_interface);

	// Register interface
	csp_iflist_add(csp_interface);
}

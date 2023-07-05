#pragma once

#include <csp/csp.h>
#include <csp/arch/csp_queue.h>
#include <pthread.h>
#include <sys/types.h>

/**
 * @brief Generic driver. This needs to be created by the user
 */
typedef struct csp_if_generic_driver_s {
	ssize_t (*write)(struct csp_if_generic_driver_s *, void * src, size_t size);
	ssize_t (*read)(struct csp_if_generic_driver_s *, void * dst, size_t size);
	void (*cleanup)(struct csp_if_generic_driver_s *);

	void * data;
} csp_if_generic_driver_t;

/**
 * @brief Generic interface data. This only requires allocaton of its members, which
 * needs to be done by the user
 */
typedef struct csp_if_generic_data_s {
	csp_queue_handle_t queue;
	pthread_t worker;

	uint8_t * buffer;
} csp_if_generic_data_t;

/**
 * @brief Initialize a generic interface @p iface with name @p name. @p data
 * and @p driver need to be created and initialized by the user.
 *
 * @param name
 * @param iface
 * @param data
 * @param driver
 * @return int CSP_ERR_NONE on success, CSP error code otherwise
 */
int csp_if_generic_init(const char * name, csp_iface_t * iface,
						csp_if_generic_data_t * data,
						csp_if_generic_driver_t * driver);

#ifndef _CSP_IF_NNG_H_
#define _CSP_IF_NNG_H_

/**
 * @brief Initialize CSP interface for the NNG socket, start receiver thread and register interface to the CSP stack
 *
 * @param csp_interface
 * @param nng_sock - already initalized NNG socket
 * @param mtu
 * @param name is a unique name of the interface
 */
void csp_nng_init(csp_iface_t *csp_interface, nng_socket *nng_sock,
		  uint16_t mtu, const char *name);

#endif /* _CSP_IF_NNG_H_ */

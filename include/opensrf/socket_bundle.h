#ifndef SOCKET_BUNDLE_H
#define SOCKET_BUNDLE_H

/**
	@file socket_bundle.h
	@brief Header for socket routines.

	These routines cover most of the low-level tedium involved in the use of sockets.

	They support UDP, TCP, and UNIX domain sockets, for both clients and servers.  That's six
	different combinations.  They also support the spawning of sockets from a listener
	by calls to accept(),

	Support for UPD is nominal at best.  UDP sockets normally involve the use of calls to
	recvfrom() or sendto(), but neither of those functions appears here.  In practice the
	functions for opening UDP sockets are completely unused at this writing.

	All socket traffic is expected to consist of text; i.e. binary data is not supported.
*/

#include <opensrf/utils.h>
#include <opensrf/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>


//---------------------------------------------------------------
// Unix headers
//---------------------------------------------------------------
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* models a single socket connection */
struct socket_node_struct;
typedef struct socket_node_struct socket_node;


/* Maintains the socket set */
/**
	@brief Manages a collection of sockets.
*/
struct socket_manager_struct {
	/** @brief Callback for passing any received data up to the calling code.
	Parameters:
	- @em blob  Opaque pointer from the calling code.
	- @em mgr Pointer to the socket_manager that manages the socket.
	- @em sock_fd File descriptor of the socket that read the data.
	- @em data Pointer to the data received.
	- @em parent_id (if > 0) listener socket from which the data socket was spawned.
	*/
	void (*data_received) (
		void* blob,
		struct socket_manager_struct* mgr,
		int sock_fd,
		char* data,
		int parent_id
	);

	/** @brief Callback for closing the socket.
	Parameters:
	- @em blob Opaque pointer from the calling code.
	- @em sock_fd File descriptor of the socket that was closed.
	*/
	void (*on_socket_closed) (  
		void* blob,
		int sock_fd
	);

	socket_node* socket;       /**< Linked list of managed sockets. */
	void* blob;                /**< Opaque pointer from the calling code .*/
};
typedef struct socket_manager_struct socket_manager;

void socket_manager_free(socket_manager* mgr);

int socket_open_tcp_server(socket_manager*, int port, const char* listen_ip );

int socket_open_unix_server(socket_manager* mgr, const char* path);

int socket_open_udp_server( socket_manager* mgr, int port, const char* listen_ip );

int socket_open_tcp_client(socket_manager*, int port, const char* dest_addr);

int socket_open_unix_client(socket_manager*, const char* sock_path);

int socket_open_udp_client( socket_manager* mgr );

int socket_send(int sock_fd, const char* data);

int socket_send_timeout( int sock_fd, const char* data, int usecs );

void socket_disconnect(socket_manager*, int sock_fd);

int socket_wait(socket_manager* mgr, int timeout, int sock_fd);

int socket_wait_all(socket_manager* mgr, int timeout);

void _socket_print_list(socket_manager* mgr);

int socket_connected(int sock_fd);

#ifdef __cplusplus
}
#endif

#endif

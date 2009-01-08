#ifndef SOCKET_BUNDLE_H
#define SOCKET_BUNDLE_H

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

#define SERVER_SOCKET			1
#define CLIENT_SOCKET			2

#define INET 10 
#define UNIX 11 


/* models a single socket connection */
struct socket_node_struct {
	int endpoint;		/* SERVER_SOCKET or CLIENT_SOCKET */
	int addr_type;		/* INET or UNIX */
	int sock_fd;
	int parent_id;		/* if we're a new client for a server socket, 
								this points to the server socket we spawned from */
	struct socket_node_struct* next;
};
typedef struct socket_node_struct socket_node;


/* Maintains the socket set */
struct socket_manager_struct {
	/* callback for passing up any received data.  sock_fd is the socket
		that read the data.  parent_id (if > 0) is the socket id of the 
		server that this socket spawned from (i.e. it's a new client connection) */
	void (*data_received) 
		(void* blob, struct socket_manager_struct*, 
		 int sock_fd, char* data, int parent_id);

	void (*on_socket_closed) (void* blob, int sock_fd);

	socket_node* socket;
	void* blob;
};
typedef struct socket_manager_struct socket_manager;

void socket_manager_free(socket_manager* mgr);

/* creates a new server socket node and adds it to the socket set.
	returns socket id on success.  -1 on failure.
	socket_type is one of INET or UNIX  */
int socket_open_tcp_server(socket_manager*, int port, const char* listen_ip );

int socket_open_unix_server(socket_manager* mgr, const char* path);

int socket_open_udp_server( socket_manager* mgr, int port, const char* listen_ip );

/* creates a client TCP socket and adds it to the socket set.
	returns 0 on success.  -1 on failure.  */
int socket_open_tcp_client(socket_manager*, int port, const char* dest_addr);

/* creates a client UNIX socket and adds it to the socket set.
	returns 0 on success.  -1 on failure.  */
int socket_open_unix_client(socket_manager*, const char* sock_path);

int socket_open_udp_client( socket_manager* mgr, int port, const char* dest_addr);

/* sends the given data to the given socket. returns 0 on success, -1 otherwise */
int socket_send(int sock_fd, const char* data);

/* waits at most usecs microseconds for the socket buffer to
 * be available */
int socket_send_timeout( int sock_fd, const char* data, int usecs );

/* disconnects the node with the given sock_fd and removes
	it from the socket set */
void socket_disconnect(socket_manager*, int sock_fd);

/* XXX This only works if 'sock_fd' is a client socket... */
int socket_wait(socket_manager* mgr, int timeout, int sock_fd);

/* waits on all sockets for incoming data.  
	timeout == -1	| block indefinitely
	timeout == 0	| don't block, just read any available data off all sockets
	timeout == x	| block for at most x seconds */
int socket_wait_all(socket_manager* mgr, int timeout);

/* utility function for displaying the currently attached sockets */
void _socket_print_list(socket_manager* mgr);

int socket_connected(int sock_fd);

#ifdef __cplusplus
}
#endif

#endif

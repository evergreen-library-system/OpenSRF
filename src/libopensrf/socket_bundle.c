/**
	@file socket_bundle.c
	@brief Collection of socket-handling routines.
*/

#include <opensrf/socket_bundle.h>

#define LISTENER_SOCKET   1
#define DATA_SOCKET       2

#define INET 10
#define UNIX 11

/**
	@brief Represents a socket owned by a socket_manager.

	A socket_manager owns a linked list of socket_nodes representing the collection of
	sockets that it manages.  It may contain a single socket for passing data, or it may
	contain a listener socket (conceivably more than one) together with any associated
	sockets created by accept() for communicating with a client.
*/
struct socket_node_struct {
	int endpoint;       /**< Role of socket: LISTENER_SOCKET or DATA_SOCKET. */
	int addr_type;      /**< INET or UNIX. */
	int sock_fd;        /**< File descriptor for socket. */
	int parent_id;      /**< For a socket created by accept() for a listener socket,
	                        this is the listener socket we spawned from. */
	struct socket_node_struct* next;  /**< Linkage pointer for linked list. */
};

/** @brief Size of buffer used to read from the sockets */
#define RBUFSIZE 1024

static socket_node* _socket_add_node(socket_manager* mgr,
		int endpoint, int addr_type, int sock_fd, int parent_id );
static socket_node* socket_find_node(socket_manager* mgr, int sock_fd);
static void socket_remove_node(socket_manager*, int sock_fd);
static int _socket_send(int sock_fd, const char* data, int flags);
static int _socket_handle_new_client(socket_manager* mgr, socket_node* node);
static int _socket_handle_client_data(socket_manager* mgr, socket_node* node);


/* --------------------------------------------------------------------
	Test Code
	-------------------------------------------------------------------- */
/*
int count = 0;
void printme(void* blob, socket_manager* mgr,
		int sock_fd, char* data, int parent_id) {

	fprintf(stderr, "Got data from socket %d with parent %d => %s",
			sock_fd, parent_id, data );

	socket_send(sock_fd, data);

	if(count++ > 2) {
		socket_disconnect(mgr, sock_fd);
		_socket_print_list(mgr);
	}
}

int main(int argc, char* argv[]) {
	socket_manager manager;
	memset(&manager, 0, sizeof(socket_manager));
	int port = 11000;
	if(argv[1])
		port = atoi(argv[1]);

	manager.data_received = &printme;
	socket_open_tcp_server(&manager, port);

	while(1)
		socket_wait_all(&manager, -1);

	return 0;
}
*/
/* -------------------------------------------------------------------- */


/**
	@brief Create a new socket_node and add it to a socket_manager's list.
	@param mgr Pointer to the socket_manager.
	@param endpoint LISTENER_SOCKET or DATA_SOCKET, denoting how the socket is to be used.
	@param addr_type address type: INET or UNIX.
	@param sock_fd sock_fd for the new socket_node.
	@param parent_id parent_id for the new node.
	@return Pointer to the new socket_node.

	If @a parent_id is negative, the new socket_node receives a parent_id of 0.
*/
static socket_node* _socket_add_node(socket_manager* mgr,
		int endpoint, int addr_type, int sock_fd, int parent_id ) {

	if(mgr == NULL) return NULL;
	osrfLogInternal( OSRF_LOG_MARK, "Adding socket node with fd %d", sock_fd);
	socket_node* new_node = safe_malloc(sizeof(socket_node));

	new_node->endpoint	= endpoint;
	new_node->addr_type	= addr_type;
	new_node->sock_fd	= sock_fd;
	new_node->next		= NULL;
	new_node->parent_id = 0;
	if(parent_id > 0)
		new_node->parent_id = parent_id;

	new_node->next			= mgr->socket;
	mgr->socket				= new_node;
	return new_node;
}

/**
	@brief Create an TCP INET listener socket and add it to a socket_manager's list.
	@param mgr Pointer to the socket manager that will own the socket.
	@param port The port number to bind to.
	@param listen_ip The IP address to bind to; or, NULL for INADDR_ANY.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: socket(), bind(), and listen().  Creates a LISTENER_SOCKET.
*/
int socket_open_tcp_server(socket_manager* mgr, int port, const char* listen_ip) {

	if( mgr == NULL ) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_server(): NULL mgr");
		return -1;
	}

	int sock_fd;
	struct sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;

	if(listen_ip != NULL) {
		struct in_addr addr;
		if( inet_aton( listen_ip, &addr ) )
			server_addr.sin_addr.s_addr = addr.s_addr;
		else {
			osrfLogError( OSRF_LOG_MARK, "Listener address is invalid: %s", listen_ip );
			return -1;
		}
	} else {
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	server_addr.sin_port = htons(port);

	errno = 0;
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd < 0) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_server(): Unable to create TCP socket: %s",
			strerror( errno ) );
		return -1;
	}

	errno = 0;
	if(bind( sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_server(): cannot bind to port %d: %s",
			port, strerror( errno ) );
		close( sock_fd );
		return -1;
	}

	errno = 0;
	if(listen(sock_fd, 20) == -1) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_server(): listen() returned error: %s",
			strerror( errno ) );
		close( sock_fd );
		return -1;
	}

	_socket_add_node(mgr, LISTENER_SOCKET, INET, sock_fd, 0);
	return sock_fd;
}

/**
	@brief Create a UNIX domain listener socket and add it to the socket_manager's list.
	@param mgr Pointer to the socket_manager that will own the socket.
	@param path Name of the socket within the file system.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: socket(), bind(), listen().  Creates a LISTENER_SOCKET.

	Apply socket option TCP_NODELAY in order to reduce latency.
*/
int socket_open_unix_server(socket_manager* mgr, const char* path) {
	if(mgr == NULL || path == NULL) return -1;

	osrfLogDebug( OSRF_LOG_MARK, "opening unix socket at %s", path);
	int sock_fd;
	struct sockaddr_un server_addr;

	if(strlen(path) > sizeof(server_addr.sun_path) - 1)
	{
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_unix_server(): path too long: %s",
			path );
		return -1;
	}

	errno = 0;
	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sock_fd < 0){
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_unix_server(): socket() failed: %s",
			strerror( errno ) );
		return -1;
	}

	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, path);

	errno = 0;
	if( bind(sock_fd, (struct sockaddr*) &server_addr,
				sizeof(struct sockaddr_un)) < 0) {
		osrfLogWarning( OSRF_LOG_MARK,
			"socket_open_unix_server(): cannot bind to unix port %s: %s",
			path, strerror( errno ) );
		close( sock_fd );
		return -1;
	}

	errno = 0;
	if(listen(sock_fd, 20) == -1) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_unix_server(): listen() returned error: %s",
			strerror( errno ) );
		close( sock_fd );
		return -1;
	}

	osrfLogDebug( OSRF_LOG_MARK, "unix socket successfully opened");

	int i = 1;

	/* causing problems with router for some reason ... */
	//osrfLogDebug( OSRF_LOG_MARK, "Setting SO_REUSEADDR");
	//setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	//osrfLogDebug( OSRF_LOG_MARK, "Setting TCP_NODELAY");
	setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));

	_socket_add_node(mgr, LISTENER_SOCKET, UNIX, sock_fd, 0);
	return sock_fd;
}


/**
	@brief Create a UDP socket for a server, and add it to a socket_manager's list.
	@param mgr Pointer to the socket_manager that will own the socket.
	@param port The port number to bind to.
	@param listen_ip The IP address to bind to, or NULL for INADDR_ANY.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: socket(), bind().  Creates a DATA_SOCKET.
*/
int socket_open_udp_server(
		socket_manager* mgr, int port, const char* listen_ip ) {

	int sockfd;
	struct sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	if(listen_ip) {
		struct in_addr addr;
		if( inet_aton( listen_ip, &addr ) )
			server_addr.sin_addr.s_addr = addr.s_addr;
		else {
			osrfLogError( OSRF_LOG_MARK, "UDP listener address is invalid: %s", listen_ip );
			return -1;
		}
	} else
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	errno = 0;
	if( (sockfd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 ) {
		osrfLogWarning( OSRF_LOG_MARK, "Unable to create UDP socket: %s", strerror( errno ) );
		return -1;
	}

	errno = 0;
	if( (bind (sockfd, (struct sockaddr *) &server_addr,sizeof(server_addr))) ) {
		osrfLogWarning( OSRF_LOG_MARK, "Unable to bind to UDP port %d: %s",
			port, strerror( errno ) );
		close( sockfd );
		return -1;
	}

	_socket_add_node(mgr, DATA_SOCKET, INET, sockfd, 0);
	return sockfd;
}


/**
	@brief Create a client TCP socket, connect with it, and add it to a socket_manager's list.
	@param mgr Pointer to the socket_manager that will own the socket.
	@param port What port number to connect to.
	@param dest_addr Host name or IP address of the server to which we are connecting.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: getaddrinfo(), socket(), connect().  Creates a DATA_SOCKET.

	Applies socket option TCP_NODELAY in order to reduce latency.
*/
int socket_open_tcp_client(socket_manager* mgr, int port, const char* dest_addr) {

	struct sockaddr_in remoteAddr;
	int sock_fd;

	// ------------------------------------------------------------------
	// Get the IP address of the hostname (for TCP only)
	// ------------------------------------------------------------------
	struct addrinfo hints = { 0, 0, 0, 0, 0, NULL, NULL, NULL };
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* addr_info = NULL;
	errno = 0;
	int rc = getaddrinfo( dest_addr, NULL, &hints, &addr_info );
	if( rc || ! addr_info ) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_client(): No Such Host => %s: %s",
			dest_addr, gai_strerror( rc ) );
		return -1;
	}

	// Look for an address supporting IPv4.  Someday we'll look for
	// either IPv4 or IPv6, and branch according to what we find.
	while( addr_info && addr_info->ai_family != PF_INET ) {
		addr_info = addr_info->ai_next;
	}

	if( ! addr_info ) {
		osrfLogWarning( OSRF_LOG_MARK,
			"socket_open_tcp_client(): Host %s does not support IPV4", dest_addr );
		return -1;
	}

	// ------------------------------------------------------------------
	// Create the socket
	// ------------------------------------------------------------------
	errno = 0;
	if( (sock_fd = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_client(): Cannot create TCP socket: %s",
			strerror( errno ) );
		return -1;
	}

	int i = 1;
	setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));

	// ------------------------------------------------------------------
	// Construct server info struct
	// ------------------------------------------------------------------
	memset( &remoteAddr, 0, sizeof(remoteAddr));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = htons( port );
	struct sockaddr_in* ai_addr_in = (struct sockaddr_in*) addr_info->ai_addr;
	remoteAddr.sin_addr.s_addr = ai_addr_in->sin_addr.s_addr;

	freeaddrinfo( addr_info );

	// ------------------------------------------------------------------
	// Connect to server
	// ------------------------------------------------------------------
	errno = 0;
	if( connect( sock_fd, (struct sockaddr*) &remoteAddr, sizeof( struct sockaddr_in ) ) < 0 ) {
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_tcp_client(): Cannot connect to server %s: %s",
			dest_addr, strerror(errno) );
		close( sock_fd );
		return -1;
	}

	_socket_add_node(mgr, DATA_SOCKET, INET, sock_fd, -1 );

	return sock_fd;
}


/**
	@brief Create a client UDP socket and add it to a socket_manager's list.
	@param mgr Pointer to the socket_manager that will own the socket.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: socket().  Creates a DATA_SOCKET.
*/
int socket_open_udp_client( socket_manager* mgr ) {

	int sockfd;

	errno = 0;
	if( (sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0 ) {
		osrfLogWarning( OSRF_LOG_MARK,
			"socket_open_udp_client(): Unable to create UDP socket: %s", strerror( errno ) );
		return -1;
	}

	_socket_add_node(mgr, DATA_SOCKET, INET, sockfd, -1 );

	return sockfd;
}


/**
	@brief Create a UNIX domain client socket, connect with it, add it to the socket_manager's list
	@param mgr Pointer to the socket_manager that will own the socket.
	@param sock_path Name of the socket within the file system.
	@return The socket's file descriptor if successful; otherwise -1.

	Calls: socket(), connect().  Creates a DATA_SOCKET.
*/
int socket_open_unix_client(socket_manager* mgr, const char* sock_path) {

	int sock_fd, len;
	struct sockaddr_un usock;

	if(strlen(sock_path) > sizeof(usock.sun_path) - 1)
	{
		osrfLogWarning( OSRF_LOG_MARK, "socket_open_unix_client(): path too long: %s",
		   sock_path );
		return -1;
	}

	errno = 0;
	if( (sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 )) < 0 ) {
		osrfLogWarning(  OSRF_LOG_MARK, "socket_open_unix_client(): Cannot create UNIX socket: %s", strerror( errno ) );
		return -1;
	}

	usock.sun_family = AF_UNIX;
	strcpy( usock.sun_path, sock_path );

	len = sizeof( usock.sun_family ) + strlen( usock.sun_path );

	errno = 0;
	if( connect( sock_fd, (struct sockaddr *) &usock, len ) < 0 ) {
		osrfLogWarning(  OSRF_LOG_MARK, "Error connecting to unix socket: %s",
			strerror( errno ) );
		close( sock_fd );
		return -1;
	}

	_socket_add_node(mgr, DATA_SOCKET, UNIX, sock_fd, -1 );

	return sock_fd;
}


/**
	@brief Search a socket_manager's list for a socket node for a given file descriptor.
	@param mgr Pointer to the socket manager.
	@param sock_fd The file descriptor to be sought.
	@return A pointer to the socket_node if found; otherwise NULL.

	Traverse a linked list owned by the socket_manager.
*/
static socket_node* socket_find_node(socket_manager* mgr, int sock_fd) {
	if(mgr == NULL) return NULL;
	socket_node* node = mgr->socket;
	while(node) {
		if(node->sock_fd == sock_fd)
			return node;
		node = node->next;
	}
	return NULL;
}

/* removes the node with the given sock_fd from the list and frees it */
/**
	@brief Remove a socket node for a given fd from a socket_manager's list.
	@param mgr Pointer to the socket_manager.
	@param sock_fd The file descriptor whose socket_node is to be removed.

	This function does @em not close the socket.  It just removes a node from the list, and
	frees it.  The disposition of the socket is the responsibility of the calling code.
*/
static void socket_remove_node(socket_manager* mgr, int sock_fd) {

	if(mgr == NULL) return;

	osrfLogDebug( OSRF_LOG_MARK, "removing socket %d", sock_fd);

	socket_node* head = mgr->socket;
	socket_node* tail = head;
	if(head == NULL) return;

	/* if removing the first node in the list */
	if(head->sock_fd == sock_fd) {
		mgr->socket = head->next;
		free(head);
		return;
	}

	head = head->next;

	/* if removing any other node */
	while(head) {
		if(head->sock_fd == sock_fd) {
			tail->next = head->next;
			free(head);
			return;
		}
		tail = head;
		head = head->next;
	}
}


/**
	@brief Write to the log: a list of socket_nodes in a socket_manager's list.
	@param mgr Pointer to the socket_manager.

	For testing and debugging.

	The messages are issued as DEBG messages, and show each file descriptor and its parent.
*/
void _socket_print_list(socket_manager* mgr) {
	if(mgr == NULL) return;
	socket_node* node = mgr->socket;
	osrfLogDebug( OSRF_LOG_MARK, "socket_node list: [");
	while(node) {
		osrfLogDebug( OSRF_LOG_MARK, "sock_fd: %d | parent_id: %d",
				node->sock_fd, node->parent_id);
		node = node->next;
	}
	osrfLogDebug( OSRF_LOG_MARK, "]");
}

/**
	@brief Send a nul-terminated string over a socket.
	@param sock_fd The file descriptor for the socket.
	@param data Pointer to the string to be sent.
	@return 0 if successful, -1 if not.

	This function is a thin wrapper for _socket_send().
*/
int socket_send(int sock_fd, const char* data) {
	return _socket_send( sock_fd, data, 0);
}

/**
	@brief Send a nul-terminated string over a socket.
	@param sock_fd The file descriptor for the socket.
	@param data Pointer to the string to be sent.
	@param flags A set of bitflags to be passed to send().
	@return 0 if successful, -1 if not.

	This function is the final common pathway for all outgoing socket traffic.
*/
static int _socket_send(int sock_fd, const char* data, int flags) {

	signal(SIGPIPE, SIG_IGN); /* in case a unix socket was closed */

	errno = 0;
	size_t r = send( sock_fd, data, strlen(data), flags );
	int local_errno = errno;

	if( r == -1 ) {
		osrfLogWarning( OSRF_LOG_MARK, "_socket_send(): Error sending data with return %d", r );
		osrfLogWarning( OSRF_LOG_MARK, "Last Sys Error: %s", strerror(local_errno));
		return -1;
	}

	return 0;
}


/* sends the given data to the given socket.
 * sets the send flag MSG_DONTWAIT which will allow the
 * process to continue even if the socket buffer is full
 * returns 0 on success, -1 otherwise */
//int socket_send_nowait( int sock_fd, const char* data) {
//	return _socket_send( sock_fd, data, MSG_DONTWAIT);
//}


/**
	@brief Wait for a socket to be ready to send, and then send a string over it.
	@param sock_fd File descriptor of the socket.
	@param data Pointer to a nul-terminated string to be sent.
	@param usecs How long to wait, in microseconds, before timing out.
	@return 0 if successful, -1 if not.

	The socket may not accept all the data we want to give it.
*/
int socket_send_timeout( int sock_fd, const char* data, int usecs ) {

	fd_set write_set;
	FD_ZERO( &write_set );
	FD_SET( sock_fd, &write_set );

	const int mil = 1000000;
	int secs = (int) usecs / mil;
	usecs = usecs - (secs * mil);

	struct timeval tv;
	tv.tv_sec = secs;
	tv.tv_usec = usecs;

	errno = 0;
	int ret = select( sock_fd + 1, NULL, &write_set, NULL, &tv);
	if( ret > 0 ) return _socket_send( sock_fd, data, 0);

	osrfLogError(OSRF_LOG_MARK, "socket_send_timeout(): "
		"timed out on send for socket %d after %d secs, %d usecs: %s",
		sock_fd, secs, usecs, strerror( errno ) );

	return -1;
}


/* disconnects the node with the given sock_fd and removes
	it from the socket set */
/**
	@brief Close a socket, and remove it from the socket_manager's list.
	@param mgr Pointer to the socket_manager.
	@param sock_fd File descriptor for the socket to be closed.

	We close the socket before determining whether it belongs to the socket_manager in question.
*/
void socket_disconnect(socket_manager* mgr, int sock_fd) {
	osrfLogInternal( OSRF_LOG_MARK, "Closing socket %d", sock_fd);
	close( sock_fd );
	socket_remove_node(mgr, sock_fd);
}


/**
	@brief Determine whether a socket is valid.
	@param sock_fd File descriptor for the socket.
	@return 1 if the socket is valid, or 0 if it isn't.

	The test is based on a call to select().  If the socket is valid but is not ready to be
	written to, we wait until it is ready, then return 1.

	If the select() fails, it may be because it was interrupted by a signal.  In that case
	we try again.  Otherwise we assume that the socket is no longer valid.  This can happen
	if, for example, the other end of a connection has closed the connection.

	The select() can also fail if it is unable to allocate enough memory for its own internal
	use.  If that happens, we may erroneously report a valid socket as invalid, but we
	probably wouldn't be able to use it anyway if we're that close to exhausting memory.
*/
int socket_connected(int sock_fd) {
	fd_set read_set;
	FD_ZERO( &read_set );
	FD_SET( sock_fd, &read_set );
	while( 1 ) {
		if( select( sock_fd + 1, &read_set, NULL, NULL, NULL) == -1 )
			return 0;
		else if( EINTR == errno )
			continue;
		else
			return 1;
	}
}

/**
	@brief Look for input on a given socket.  If you find some, react to it.
	@param mgr Pointer to the socket_manager that presumably owns the socket.
	@param timeout Timeout interval, in seconds (see notes).
	@param sock_fd The file descriptor to look at.
	@return 0 if successful, or -1 if a timeout or other error occurs, or if the sender
		closes the connection.

	If @a timeout is -1, wait indefinitely for input activity to appear.  If @a timeout is
	zero, don't wait at all.  If @a timeout is positive, wait that number of seconds
	before timing out.  If @a timeout has a negative value other than -1, the results are not
	well defined, but we'll probably get an EINVAL error from select().

	If we detect activity, branch on the type of socket:

	- If it's a listener, accept a new connection, and add the new socket to the
	socket_manager's list, without actually reading any data.
	- Otherwise, read as much data as is available from the input socket, passing it a
	buffer at a time to whatever callback function has been defined to the socket_manager.
*/
int socket_wait( socket_manager* mgr, int timeout, int sock_fd ) {

	int retval = 0;
	fd_set read_set;
	FD_ZERO( &read_set );
	FD_SET( sock_fd, &read_set );

	struct timeval tv;
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	errno = 0;

	if( timeout < 0 ) {

		// If timeout is -1, we block indefinitely
		if( (retval = select( sock_fd + 1, &read_set, NULL, NULL, NULL)) == -1 ) {
			osrfLogDebug( OSRF_LOG_MARK, "Call to select() interrupted: Sys Error: %s",
					strerror(errno));
			return -1;
		}

	} else if( timeout > 0 ) { /* timeout of 0 means don't block */

		if( (retval = select( sock_fd + 1, &read_set, NULL, NULL, &tv)) == -1 ) {
			osrfLogDebug( OSRF_LOG_MARK, "Call to select() interrupted: Sys Error: %s",
					strerror(errno));
			return -1;
		}
	}

	osrfLogInternal( OSRF_LOG_MARK, "%d active sockets after select()", retval);

	socket_node* node = socket_find_node(mgr, sock_fd);
	if( node ) {
		if( node->endpoint == LISTENER_SOCKET ) {
			_socket_handle_new_client( mgr, node );  // accept new connection
		} else {
			int status = _socket_handle_client_data( mgr, node );   // read data
			if( status == -1 ) {
				close( sock_fd );
				socket_remove_node( mgr, sock_fd );
				return -1;
			}
		}
		return 0;
	}
	else
		return -1;    // No such file descriptor for this socket_manager
}


/**
	@brief Wait for input on all of a socket_manager's sockets; react to any input found.
	@param mgr Pointer to the socket_manager.
	@param timeout How many seconds to wait before timing out (see notes).
	@return 0 if successful, or -1 if a timeout or other error occurs.

	If @a timeout is -1, wait indefinitely for input activity to appear.  If @a timeout is
	zero, don't wait at all.  If @a timeout is positive, wait that number of seconds
	before timing out.  If @a timeout has a negative value other than -1, the results are not
	well defined, but we'll probably get an EINVAL error from select().

	For each active socket found:

	- If it's a listener, accept a new connection, and add the new socket to the
	socket_manager's list, without actually reading any data.
	- Otherwise, read as much data as is available from the input socket, passing it a
	buffer at a time to whatever callback function has been defined to the socket_manager.
*/
int socket_wait_all(socket_manager* mgr, int timeout) {

	if(mgr == NULL) {
		osrfLogWarning( OSRF_LOG_MARK,  "socket_wait_all(): null mgr" );
		return -1;
	}

	int num_active = 0;
	fd_set read_set;
	FD_ZERO( &read_set );

	socket_node* node = mgr->socket;
	int max_fd = 0;
	while(node) {
		osrfLogInternal( OSRF_LOG_MARK, "Adding socket fd %d to select set",node->sock_fd);
		FD_SET( node->sock_fd, &read_set );
		if(node->sock_fd > max_fd) max_fd = node->sock_fd;
		node = node->next;
	}
	max_fd += 1;

	struct timeval tv;
	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	errno = 0;

	if( timeout < 0 ) {

		// If timeout is -1, there is no timeout passed to the call to select
		if( (num_active = select( max_fd, &read_set, NULL, NULL, NULL)) == -1 ) {
			osrfLogWarning( OSRF_LOG_MARK, "select() call aborted: %s", strerror(errno));
			return -1;
		}

	} else if( timeout != 0 ) { /* timeout of 0 means don't block */

		if( (num_active = select( max_fd, &read_set, NULL, NULL, &tv)) == -1 ) {
			osrfLogWarning( OSRF_LOG_MARK, "select() call aborted: %s", strerror(errno));
			return -1;
		}
	}

	osrfLogDebug( OSRF_LOG_MARK, "%d active sockets after select()", num_active);

	node = mgr->socket;
	int handled = 0;

	while(node && (handled < num_active)) {

		socket_node* next_node = node->next;
		int sock_fd = node->sock_fd;

		/* does this socket have data? */
		if( FD_ISSET( sock_fd, &read_set ) ) {

			osrfLogInternal( OSRF_LOG_MARK, "Socket %d active", sock_fd);
			handled++;
			FD_CLR(sock_fd, &read_set);

			if(node->endpoint == LISTENER_SOCKET)
				_socket_handle_new_client(mgr, node);

			else {
				if( _socket_handle_client_data(mgr, node) == -1 ) {
					/* someone may have yanked a socket_node out from under us */
					close( sock_fd );
					socket_remove_node( mgr, sock_fd );
				}
			}
		}

		node = next_node;
	} // is_set

	return 0;
}

/**
	@brief Accept a new socket from a listener, and add it to the socket_manager's list.
	@param mgr Pointer to the socket_manager that will own the new socket.
	@param node Pointer to the socket_node for the listener socket.
	@return 0 if successful, or -1 if not.

	Call: accept().  Creates a DATA_SOCKET (even though the socket resides on the server).
*/
static int _socket_handle_new_client(socket_manager* mgr, socket_node* node) {
	if(mgr == NULL || node == NULL) return -1;

	errno = 0;
	int new_sock_fd;
	new_sock_fd = accept(node->sock_fd, NULL, NULL);
	if(new_sock_fd < 0) {
		osrfLogWarning( OSRF_LOG_MARK, "_socket_handle_new_client(): accept() failed: %s",
			strerror( errno ) );
		return -1;
	}

	if(node->addr_type == INET) {
		_socket_add_node(mgr, DATA_SOCKET, INET, new_sock_fd, node->sock_fd);
		osrfLogDebug( OSRF_LOG_MARK, "Adding new INET client for %d", node->sock_fd);

	} else if(node->addr_type == UNIX) {
		_socket_add_node(mgr, DATA_SOCKET, UNIX, new_sock_fd, node->sock_fd);
		osrfLogDebug( OSRF_LOG_MARK, "Adding new UNIX client for %d", node->sock_fd);
	}

	return 0;
}


/**
	@brief Receive data on a streaming socket.
	@param mgr Pointer to the socket_manager that owns the socket_node.
	@param node Pointer to the socket_node that owns the socket.
	@return 0 if successful, or -1 upon failure.

	Receive one or more buffers until no more bytes are available for receipt.  Add a
	terminal nul to each buffer and pass it to a callback function previously defined by the
	application to the socket_manager.

	If the sender closes the connection, call another callback function, if one has been
	defined.

	Even when the function returns successfully, the received message may not be complete --
	there may be more data that hasn't arrived yet.  It is the responsibility of the
	calling code to recognize message boundaries.

	Called only for a DATA_SOCKET.
*/
static int _socket_handle_client_data(socket_manager* mgr, socket_node* node) {
	if(mgr == NULL || node == NULL) return -1;

	char buf[RBUFSIZE];
	int read_bytes;
	int sock_fd = node->sock_fd;

	set_fl(sock_fd, O_NONBLOCK);

	osrfLogInternal( OSRF_LOG_MARK, "%ld : Received data at %f\n",
			(long) getpid(), get_timestamp_millis());

	while( (read_bytes = recv(sock_fd, buf, RBUFSIZE-1, 0) ) > 0 ) {
		buf[read_bytes] = '\0';
		osrfLogInternal( OSRF_LOG_MARK, "Socket %d Read %d bytes and data: %s",
				sock_fd, read_bytes, buf);
		if(mgr->data_received)
			mgr->data_received(mgr->blob, mgr, sock_fd, buf, node->parent_id);
	}
	int local_errno = errno; /* capture errno as set by recv() */

	if(socket_find_node(mgr, sock_fd)) {  /* someone may have closed this socket */
		clr_fl(sock_fd, O_NONBLOCK);
		if(read_bytes < 0) {
			// EAGAIN would have meant that no more data was available
			if(local_errno != EAGAIN)   // but if that's not the case...
				osrfLogWarning( OSRF_LOG_MARK, " * Error reading socket with error %s",
					strerror(local_errno) );
		}

	} else { return -1; } /* inform the caller that this node has been tampered with */

	if(read_bytes == 0) {  /* socket closed by client */
		if(mgr->on_socket_closed) {
			mgr->on_socket_closed(mgr->blob, sock_fd);
		}
		return -1;
	}

	return 0;

}


/**
	@brief Destroy a socket_manager, and close all of its sockets.
	@param mgr Pointer to the socket_manager to be destroyed.
*/
void socket_manager_free(socket_manager* mgr) {
	if(mgr == NULL) return;
	socket_node* tmp;
	while(mgr->socket) {
		tmp = mgr->socket->next;
		socket_disconnect(mgr, mgr->socket->sock_fd);
		mgr->socket = tmp;
	}
	free(mgr);

}

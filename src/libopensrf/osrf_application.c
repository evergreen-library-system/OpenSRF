#include <opensrf/osrf_application.h>

/**
	@file osrf_application.c
	@brief Load and manage shared object libraries.

	Maintain a registry of applications, using an osrfHash keyed on application name,

	For each application, load a shared object library so that we can call
	application-specific functions dynamically.  In order to map method names to the
	corresponding functions (i.e. symbol names in the library), maintain a registry of
	methods, using an osrfHash keyed on method name.
*/

// The following macro is commented out because it ia no longer used.

// Used internally to make sure the method description provided is OK
/*
#define OSRF_METHOD_VERIFY_DESCRIPTION(app, d) \
	if(!app) return -1; \
	if(!d) return -1;\
	if(!d->name) { \
		osrfLogError( OSRF_LOG_MARK,  "No method name provided in description" ), \
		return -1; \
} \
	if(!d->symbol) { \
		osrfLogError( OSRF_LOG_MARK, "No method symbol provided in description" ), \
		return -1; \
} \
	if(!d->notes) \
		d->notes = ""; \
	if(!d->paramNotes) \
		d->paramNotes = "";\
	if(!d->returnNotes) \
		d->returnNotes = "";
*/

/**
	@name Well known method names
	@brief These methods are automatically implemented for every application.
*/
/*@{*/
#define OSRF_SYSMETHOD_INTROSPECT               "opensrf.system.method"
#define OSRF_SYSMETHOD_INTROSPECT_ATOMIC        "opensrf.system.method.atomic"
#define OSRF_SYSMETHOD_INTROSPECT_ALL           "opensrf.system.method.all"
#define OSRF_SYSMETHOD_INTROSPECT_ALL_ATOMIC    "opensrf.system.method.all.atomic"
#define OSRF_SYSMETHOD_ECHO                     "opensrf.system.echo"
#define OSRF_SYSMETHOD_ECHO_ATOMIC              "opensrf.system.echo.atomic"
/*@}*/

/**
	@name Method options
	@brief Macros that get OR'd together to form method options.

	These options are in addition to the ones stipulated by the caller of
	osrfRegisterMethod(), and are not externally visible.
*/
/*@{*/
/**
	@brief  Marks a method as a system method.

	System methods are implemented by generic functions, called via static linkage.  They
	are not loaded or executed from shared objects.
*/
#define OSRF_METHOD_SYSTEM          1
/**
	@brief  Combines all responses into a single RESULT message.

	For a @em non-atomic method, the server returns each response to the client in a
	separate RESULT message.  It sends a STATUS message at the end to signify the end of the
	message stream.

	For an @em atomic method, the server buffers all responses until the method returns,
	and then sends them all at once in a single RESULT message (followed by a STATUS message).
	Each individual response is encoded as an entry in a JSON array.  This buffering is
	transparent to the function that implements the method.

	Atomic methods incur less networking overhead than non-atomic methods, at the risk of
	creating excessively large RESULT messages.  The HTTP gateway requires the atomic versions
	of streaming methods because of the stateless nature of the HTTP protocol.

	If OSRF_METHOD_STREAMING is set for a method, the application generates both an atomic
	and a non-atomic method, whose names are identical except that the atomic one carries a
	suffix of ".atomic".
*/
#define OSRF_METHOD_ATOMIC          4
/*@}*/

/**
	@brief Default size of output buffer.
*/
#define OSRF_MSG_BUFFER_SIZE     10240

/**
	@brief Represent an Application.
*/
typedef struct {
	void* handle;               /**< Handle to the shared object library. */
	osrfHash* methods;          /**< Registry of method names. */
	void (*onExit) (void);      /**< Exit handler for the application. */
} osrfApplication;

static void register_method( osrfApplication* app, const char* methodName,
	const char* symbolName, const char* notes, int argc, int options, void * user_data );
static osrfMethod* build_method( const char* methodName, const char* symbolName,
	const char* notes, int argc, int options, void* );
static void osrfAppSetOnExit(osrfApplication* app, const char* appName);
static void register_system_methods( osrfApplication* app );
static inline osrfApplication* _osrfAppFindApplication( const char* name );
static inline osrfMethod* osrfAppFindMethod( osrfApplication* app, const char* methodName );
static int _osrfAppRespond( osrfMethodContext* context, const jsonObject* data, int complete );
static int _osrfAppPostProcess( osrfMethodContext* context, int retcode );
static int _osrfAppRunSystemMethod(osrfMethodContext* context);
static void _osrfAppSetIntrospectMethod( osrfMethodContext* ctx, const osrfMethod* method,
	jsonObject* resp );
static int osrfAppIntrospect( osrfMethodContext* ctx );
static int osrfAppIntrospectAll( osrfMethodContext* ctx );
static int osrfAppEcho( osrfMethodContext* ctx );
static void osrfMethodFree( char* name, void* p );
static void osrfAppFree( char* name, void* p );

/**
	@brief Registry of applications.

	The key of the hash is the application name, and the associated data is an osrfApplication.
*/
static osrfHash* _osrfAppHash = NULL;

/**
	@brief Register an application.
	@param appName Name of the application.
	@param soFile Name of the shared object file to be loaded for this application.
	@return Zero if successful, or -1 upon error.

	Open the shared object file and call its osrfAppInitialize() function, if it has one.
	Register the standard system methods for it.  Arrange for the application name to
	appear in subsequent log messages.
*/
int osrfAppRegisterApplication( const char* appName, const char* soFile ) {
	if( !appName || ! soFile ) return -1;
	char* error;

	osrfLogSetAppname( appName );

	if( !_osrfAppHash ) {
		_osrfAppHash = osrfNewHash();
		osrfHashSetCallback( _osrfAppHash, osrfAppFree );
	}

	osrfLogInfo( OSRF_LOG_MARK, "Registering application %s with file %s", appName, soFile );

	// Open the shared object.
	void* handle = dlopen( soFile, RTLD_NOW );
	if( ! handle ) {
		const char* msg = dlerror();
		osrfLogWarning( OSRF_LOG_MARK, "Failed to dlopen library file %s: %s", soFile, msg );
		return -1;
	}

	// Construct the osrfApplication.
	osrfApplication* app = safe_malloc(sizeof(osrfApplication));
	app->handle = handle;
	app->methods = osrfNewHash();
	osrfHashSetCallback( app->methods, osrfMethodFree );
	app->onExit = NULL;

	// Add the newly-constructed app to the list.
	osrfHashSet( _osrfAppHash, app, appName );

	// Try to run the initialize method.  Typically it will register one or more
	// methods of the application.
	int (*init) (void);
	*(void **) (&init) = dlsym( handle, "osrfAppInitialize" );

	if( (error = dlerror()) != NULL ) {
		osrfLogWarning( OSRF_LOG_MARK,
			"! Unable to locate method symbol [osrfAppInitialize] for app %s: %s",
			appName, error );

	} else {

		/* run the method */
		int ret;
		if( (ret = (*init)()) ) {
			osrfLogWarning( OSRF_LOG_MARK, "Application %s returned non-zero value from "
				"'osrfAppInitialize', not registering...", appName );
			osrfHashRemove( _osrfAppHash, appName );
			return ret;
		}
	}

	register_system_methods( app );
	osrfLogInfo( OSRF_LOG_MARK, "Application %s registered successfully", appName );
	osrfAppSetOnExit( app, appName );

	return 0;
}

/**
	@brief Save a pointer to the application's exit function.
	@param app Pointer to the osrfApplication.
	@param appName Application name (used only for log messages).

	Look in the shared object for a symbol named "osrfAppChildExit".  If you find one, save
	it as a pointer to the application's exit function.  If present, this function will be
	called when a server's child process (a so-called "drone") is shutting down.
*/
static void osrfAppSetOnExit(osrfApplication* app, const char* appName) {
	if(!(app && appName)) return;

	/* see if we can run the initialize method */
	char* error;
	void (*onExit) (void);
	*(void **) (&onExit) = dlsym(app->handle, "osrfAppChildExit");

	if( (error = dlerror()) != NULL ) {
		osrfLogDebug(OSRF_LOG_MARK, "No exit handler defined for %s", appName);
		return;
	}

	osrfLogInfo(OSRF_LOG_MARK, "registering exit handler for %s", appName);
	app->onExit = (*onExit);
}

/**
	@brief Run the application-specific child initialization function for a given application.
	@param appname Name of the application.
	@return Zero if successful, or if the application has no child initialization function; -1
	if the application is not registered, or if the function returns non-zero.

	The child initialization function must be named "osrfAppChildInit" within the shared
	object library.  It initializes a drone process of a server.
*/
int osrfAppRunChildInit(const char* appname) {
	osrfApplication* app = _osrfAppFindApplication(appname);
	if(!app) return -1;

	char* error;
	int ret;
	int (*childInit) (void);

	*(void**) (&childInit) = dlsym(app->handle, "osrfAppChildInit");

	if( (error = dlerror()) != NULL ) {
		osrfLogInfo( OSRF_LOG_MARK, "No child init defined for app %s : %s", appname, error);
		return 0;
	}

	if( (ret = (*childInit)()) ) {
		osrfLogError(OSRF_LOG_MARK, "App %s child init failed", appname);
		return -1;
	}

	osrfLogInfo(OSRF_LOG_MARK, "%s child init succeeded", appname);
	return 0;
}

/**
	@brief Call the exit handler for every application that has one.

	Normally a server's child process (a so-called "drone") calls this function just before
	shutting down.
*/
void osrfAppRunExitCode( void ) {
	osrfHashIterator* itr = osrfNewHashIterator(_osrfAppHash);
	osrfApplication* app;
	while( (app = osrfHashIteratorNext(itr)) ) {
		if( app->onExit ) {
			osrfLogInfo(OSRF_LOG_MARK, "Running onExit handler for app %s",
				osrfHashIteratorKey(itr) );
			app->onExit();
		}
	}
	osrfHashIteratorFree(itr);
}

/**
	@brief Register a method for a specified application.

	@param appName Name of the application that implements the method.
	@param methodName The fully qualified name of the method.
	@param symbolName The symbol name (function name) that implements the method.
	@param notes Public documentation for this method.
	@param argc The minimum number of arguments for the function.
	@param options Bit switches setting various options.
	@return Zero on success, or -1 on error.

	Registering a method enables us to call the right function when a client requests a
	method call.

	The @a options parameter is zero or more of the following macros, OR'd together:

	- OSRF_METHOD_STREAMING     method may return more than one response
	- OSRF_METHOD_CACHABLE      cache results in memcache

	If the OSRF_METHOD_STREAMING bit is set, also register an ".atomic" version of the method.
*/
int osrfAppRegisterMethod( const char* appName, const char* methodName,
		const char* symbolName, const char* notes, int argc, int options ) {

	return osrfAppRegisterExtendedMethod(
			appName,
			methodName,
			symbolName,
			notes,
			argc,
			options,
			NULL
	);
}

/**
	@brief Register an extended method for a specified application.

	@param appName Name of the application that implements the method.
	@param methodName The fully qualified name of the method.
	@param symbolName The symbol name (function name) that implements the method.
	@param notes Public documentation for this method.
	@param argc How many arguments this method expects.
	@param options Bit switches setting various options.
	@param user_data Opaque pointer to be passed to the dynamically called function.
	@return Zero if successful, or -1 upon error.

	This function is identical to osrfAppRegisterMethod(), except that it also installs
	a method-specific opaque pointer.  When we call the corresponding function at
	run time, this pointer will be available to the function via the method context.
*/
int osrfAppRegisterExtendedMethod( const char* appName, const char* methodName,
	const char* symbolName, const char* notes, int argc, int options, void * user_data ) {

	if( !appName || ! methodName ) return -1;

	osrfApplication* app = _osrfAppFindApplication(appName);
	if(!app) {
		osrfLogWarning( OSRF_LOG_MARK, "Unable to locate application %s", appName );
		return -1;
	}

	osrfLogDebug( OSRF_LOG_MARK, "Registering method %s for app %s", methodName, appName );

	// Extract the only valid option bits, and ignore the rest.
	int opts = options & ( OSRF_METHOD_STREAMING | OSRF_METHOD_CACHABLE );

	// Build and install a non-atomic method.
	register_method(
		app, methodName, symbolName, notes, argc, opts, user_data );

	if( opts & OSRF_METHOD_STREAMING ) {
		// Build and install an atomic version of the same method.
		register_method(
			app, methodName, symbolName, notes, argc, opts | OSRF_METHOD_ATOMIC, user_data );
	}

	return 0;
}

/**
	@brief Register a single method for a specified application.

	@param appName Pointer to the application that implements the method.
	@param methodName The fully qualified name of the method.
	@param symbolName The symbol name (function name) that implements the method.
	@param notes Public documentation for this method.
	@param argc How many arguments this method expects.
	@param options Bit switches setting various options.
	@param user_data Opaque pointer to be passed to the dynamically called function.
*/
static void register_method( osrfApplication* app, const char* methodName,
	const char* symbolName, const char* notes, int argc, int options, void * user_data ) {

	if( !app || ! methodName ) return;

	// Build a method and add it to the list of methods
	osrfMethod* method = build_method(
		methodName, symbolName, notes, argc, options, user_data );
	osrfHashSet( app->methods, method, method->name );
}

/**
	@brief Allocate and populate an osrfMethod.
	@param methodName Name of the method.
	@param symbolName Name of the function that implements the method.
	@param notes Remarks documenting the method.
	@param argc Minimum number of arguments to the method.
	@param options Bit switches setting various options.
	@param user_data An opaque pointer to be passed in the method context.
	@return Pointer to the newly allocated osrfMethod.

	If OSRF_METHOD_ATOMIC is set, append ".atomic" to the method name.
*/
static osrfMethod* build_method( const char* methodName, const char* symbolName,
	const char* notes, int argc, int options, void* user_data ) {

	osrfMethod* method      = safe_malloc(sizeof(osrfMethod));

	if( !methodName )
		methodName = "";  // should never happen

	if( options & OSRF_METHOD_ATOMIC ) {
		// Append ".atomic" to the name.
		char mb[ strlen( methodName ) + 8 ];
		sprintf( mb, "%s.atomic", methodName );
		method->name        = strdup( mb );
	} else {
		method->name        = strdup(methodName);
	}

	if(symbolName)
		method->symbol      = strdup(symbolName);
	else
		method->symbol      = NULL;

	if(notes)
		method->notes       = strdup(notes);
	else
		method->notes       = NULL;

	method->argc            = argc;
	method->options         = options;

	if(user_data)
		method->userData    = user_data;

	method->bufsize         = OSRF_MSG_BUFFER_SIZE;
	return method;
}

/**
	@brief Set the effective output buffer size for a given method.
	@param appName Name of the application.
	@param methodName Name of the method.
	@param bufsize Desired size of the output buffer, in bytes.
	@return Zero if successful, or -1 if the specified method cannot be found.

	A smaller buffer size may result in a lower latency for the first response, since we don't
	wait for as many messages to accumulate before flushing the output buffer.  On the other
	hand a larger buffer size may result in higher throughput due to lower network overhead.

	Since the buffer size is not an absolute limit, it may be set to zero, in which case each
	output transport message will contain no more than one RESULT message.

	This function has no effect on atomic methods, because all responses are sent in a single
	message anyway.  Likewise it has no effect on a method that returns only a single response.
*/
int osrfMethodSetBufferSize( const char* appName, const char* methodName, size_t bufsize ) {
	osrfMethod* method = _osrfAppFindMethod( appName, methodName );
	if( method ) {
		osrfLogInfo( OSRF_LOG_MARK,
			"Setting outbuf buffer size to %lu for method %s of application %s",
			(unsigned long) bufsize, methodName, appName );
		method->bufsize = bufsize;
		return 0;
	} else {
		osrfLogWarning( OSRF_LOG_MARK,
			"Unable to set outbuf buffer size to %lu for method %s of application %s",
			(unsigned long) bufsize, methodName, appName );
		return -1;
	}
}

/**
	@brief Register all of the system methods for this application.
	@param app Pointer to the application.

	A client can call these methods the same way it calls application-specific methods,
	but they are implemented by functions here in this module, not by functions in the
	shared object.
*/
static void register_system_methods( osrfApplication* app ) {

	if( !app ) return;

	register_method(
		app, OSRF_SYSMETHOD_INTROSPECT, NULL,
		"Return a list of methods whose names have the same initial "
		"substring as that of the provided method name PARAMS( methodNameSubstring )",
		1, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING,
		NULL );

	register_method(
		app, OSRF_SYSMETHOD_INTROSPECT, NULL,
		"Return a list of methods whose names have the same initial "
		"substring as that of the provided method name PARAMS( methodNameSubstring )",
		1, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING | OSRF_METHOD_ATOMIC,
		NULL );

	register_method(
		app, OSRF_SYSMETHOD_INTROSPECT_ALL, NULL,
		"Returns a complete list of methods. PARAMS()",
		0, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING,
		NULL );

	register_method(
		app, OSRF_SYSMETHOD_INTROSPECT_ALL, NULL,
		"Returns a complete list of methods. PARAMS()",
		0, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING | OSRF_METHOD_ATOMIC,
		NULL );

	register_method(
		app, OSRF_SYSMETHOD_ECHO, NULL,
		"Echos all data sent to the server back to the client. PARAMS([a, b, ...])",
		0, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING,
		NULL );

	register_method(
		app, OSRF_SYSMETHOD_ECHO, NULL,
		"Echos all data sent to the server back to the client. PARAMS([a, b, ...])",
		0, OSRF_METHOD_SYSTEM | OSRF_METHOD_STREAMING | OSRF_METHOD_ATOMIC,
		NULL );
}

/**
	@brief Look up an application by name in the application registry.
	@param name The name of the application.
	@return Pointer to the corresponding osrfApplication if found, or NULL if not.
*/
static inline osrfApplication* _osrfAppFindApplication( const char* name ) {
	return (osrfApplication*) osrfHashGet(_osrfAppHash, name);
}

/**
	@brief Look up a method by name for a given application.
	@param app Pointer to the osrfApplication that owns the method.
	@param methodName Name of the method to find.
	@return Pointer to the corresponding osrfMethod if found, or NULL if not.
*/
static inline osrfMethod* osrfAppFindMethod( osrfApplication* app, const char* methodName ) {
	if( !app ) return NULL;
	return (osrfMethod*) osrfHashGet( app->methods, methodName );
}

/**
	@brief Look up a method by name for an application with a given name.
	@param appName Name of the osrfApplication.
	@param methodName Name of the method to find.
	@return Pointer to the corresponding osrfMethod if found, or NULL if not.
*/
osrfMethod* _osrfAppFindMethod( const char* appName, const char* methodName ) {
	if( !appName ) return NULL;
	return osrfAppFindMethod( _osrfAppFindApplication(appName), methodName );
}

/**
	@brief Call the function that implements a specified method.
	@param appName Name of the application.
	@param methodName Name of the method.
	@param ses Pointer to the current application session.
	@param reqId The request id of the request invoking the method.
	@param params Pointer to a jsonObject encoding the parameters to the method.
	@return Zero if successful, or -1 upon failure.

	If we can't find a function corresponding to the method, or if we call it and it returns
	a negative return code, send a STATUS message to the client to report an exception.

	A return code of -1 means that the @a appName, @a methodName, or @a ses parameter was NULL.
*/
int osrfAppRunMethod( const char* appName, const char* methodName,
		osrfAppSession* ses, int reqId, jsonObject* params ) {

	if( !(appName && methodName && ses) ) return -1;

	// Find the application, and then find the method for it
	osrfApplication* app = _osrfAppFindApplication(appName);
	if( !app )
		return osrfAppRequestRespondException( ses,
				reqId, "Application not found: %s", appName );

	osrfMethod* method = osrfAppFindMethod( app, methodName );
	if( !method )
		return osrfAppRequestRespondException( ses, reqId,
				"Method [%s] not found for service %s", methodName, appName );

	#ifdef OSRF_STRICT_PARAMS
	if( method->argc > 0 ) {
		// Make sure that the client has passed at least the minimum number of arguments.
		if(!params || params->type != JSON_ARRAY || params->size < method->argc )
			return osrfAppRequestRespondException( ses, reqId,
				"Not enough params for method %s / service %s", methodName, appName );
	}
	#endif

	// Build an osrfMethodContext, which we will pass by pointer to the function.
	osrfMethodContext context;

	context.session = ses;
	context.method = method;
	context.params = params;
	context.request = reqId;
	context.responses = NULL;

	int retcode = 0;

	if( method->options & OSRF_METHOD_SYSTEM ) {
		retcode = _osrfAppRunSystemMethod(&context);

	} else {

		// Function pointer through which we will call the function dynamically
		int (*meth) (osrfMethodContext*);

		// Open the function that implements the method
		meth = dlsym(app->handle, method->symbol);

		const char* error = dlerror();
		if( error != NULL ) {
			return osrfAppRequestRespondException( ses, reqId,
				"Unable to execute method [%s] for service %s", methodName, appName );
		}

		// Run it
		retcode = meth( &context );
	}

	if(retcode < 0)
		return osrfAppRequestRespondException(
				ses, reqId, "An unknown server error occurred" );

	retcode = _osrfAppPostProcess( &context, retcode );

	if( context.responses )
		jsonObjectFree( context.responses );
	return retcode;
}

/**
	@brief Either send or enqueue a response to a client.
	@param ctx Pointer to the current method context.
	@param data Pointer to the response, in the form of a jsonObject.
	@return Zero if successful, or -1 upon error.  The only recognized errors are if either
	the @a context pointer or its method pointer is NULL.

	For an atomic method, add a copy of the response data to a cache within the method
	context, to be sent later.  Otherwise, send a RESULT message to the client, with the
	results in @a data.

	Note that, for an atomic method, this function is equivalent to osrfAppRespondComplete():
	we send the STATUS message after the method returns, and not before.
*/
int osrfAppRespond( osrfMethodContext* ctx, const jsonObject* data ) {
	return _osrfAppRespond( ctx, data, 0 );
}

/**
	@brief Either send or enqueue a response to a client, with a completion notice.
	@param context Pointer to the current method context.
	@param data Pointer to the response, in the form of a jsonObject.
	@return Zero if successful, or -1 upon error.  The only recognized errors are if either
	the @a context pointer or its method pointer is NULL.

	For an atomic method, add a copy of the response data to a cache within the method
	context, to be sent later.  Otherwise, send a RESULT message to the client, with the
	results in @a data.  Also send a STATUS message to indicate that the response is complete.

	Note that, for an atomic method, this function is equivalent to osrfAppRespond(): we
	send the STATUS message after the method returns, and not before.
*/
int osrfAppRespondComplete( osrfMethodContext* context, const jsonObject* data ) {
	return _osrfAppRespond( context, data, 1 );
}

/**
	@brief Send any response messages that have accumulated in the output buffer.
	@param ses Pointer to the current application session.
	@param outbuf Pointer to the output buffer.
	@return Zero if successful, or -1 if not.

	Used only by servers to respond to clients.
*/
static int flush_responses( osrfAppSession* ses, growing_buffer* outbuf ) {

	// Collect any inbound traffic on the socket(s).  This doesn't accomplish anything for the
	// immediate task at hand, but it may help to keep TCP from getting clogged in some cases.
	osrf_app_session_queue_wait( ses, 0, NULL );

	int rc = 0;
	if( buffer_length( outbuf ) > 0 ) {    // If there's anything to send...
		buffer_add_char( outbuf, ']' );    // Close the JSON array
		if( osrfSendTransportPayload( ses, OSRF_BUFFER_C_STR( ses->outbuf ))) {
			osrfLogError( OSRF_LOG_MARK, "Unable to flush response buffer" );
			rc = -1;
		}
	}
	buffer_reset( ses->outbuf );
	return rc;
}

/**
	@brief Add a message to an output buffer.
	@param outbuf Pointer to the output buffer.
	@param msg Pointer to the message to be added, in the form of a JSON string.

	Since the output buffer is in the form of a JSON array, prepend a left bracket to the
	first message, and a comma to subsequent ones.

	Used only by servers to respond to clients.
*/
static inline void append_msg( growing_buffer* outbuf, const char* msg ) {
	if( outbuf && msg ) {
		char prefix = buffer_length( outbuf ) > 0 ? ',' : '[';
		buffer_add_char( outbuf, prefix );
		buffer_add( outbuf, msg );
	}
}

/**
	@brief Either send or enqueue a response to a client, optionally with a completion notice.
	@param ctx Pointer to the method context.
	@param data Pointer to the response, in the form of a jsonObject.
	@param complete Boolean: if true, we will accompany the RESULT message with a STATUS
	message indicating that the response is complete.
	@return Zero if successful, or -1 upon error.

	For an atomic method, add a copy of the response data to a cache within the method
	context, to be sent later.  In this case the @a complete parameter has no effect,
	because we'll send the STATUS message later when we send the cached results.

	If the method is not atomic, translate the message into JSON and append it to a buffer,
	flushing the buffer as needed to avoid overflow.  If @a complete is true, append
	a STATUS message (as JSON) to the buffer and flush the buffer.
*/
static int _osrfAppRespond( osrfMethodContext* ctx, const jsonObject* data, int complete ) {
	if(!(ctx && ctx->method)) return -1;

	if( ctx->method->options & OSRF_METHOD_ATOMIC ) {
		osrfLogDebug( OSRF_LOG_MARK,
			"Adding responses to stash for atomic method %s", ctx->method->name );

		// If we don't already have one, create a JSON_ARRAY to serve as a cache.
		if( ctx->responses == NULL )
			ctx->responses = jsonNewObjectType( JSON_ARRAY );

		// Add a copy of the data object to the cache.
		if ( data != NULL )
			jsonObjectPush( ctx->responses, jsonObjectClone(data) );
	} else {
		osrfLogDebug( OSRF_LOG_MARK,
			"Adding responses to stash for method %s", ctx->method->name );

		if( data ) {
			// If you want to flush the intput buffers for every output message,
			// this is the place to do it.
			//osrf_app_session_queue_wait( ctx->session, 0, NULL );

			// Create an OSRF message
			osrfMessage* msg = osrf_message_init( RESULT, ctx->request, 1 );
			osrf_message_set_status_info( msg, NULL, "OK", OSRF_STATUS_OK );
			osrf_message_set_result( msg, data );

			// Serialize the OSRF message into JSON text
			char* json = jsonObjectToJSON( osrfMessageToJSON( msg ));
			osrfMessageFree( msg );

			// If the new message would overflow the buffer, flush the output buffer first
			int len_so_far = buffer_length( ctx->session->outbuf );
			if( len_so_far && (strlen( json ) + len_so_far + 3 >= ctx->method->bufsize )) {
				if( flush_responses( ctx->session, ctx->session->outbuf ))
					return -1;
			}

			// Append the JSON text to the output buffer
			append_msg( ctx->session->outbuf, json );
			free( json );
		}

		if(complete) {
			// Create a STATUS message
			osrfMessage* status_msg = osrf_message_init( STATUS, ctx->request, 1 );
			osrf_message_set_status_info( status_msg, "osrfConnectStatus", "Request Complete",
				OSRF_STATUS_COMPLETE );

			// Serialize the STATUS message into JSON text
			char* json = jsonObjectToJSON( osrfMessageToJSON( status_msg ));
			osrfMessageFree( status_msg );

			// Add the STATUS message to the output buffer.
			// It's short, so don't worry about avoiding overflow.
			append_msg( ctx->session->outbuf, json );
			free( json );

			// Flush the output buffer, sending any accumulated messages.
			if( flush_responses( ctx->session, ctx->session->outbuf ))
				return -1;
		}
	}

	return 0;
}

/**
	@brief Finish up the processing of a request.
	@param ctx Pointer to the method context.
	@param retcode The return code from the method's function.
	@return 0 if successfull, or -1 upon error.

	For an atomic method: send whatever responses we have been saving up, together with a
	STATUS message to say that we're finished.

	For a non-atomic method: if the return code from the method is greater than zero, just
	send the STATUS message.  If the return code is zero, do nothing; the method presumably
	sent the STATUS message on its own.
*/
static int _osrfAppPostProcess( osrfMethodContext* ctx, int retcode ) {
	if(!(ctx && ctx->method)) return -1;

	osrfLogDebug( OSRF_LOG_MARK, "Postprocessing method %s with retcode %d",
			ctx->method->name, retcode );

	if(ctx->responses) {
		// We have cached atomic responses to return, collected in a JSON ARRAY (we
		// haven't sent any responses yet).  Now send them all at once, followed by
		// a STATUS message to say that we're finished.
		osrfAppRequestRespondComplete( ctx->session, ctx->request, ctx->responses );

	} else {
		// We have no cached atomic responses to return, but we may have some
		// non-atomic messages waiting in the buffer.
		if( retcode > 0 )
			// Send a STATUS message to say that we're finished, and to force a
			// final flush of the buffer.
			osrfAppRespondComplete( ctx, NULL );
	}

	return 0;
}

/**
	@brief Send a STATUS message to the client, notifying it of an error.
	@param ses Pointer to the current application session.
	@param request Request ID of the request.
	@param msg A printf-style format string defining an explanatory message to be sent to
	the client.  Subsequent parameters, if any, will be formatted and inserted into the
	resulting output string.
	@return -1 if the @a ses parameter is NULL; otherwise zero.
*/
int osrfAppRequestRespondException( osrfAppSession* ses, int request, const char* msg, ... ) {
	if(!ses) return -1;
	if(!msg) msg = "";
	VA_LIST_TO_STRING(msg);
	osrfLogWarning( OSRF_LOG_MARK,  "Returning method exception with message: %s", VA_BUF );
	osrfAppSessionStatus( ses, OSRF_STATUS_NOTFOUND, "osrfMethodException", request,  VA_BUF );
	return 0;
}

/**
	@brief Introspect a specified method.
	@param ctx Pointer to the method context.
	@param method Pointer to the osrfMethod for the specified method.
	@param resp Pointer to the jsonObject into which method information will be placed.

	Treating the @a resp object as a JSON_HASH, insert entries for various bits of information
	about the specified method.
*/
static void _osrfAppSetIntrospectMethod( osrfMethodContext* ctx, const osrfMethod* method,
		jsonObject* resp ) {
	if(!(ctx && resp)) return;

	jsonObjectSetKey(resp, "api_name",  jsonNewObject(method->name));
	jsonObjectSetKey(resp, "method",    jsonNewObject(method->symbol));
	jsonObjectSetKey(resp, "service",   jsonNewObject(ctx->session->remote_service));
	jsonObjectSetKey(resp, "notes",     jsonNewObject(method->notes));
	jsonObjectSetKey(resp, "argc",      jsonNewNumberObject(method->argc));

	jsonObjectSetKey(resp, "sysmethod",
			jsonNewNumberObject( (method->options & OSRF_METHOD_SYSTEM) ? 1 : 0 ));
	jsonObjectSetKey(resp, "atomic",
			jsonNewNumberObject( (method->options & OSRF_METHOD_ATOMIC) ? 1 : 0 ));
	jsonObjectSetKey(resp, "cachable",
			jsonNewNumberObject( (method->options & OSRF_METHOD_CACHABLE) ? 1 : 0 ));
}

/**
	@brief Run the requested system method.
	@param ctx The method context.
	@return Zero if the method is run successfully; -1 if the method was not run; 1 if the
	method was run and the application code now needs to send a 'request complete' message.

	A system method is a well known method implemented here for all servers.  Instead of
	looking in the shared object, branch on the method name and call the corresponding
	function.
*/
static int _osrfAppRunSystemMethod(osrfMethodContext* ctx) {
	if( osrfMethodVerifyContext( ctx ) < 0 ) {
		osrfLogError( OSRF_LOG_MARK,  "_osrfAppRunSystemMethod: Received invalid method context" );
		return -1;
	}

	if( !strcmp(ctx->method->name, OSRF_SYSMETHOD_INTROSPECT_ALL ) ||
			!strcmp(ctx->method->name, OSRF_SYSMETHOD_INTROSPECT_ALL_ATOMIC )) {
		return osrfAppIntrospectAll(ctx);
	}

	if( !strcmp(ctx->method->name, OSRF_SYSMETHOD_INTROSPECT ) ||
			!strcmp(ctx->method->name, OSRF_SYSMETHOD_INTROSPECT_ATOMIC )) {
		return osrfAppIntrospect(ctx);
	}

	if( !strcmp(ctx->method->name, OSRF_SYSMETHOD_ECHO ) ||
			!strcmp(ctx->method->name, OSRF_SYSMETHOD_ECHO_ATOMIC )) {
		return osrfAppEcho(ctx);
	}

	osrfAppRequestRespondException( ctx->session,
			ctx->request, "System method implementation not found");

	return 0;
}

/**
	@brief Run the introspect method for a specified method or group of methods.
	@param ctx Pointer to the method context.
	@return 1 if successful, or if no search target is specified as a parameter; -1 if unable
	to find a pointer to the application.

	Traverse the list of methods, and report on each one whose name starts with the specified
	search target.  In effect, the search target ends with an implicit wild card.
*/
static int osrfAppIntrospect( osrfMethodContext* ctx ) {

	// Get the name of the method to introspect
	const char* methodSubstring = jsonObjectGetString( jsonObjectGetIndex(ctx->params, 0) );
	if( !methodSubstring )
		return 1; /* respond with no methods */

	// Get a pointer to the application
	osrfApplication* app = _osrfAppFindApplication( ctx->session->remote_service );
	if( !app )
		return -1;   // Oops, no application...

	int len = 0;
	osrfHashIterator* itr = osrfNewHashIterator(app->methods);
	osrfMethod* method;

	while( (method = osrfHashIteratorNext(itr)) ) {
		if( (len = strlen(methodSubstring)) <= strlen(method->name) ) {
			if( !strncmp( method->name, methodSubstring, len) ) {
				jsonObject* resp = jsonNewObject(NULL);
				_osrfAppSetIntrospectMethod( ctx, method, resp );
				osrfAppRespond(ctx, resp);
				jsonObjectFree(resp);
			}
		}
	}

	osrfHashIteratorFree(itr);
	return 1;
}

/**
	@brief Run the implement_all method.
	@param ctx Pointer to the method context.
	@return 1 if successful, or -1 if unable to find a pointer to the application.

	Report on all of the methods of the application.
*/
static int osrfAppIntrospectAll( osrfMethodContext* ctx ) {
	osrfApplication* app = _osrfAppFindApplication( ctx->session->remote_service );

	if(app) {
		osrfHashIterator* itr = osrfNewHashIterator(app->methods);
		osrfMethod* method;
		while( (method = osrfHashIteratorNext(itr)) ) {
			jsonObject* resp = jsonNewObject(NULL);
			_osrfAppSetIntrospectMethod( ctx, method, resp );
			osrfAppRespond(ctx, resp);
			jsonObjectFree(resp);
		}
		osrfHashIteratorFree(itr);
		return 1;
	} else
		return -1;
}

/**
	@brief Run the echo method.
	@param ctx Pointer to the method context.
	@return -1 if the method context is invalid or corrupted; otherwise 1.

	Send the client a copy of each parameter.
*/
static int osrfAppEcho( osrfMethodContext* ctx ) {
	if( osrfMethodVerifyContext( ctx ) < 0 ) {
		osrfLogError( OSRF_LOG_MARK,  "osrfAppEcho: Received invalid method context" );
		return -1;
	}

	int i;
	for( i = 0; i < ctx->params->size; i++ ) {
		const jsonObject* str = jsonObjectGetIndex(ctx->params,i);
		osrfAppRespond(ctx, str);
	}
	return 1;
}

/**
	@brief Perform a series of sanity tests on an osrfMethodContext.
	@param ctx Pointer to the osrfMethodContext to be checked.
	@return Zero if the osrfMethodContext passes all tests, or -1 if it doesn't.
*/
int osrfMethodVerifyContext( osrfMethodContext* ctx )
{
	if( !ctx ) {
		osrfLogError( OSRF_LOG_MARK,  "Context is NULL in app request" );
		return -1;
	}

	if( !ctx->session ) {
		osrfLogError( OSRF_LOG_MARK, "Session is NULL in app request" );
		return -1;
	}

	if( !ctx->method )
	{
		osrfLogError( OSRF_LOG_MARK, "Method is NULL in app request" );
		return -1;
	}

	if( ctx->method->argc ) {
		if( !ctx->params ) {
			osrfLogError( OSRF_LOG_MARK,
				"Params is NULL in app request %s", ctx->method->name );
			return -1;
		}
		if( ctx->params->type != JSON_ARRAY ) {
			osrfLogError( OSRF_LOG_MARK,
				"'params' is not a JSON array for method %s", ctx->method->name );
			return -1;
		}
	}

	if( !ctx->method->name ) {
		osrfLogError( OSRF_LOG_MARK, "Method name is NULL" );
		 return -1;
	}

	// Log the call, with the method and parameters
	char* params_str = jsonObjectToJSON( ctx->params );
	if( params_str ) {
		// params_str will at minimum be "[]"
		int i = 0;
		const char* str;
		char* method = ctx->method->name;
		int redact_params = 0;
		while( (str = osrfStringArrayGetString(log_protect_arr, i++)) ) {
			//osrfLogInternal(OSRF_LOG_MARK, "Checking for log protection [%s]", str);
			if(!strncmp(method, str, strlen(str))) {
				redact_params = 1;
				break;
			}
		}

		char* params_logged;
		if(redact_params) {
			params_logged = strdup("**PARAMS REDACTED**");
		} else {
			params_str[strlen(params_str) - 1] = '\0'; // drop the trailing ']'
			params_logged = strdup(params_str + 1);
		}
		free( params_str );
		osrfLogInfo( OSRF_LOG_MARK, "CALL: %s %s %s",
			ctx->session->remote_service, ctx->method->name, params_logged);
		free( params_logged );
	}
	return 0;
}

/**
	@brief Free an osrfMethod.
	@param name Name of the method (not used).
	@param p Void pointer pointing to the osrfMethod.

	This function is designed to be installed as a callback for an osrfHash (hence the
	unused @a name parameter and the void pointer).
*/
static void osrfMethodFree( char* name, void* p ) {
	osrfMethod* method = p;
	if( method ) {
		free( method->name );
		free( method->symbol );
		free( method->notes );
		free( method );
	}
}

/**
	@brief Free an osrfApplication
	@param name Name of the application (not used).
	@param p Void pointer pointing to the osrfApplication.

	This function is designed to be installed as a callback for an osrfHash (hence the
	unused @a name parameter and the void pointer).
*/
static void osrfAppFree( char* name, void* p ) {
	osrfApplication* app = p;
	if( app ) {
		dlclose( app->handle );
		osrfHashFree( app->methods );
		free( app );
	}
}

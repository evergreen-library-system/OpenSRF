#ifndef OSRF_APPLICATION_H
#define OSRF_APPLICATION_H

/**
	@file osrf_application.h
	@brief Routines to load and manage shared object libraries.

	Every method of a service is implemented by a C function.  In a few cases those
	functions are generic to all services.  In other cases they are loaded and executed from
	a shared object library that is specific to the application offering the service,  A
	registry maps method names to function names so that we can call the right function.

	Each such function has a similar signature:

		int method_name( osrfMethodContext* ctx );

	The return value is negative in case of an error.  A return code of zero implies that
	the method has already sent the client a STATUS message to say that it is finished.
	A return code greater than zero implies that the method has not sent such a STATUS
	message, so we need to do so after the method returns.

	Any arguments passed to the method are bundled together in a jsonObject inside the
	osrfMethodContext.

	An application's shared object may also implement any or all of three standard functions:

	- int osrfAppInitialize( void ) Called when an application is registered
	- int osrfAppChildInit( void ) Called when a server drone is spawned
	- void osrfAppChildExit( void ) Called when a server drone terminates

	osrfAppInitialize() and osrfAppChild return zero if successful, and non-zero if not.
*/

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_app_session.h>
#include <opensrf/osrf_hash.h>

#include <opensrf/osrf_json.h>
#include <stdio.h>
#include <dlfcn.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	All OpenSRF methods take the signature
	int methodName( osrfMethodContext* );
	If a negative number is returned, it means an unknown error occured and an exception
	will be returned to the client automatically.
	If a positive number is returned, it means that libopensrf should send a 'Request Complete'
	message following any messages sent by the method.
	If 0 is returned, it tells libopensrf that the method completed successfully and
	there is no need to send any further data to the client.
*/

/** This macro verifies that methods receive the correct parameters */
#define _OSRF_METHOD_VERIFY_CONTEXT(d) \
	if(!d) return -1; \
	if(!d->session) { \
		 osrfLogError( OSRF_LOG_MARK, "Session is NULL in app request" ); \
		 return -1; \
	} \
	if(!d->method) { \
		osrfLogError( OSRF_LOG_MARK, "Method is NULL in app request" ); \
		return -1; \
	} \
	if(d->method->argc) { \
		if(!d->params) { \
			osrfLogError( OSRF_LOG_MARK, "Params is NULL in app request %s", d->method->name ); \
			return -1; \
		} \
		if( d->params->type != JSON_ARRAY ) { \
			osrfLogError( OSRF_LOG_MARK, "'params' is not a JSON array for method %s", \
				d->method->name); \
			return -1; } \
	} \
	if( !d->method->name ) { \
		osrfLogError( OSRF_LOG_MARK, "Method name is NULL"); return -1; \
	}

#ifdef OSRF_LOG_PARAMS
#define OSRF_METHOD_VERIFY_CONTEXT(d) \
	_OSRF_METHOD_VERIFY_CONTEXT(d); \
	char* __j = jsonObjectToJSON(d->params); \
	if(__j) { \
		osrfLogInfo( OSRF_LOG_MARK, "CALL:\t%s %s - %s", d->session->remote_service, \
				d->method->name, __j);\
		free(__j); \
	}
#else
#define OSRF_METHOD_VERIFY_CONTEXT(d) _OSRF_METHOD_VERIFY_CONTEXT(d);
#endif

/**
	@name Method options
	@brief Macros that get OR'd together to form method options.
*/
/*@{*/
/**
	@brief Notes that the method may return more than one result.

	For a @em streaming method, we register both an atomic method and a non-atomic method.
*/
#define OSRF_METHOD_STREAMING       2
/**
	@brief  Notes that a previous result to the same call may be available in memcache.

	Before calling the registered function, a cachable method checks memcache for a previously
	determined result for the same call.  If no such result is available, it calls the
	registered function and caches the new result before returning.

	This caching is not currently implemented for C methods.
*/
#define OSRF_METHOD_CACHABLE        8
/*@}*/

typedef struct {
	char* name;                 /**< Method name. */
	char* symbol;               /**< Symbol name (function name) within the shared object. */
	char* notes;                /**< Public method documentation. */
	int argc;                   /**< The minimum number of arguments for the method. */
	//char* paramNotes;         /**< Description of the params expected for this method. */
	int options;                /**< Bit switches setting various options for this method. */
	void* userData;             /**< Opaque pointer to application-specific data. */
	size_t bufsize;             /**< How big a buffer to use for non-atomic methods */

	/*
	int sysmethod;
	int streaming;
	int atomic;
	int cachable;
	*/
} osrfMethod;

typedef struct {
	osrfAppSession* session;    /**< Pointer to the current application session. */
	osrfMethod* method;         /**< Pointer to the requested method. */
	jsonObject* params;         /**< Parameters to the method. */
	int request;                /**< Request id. */
	jsonObject* responses;      /**< Array of cached responses. */
} osrfMethodContext;

int osrfAppRegisterApplication( const char* appName, const char* soFile );

int osrfAppRegisterMethod( const char* appName, const char* methodName,
		const char* symbolName, const char* notes, int argc, int options );

int osrfAppRegisterExtendedMethod( const char* appName, const char* methodName,
		const char* symbolName, const char* notes, int argc, int options, void* );

int osrfMethodSetBufferSize( const char* appName, const char* methodName, size_t bufsize );

osrfMethod* _osrfAppFindMethod( const char* appName, const char* methodName );

int osrfAppRunMethod( const char* appName, const char* methodName,
		osrfAppSession* ses, int reqId, jsonObject* params );

int osrfAppRequestRespondException( osrfAppSession* ses, int request, const char* msg, ... );

int osrfAppRespond( osrfMethodContext* context, const jsonObject* data );

int osrfAppRespondComplete( osrfMethodContext* context, const jsonObject* data );

int osrfAppRunChildInit(const char* appname);

void osrfAppRunExitCode( void );

int osrfMethodVerifyContext( osrfMethodContext* ctx );

#ifdef __cplusplus
}
#endif

#endif

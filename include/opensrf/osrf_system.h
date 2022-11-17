/**
	@file osrf_system.h
	@brief Header for various top-level system routines.
*/

#ifndef OSRF_SYSTEM_H
#define OSRF_SYSTEM_H

#include <opensrf/transport_client.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_settings.h>
#include <opensrf/osrfConfig.h>
#include <opensrf/osrf_cache.h>

#ifdef __cplusplus
extern "C" {
#endif

void osrfSystemSetPidFile( const char* name );

int osrf_system_bootstrap_common(const char* config_file, 
    const char* contextnode, const char* appname, int is_service);

int osrf_system_bootstrap_client(const char* config_file, const char* contextnode);

int osrf_system_bootstrap_service(
    const char* config_file, const char* contextnode, const char* appname);

int osrfSystemBootstrapClientResc(const char* config_file, 
    const char* contextnode, const char* appname);

int osrfSystemBootstrap( const char* hostname, const char* configfile,
		const char* contextNode );

int osrf_system_service_ctrl(
    const char* host,    const char* config,
    const char* context, const char* piddir, 
    const char* action,  const char* service);

transport_client* osrfSystemGetTransportClient( void );

int osrf_system_disconnect_client();

int osrf_system_shutdown( void );

void osrfSystemIgnoreTransportClient();

int osrfSystemInitCache(void);

extern osrfStringArray* log_protect_arr;

#ifdef __cplusplus
}
#endif

#endif

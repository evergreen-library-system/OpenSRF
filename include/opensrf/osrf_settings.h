#ifndef OSRF_SETTINGS_H
#define OSRF_SETTINGS_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include <opensrf/log.h>
#include <opensrf/utils.h>
#include <opensrf/osrf_app_session.h>

#include <opensrf/osrf_json.h>

/**
	@file osrf_settings.h
	@brief Facility for retrieving server configuration settings.

	Look up server configuration settings from a settings server, cache them in the form of
	a jsonObject, and retrieve them on request.

	Not generally intended for client processes, unless they are also servers in their own right.
*/
#ifdef __cplusplus
extern "C" {
#endif

struct osrf_host_config_;
typedef struct osrf_host_config_ osrf_host_config;

void osrf_settings_free_host_config(osrf_host_config*);
char* osrf_settings_host_value(const char* path, ...);
jsonObject* osrf_settings_host_value_object(const char* format, ...);
int osrf_settings_retrieve(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif


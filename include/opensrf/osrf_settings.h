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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { 
	char* hostname; 
	jsonObject* config; 
} osrf_host_config;


osrf_host_config* osrf_settings_new_host_config(const char* hostname);
void osrf_settings_free_host_config(osrf_host_config*);
char* osrf_settings_host_value(const char* path, ...);
jsonObject* osrf_settings_host_value_object(const char* format, ...);
int osrf_settings_retrieve(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif


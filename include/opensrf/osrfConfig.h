/*
Copyright (C) 2005  Georgia Public Library Service 
Bill Erickson <highfalutin@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef OSRF_CONFIG_H
#define OSRF_CONFIG_H

/**
	@file osrfConfig.h
	@brief Routines for loading, storing, and searching configurations.

	A configuration file, encoded as XML, is loaded and translated into a jsonObject.  This
	object is stored in an osrfConfig, along with an optional context string which, if
	present, restricts subsequent searches to a subset of the jsonObject.

	In theory the context string could identify multiple subtrees of the total configuration.
	In practice it is used to partition a configuration file into different pieces, each piece
	to be used by a different application.

	Normally an application loads a default configuration, accessible from every linked
	module.  It is also possible, although seldom useful, to create and search a configuration
	distinct from the default configuration.
*/

#include <opensrf/xml_utils.h>
#include <opensrf/utils.h>
#include <opensrf/string_array.h>
#include <opensrf/osrf_json.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@brief Represents a configuration; normally loaded from an XML configuration file.
*/
typedef struct {
	jsonObject* config;   /**< Represents the contents of the XML configuration file. */
	char* configContext;  /**< Context string (optional). */
} osrfConfig;

osrfConfig* osrfConfigInit(const char* configFile, const char* configContext);

int osrfConfigHasDefaultConfig( void );

void osrfConfigReplaceConfig(osrfConfig* cfg, const jsonObject* obj);

void osrfConfigFree(osrfConfig* cfg);

void osrfConfigSetDefaultConfig(osrfConfig* cfg);

void osrfConfigCleanup( void );

char* osrfConfigGetValue(const osrfConfig* cfg, const char* path, ...);

jsonObject* osrfConfigGetValueObject(osrfConfig* cfg, const char* path, ...);

int osrfConfigGetValueList(const osrfConfig* cfg, osrfStringArray* arr,
		const char* path, ...);

#ifdef __cplusplus
}
#endif

#endif

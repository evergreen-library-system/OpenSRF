/* defines the currently used bootstrap config file */
#include <opensrf/osrfConfig.h>

/**
	@file osrfConfig.c
	@brief Routines for managing osrfConfigs to represent configuration information.
*/

/**
	@brief Points to the default configuration.
*/
static osrfConfig* osrfConfigDefault = NULL;

/**
	@brief Install a specified osrfConfig as the default configuration.
	@param cfg Pointer to the configuration to be stored.

	Store the passed pointer for future reference.  The calling code yields ownership of the
	associated osrfConfig.
*/
void osrfConfigSetDefaultConfig(osrfConfig* cfg) {
	if(cfg) {
		if( osrfConfigDefault )
			osrfConfigFree( osrfConfigDefault );
		osrfConfigDefault = cfg;
	}
}

/**
	@brief Free an osrfConfig.
	@param cfg Pointer to the osrfConfig to be freed.
*/
void osrfConfigFree(osrfConfig* cfg) {
	if(cfg) {
		jsonObjectFree(cfg->config);
		free(cfg->configContext);
		free(cfg);
	}
}

/**
	@brief Report whether a default configuration has been installed.
	@return Boolean: true if a default configuration is available, or false if not.
*/
int osrfConfigHasDefaultConfig( void ) {
	return ( osrfConfigDefault != NULL );
}

/**
	@brief Free the default configuration, if it exists.
*/
void osrfConfigCleanup( void ) {
	osrfConfigFree(osrfConfigDefault);
	osrfConfigDefault = NULL;
}

/**
	@brief Replace the jsonObject of an osrfConfig.
	@param cfg The osrfConfig to alter.
	@param obj The jsonObject to install in the osrfConfig.

	This is useful if you have a json object already, rather than an XML configuration file
	to parse.
*/
void osrfConfigReplaceConfig(osrfConfig* cfg, const jsonObject* obj) {
	if(!cfg || !obj) return;
	jsonObjectFree(cfg->config);
	cfg->config = jsonObjectClone(obj);
}

/**
	@brief Load an XML configuration file into a jsonObject within an osrfConfig.
	@param configFile Name of the XML configuration file.
	@param configContext (Optional) Root of a subtree in the configuration file,
	@return If successful, a pointer to the resulting osrfConfig; otherwise NULL.

	If @a configContext is not NULL, save a copy of the string to which it points.  It is a
	tag identifying one or more subtrees within the XML; subsequent searches will examine
	only matching subtrees.  Otherwise they will search the tree from the root.

	(In practice a configContext is always supplied, so that different programs can share
	the same configuration file, partitioned by context tags.)

	The calling code is responsible for freeing the returned config object by calling
	osrfConfigFree().
*/
osrfConfig* osrfConfigInit(const char* configFile, const char* configContext) {
	if(!configFile) return NULL;

	// Load XML from the configuration file

	xmlDocPtr doc = xmlParseFile(configFile);
	if(!doc) {
		osrfLogWarning( OSRF_LOG_MARK, "Unable to parse XML config file %s", configFile);
		return NULL;
	}

	// Translate it into a jsonObject
	jsonObject* json_config = xmlDocToJSON(doc);
	xmlFreeDoc(doc);

	if(!json_config ) {
		osrfLogWarning( OSRF_LOG_MARK, "xmlDocToJSON failed for config %s", configFile);
		return NULL;
	}

	// Build an osrfConfig and return it by pointer
	osrfConfig* cfg = safe_malloc(sizeof(osrfConfig));

	if( configContext )
		cfg->configContext = strdup(configContext);
	else
		cfg->configContext = NULL;

	cfg->config = json_config;

	return cfg;
}

/**
	@brief Search a configuration for a specified value.
	@param cfg (Optional) The configuration to search, or NULL for the default configuration.
	@param path A printf-style format string representing the search path.  Subsequent
	parameters, if any, are inserted into the format string to form the search path.
	@return A pointer to a newly allocated string containing the value, if found; otherwise
	NULL.

	Search for a value in the configuration, as specified by the search path.

	If cfg is NULL, search the default configuration; otherwise search the configuration
	specified.

	If the configuration includes a configContext, then prepend it to the path, like so:
	"//<configContext><path>".  Hence the path should begin with a slash to separate it from
	the context.  Search for the resulting effective path at any level within the
	configuration.

	If the configuration does @em not include a configContext, then start the search at the
	root of the configuration.  In this case the path should @em not begin with a slash.

	If the configuration contains more than one entry at locations matching the effective
	search path, return NULL.

	Return numeric values as numeric strings.

	The calling code is responsible for freeing the returned string by calling free().
*/
char* osrfConfigGetValue(const osrfConfig* cfg, const char* path, ...) {
	if(!path) return NULL;
	if(!cfg)
		cfg = osrfConfigDefault;
	if(!cfg) {
		osrfLogWarning( OSRF_LOG_MARK, "No Config object in osrfConfigGetValue()");
		return NULL;
	}

	VA_LIST_TO_STRING(path);
	jsonObject* obj;

	if(cfg->configContext) {
		jsonObject* outer_obj =
			jsonObjectFindPath(cfg->config, "//%s%s", cfg->configContext, VA_BUF);
		obj = jsonObjectExtractIndex( outer_obj, 0 );
		jsonObjectFree( outer_obj );
	} else
		obj = jsonObjectFindPath( cfg->config, VA_BUF );

	char* val = jsonObjectToSimpleString(obj);
	jsonObjectFree(obj);
	return val;
}

/**
	@brief Search for one or more subtrees of a configuration.
	@param cfg (Optional) The configuration to search, or NULL for the default configuration.
	@param path A printf-style format string representing the search path.  Subsequent
	parameters, if any, are inserted into the format string to form the search path.
	@return A pointer to a jsonObject representing a subset of the specified configuration,
	if found; otherwise NULL.

	Search for subtrees of the configuration, as specified by the search path.

	If the configuration includes a configContext, then prepend it to the path, like so:
	"//<configContext><path>".  Hence the path should begin with a slash to separate it from
	the context.  Search for the resulting effective path at any level within the
	configuration.

	If the configuration does @em not include a configContext, then start the search at the
	root of the configuration.  In this case the path should @em not begin with a slash.

	If any entries match the effective path, return copies of them all as elements of a
	jsonObject of type JSON_ARRAY.

	If no entries match the effective path, return a jsonObject of type JSON_NULL.

	The calling code is responsible for freeing the returned jsonObject by calling
	jsonObjectFree().
*/
jsonObject* osrfConfigGetValueObject(osrfConfig* cfg, const char* path, ...) {
	if(!path) return NULL;
	if(!cfg)
		cfg = osrfConfigDefault;
	if(!cfg) {
		osrfLogWarning( OSRF_LOG_MARK, "No Config object in osrfConfigGetValueObject()");
		return NULL;
	}

	VA_LIST_TO_STRING(path);
	if(cfg->configContext)
		return jsonObjectFindPath(cfg->config, "//%s%s", cfg->configContext, VA_BUF);
	else
		return jsonObjectFindPath(cfg->config, VA_BUF);
}

/**
	@brief Search for one or more values in a configuration, specified by a path.
	@param cfg (Optional) The configuration to search, or NULL for the default configuration.
	@param arr Pointer to an osrfStringArray to be populated with values.
	@param path A printf-style format string representing the search path.  Subsequent
	parameters, if any, are inserted into the format string to form the search path.
	@return The number of values loaded into the osrfStringArray.

	Search the configuration for values specified by the search path.  Load any values
	found (either strings or numbers) into an existing osrfStringArray supplied by the
	calling code.

	Make no effort to delete any strings that may already be in the osrfStringArray.
	Ordinarily the calling code should ensure that the osrfStringArray is empty when passed.

	If the configuration includes a configContext, then prepend it to the path, like so:
	"//<configContext><path>".  Hence the path should begin with a slash to separate it from
	the context.  Search for the resulting effective path at any level within the
	configuration.

	If the configuration does @em not include a configContext, then start the search at the
	root of the configuration.  In this case the path should @em not begin with a slash.
*/
int osrfConfigGetValueList(const osrfConfig* cfg, osrfStringArray* arr,
		const char* path, ...) {

	if(!arr || !path) return 0;
	if(!cfg)
		cfg = osrfConfigDefault;
	if(!cfg) {
		osrfLogWarning( OSRF_LOG_MARK, "No Config object!");
		return -1;
	}

	VA_LIST_TO_STRING(path);

	jsonObject* obj;
	if(cfg->configContext) {
		obj = jsonObjectFindPath( cfg->config, "//%s%s", cfg->configContext, VA_BUF);
	} else {
		obj = jsonObjectFindPath( cfg->config, VA_BUF);
	}

	int count = 0;

	if(obj && obj->type == JSON_ARRAY ) {

		int i;
		for( i = 0; i < obj->size; i++ ) {

			const char* val = jsonObjectGetString( jsonObjectGetIndex(obj, i) );
			if(val) {
				count++;
				osrfStringArrayAdd(arr, val);
			}
		}
	}

	jsonObjectFree(obj);
	return count;
}

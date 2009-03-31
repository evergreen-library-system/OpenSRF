/* defines the currently used bootstrap config file */
#include <opensrf/osrfConfig.h>

static osrfConfig* osrfConfigDefault = NULL;


void osrfConfigSetDefaultConfig(osrfConfig* cfg) {
	if(cfg) {
		if( osrfConfigDefault )
			osrfConfigFree( osrfConfigDefault );
		osrfConfigDefault = cfg;
	}
}

void osrfConfigFree(osrfConfig* cfg) {
	if(cfg) {
		jsonObjectFree(cfg->config);
		free(cfg->configContext);
		free(cfg);
	}	
}


int osrfConfigHasDefaultConfig() {
	return ( osrfConfigDefault != NULL );
}


void osrfConfigCleanup() { 
	osrfConfigFree(osrfConfigDefault);
	osrfConfigDefault = NULL;
}


void osrfConfigReplaceConfig(osrfConfig* cfg, const jsonObject* obj) {
	if(!cfg || !obj) return;
	jsonObjectFree(cfg->config);
	cfg->config = jsonObjectClone(obj);	
}

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

	if(configContext) cfg->configContext = strdup(configContext);
	else cfg->configContext = NULL;

	cfg->config = json_config;
	
	return cfg;
}


char* osrfConfigGetValue(const osrfConfig* cfg, const char* path, ...) {
	if(!path) return NULL;
	if(!cfg) cfg = osrfConfigDefault;
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
		obj = jsonObjectFindPath( cfg->config, VA_BUF);

	char* val = jsonObjectToSimpleString(obj);
	jsonObjectFree(obj);
	return val;
}

jsonObject* osrfConfigGetValueObject(osrfConfig* cfg, char* path, ...) {
	if(!path) return NULL;
	if(!cfg) cfg = osrfConfigDefault;
	VA_LIST_TO_STRING(path);
	if(cfg->configContext) 
        return jsonObjectFindPath(cfg->config, "//%s%s", cfg->configContext, VA_BUF);
	else
		return jsonObjectFindPath(cfg->config, VA_BUF);
}

int osrfConfigGetValueList(const osrfConfig* cfg, osrfStringArray* arr,
		const char* path, ...) {

	if(!arr || !path) return 0;
	if(!cfg) cfg = osrfConfigDefault;
	if(!cfg) { osrfLogWarning( OSRF_LOG_MARK, "No Config object!"); return -1;}

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

			char* val = jsonObjectToSimpleString(jsonObjectGetIndex(obj, i));
			if(val) {
				count++;
				osrfStringArrayAdd(arr, val);
				free(val);
			}
		}
	}

	jsonObjectFree(obj);
	return count;
}


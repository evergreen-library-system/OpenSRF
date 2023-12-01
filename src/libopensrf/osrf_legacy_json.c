/*
Copyright (C) 2006  Georgia Public Library Service 
Bill Erickson <billserickson@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/


#include <opensrf/osrf_legacy_json.h>

/* keep a copy of the length of the current json string so we don't 
 * have to calculate it in each function
 */
int current_strlen; 


jsonObject* legacy_jsonParseString( const char* string) {
	return json_parse_string( (char*) string );
}

jsonObject* legacy_jsonParseStringFmt( const char* string, ... ) {
	VA_LIST_TO_STRING(string);
	return json_parse_string( VA_BUF );
}


jsonObject* json_parse_string(char* string) {

	if(string == NULL) return NULL;

	current_strlen = strlen(string);

	if(current_strlen == 0) 
		return NULL;

	unsigned long index = 0;

	json_eat_ws(string, &index, 1, current_strlen); /* remove leading whitespace */
	if(index == current_strlen) return NULL;

	jsonObject* obj = jsonNewObject(NULL);

	int status = _json_parse_string(string, &index, obj, current_strlen);
	if(!status) return obj;

	if(status == -2) {
		jsonObjectFree(obj);
		return NULL;
	}

	return NULL;
}


int _json_parse_string(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {
	if( !string || !index || *index >= current_strlen) return -2;

	int status = 0; /* return code from parsing routines */
	char* classname = NULL; /* object class hint */
	json_eat_ws(string, index, 1, current_strlen); /* remove leading whitespace */

	char c = string[*index];

	/* remove any leading comments */
	if( c == '/' ) { 

		while(1) {
			(*index)++; /* move to second comment char */
			status = json_eat_comment(string, index, &classname, 1, current_strlen);
			if(status) return status;

			json_eat_ws(string, index, 1, current_strlen);
			c = string[*index];
			if(c != '/')
				break;
		}
	}

	json_eat_ws(string, index, 1, current_strlen); /* remove leading whitespace */

	if(*index >= current_strlen)
		return -2;

	switch(c) {
				
		/* json string */
		case '"': 
			(*index)++;
			status = json_parse_json_string(string, index, obj, current_strlen);
			break;

		/* json array */
		case '[':
			(*index)++;
			status = json_parse_json_array(string, index, obj, current_strlen);
			break;

		/* json object */
		case '{':
			(*index)++;
			status = json_parse_json_object(string, index, obj, current_strlen);
			break;

		/* NULL */
		case 'n':
		case 'N':
			status = json_parse_json_null(string, index, obj, current_strlen);
			break;
			

		/* true, false */
		case 'f':
		case 'F':
		case 't':
		case 'T':
			status = json_parse_json_bool(string, index, obj, current_strlen);
			break;

		default:
			if(isdigit(c) || c == '.' || c == '-') { /* are we a number? */
				status = json_parse_json_number(string, index, obj, current_strlen);
				if(status) return status;
				break;
			}

			(*index)--;
			/* we should never get here */
			return json_handle_error(string, index, "_json_parse_string() final switch clause");
	}	

	if(status) return status;

	json_eat_ws(string, index, 1, current_strlen);

	if( *index < current_strlen ) {
		/* remove any trailing comments */
		c = string[*index];
		if( c == '/' ) { 
			(*index)++;
			status = json_eat_comment(string, index, NULL, 0, current_strlen);
			if(status) return status;
		}
	}

	if(classname){
		jsonObjectSetClass(obj, classname);
		free(classname);
	}

	return 0;
}


int json_parse_json_null(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {

	if(*index >= (current_strlen - 3)) {
		return json_handle_error(string, index, 
			"_parse_json_null(): invalid null" );
	}

	if(!strncasecmp(string + (*index), "null", 4)) {
		(*index) += 4;
		obj->type = JSON_NULL;
		return 0;
	} else {
		return json_handle_error(string, index,
			"_parse_json_null(): invalid null" );
	}
}

/* should be at the first character of the bool at this point */
int json_parse_json_bool(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {
	if( ! string || ! obj || *index >= current_strlen ) return -1;

	char* ret = "json_parse_json_bool(): truncated bool";

	if( *index >= (current_strlen - 5))
		return json_handle_error(string, index, ret);
	
	if(!strncasecmp( string + (*index), "false", 5)) {
		(*index) += 5;
		obj->value.b = 0;
		obj->type = JSON_BOOL;
		return 0;
	}

	if( *index >= (current_strlen - 4))
		return json_handle_error(string, index, ret);

	if(!strncasecmp( string + (*index), "true", 4)) {
		(*index) += 4;
		obj->value.b = 1;
		obj->type = JSON_BOOL;
		return 0;
	}

	return json_handle_error(string, index, ret);
}


/* expecting the first character of the number */
int json_parse_json_number(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {
	if( ! string || ! obj || *index >= current_strlen ) return -1;

	growing_buffer* buf = osrf_buffer_init(64);
	char c = string[*index];

	int done = 0;
	int dot_seen = 0;

	/* negative number? */
	if(c == '-') { osrf_buffer_add(buf, "-"); (*index)++; }

	c = string[*index];

	while(*index < current_strlen) {

		if(isdigit(c)) {
			osrf_buffer_add_char(buf, c);
		}

		else if( c == '.' ) {
			if(dot_seen) {
				osrf_buffer_free(buf);
				return json_handle_error(string, index, 
					"json_parse_json_number(): malformed json number");
			}
			dot_seen = 1;
			osrf_buffer_add_char(buf, c);
		} else {
			done = 1; break;
		}

		(*index)++;
		c = string[*index];
		if(done) break;
	}

	obj->type = JSON_NUMBER;
	obj->value.s = osrf_buffer_release(buf);
	return 0;
}

/* index should point to the character directly following the '['.  when done
 * index will point to the character directly following the ']' character
 */
int json_parse_json_array(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {

	if( ! string || ! obj || ! index || *index >= current_strlen ) return -1;

	int status = 0;
	int in_parse = 0; /* true if this array already contains one item */
	obj->type = JSON_ARRAY;
	int set = 0;
	int done = 0;

	while(*index < current_strlen) {

		json_eat_ws(string, index, 1, current_strlen);

		if(string[*index] == ']') {
			(*index)++;
			done = 1;
			break;
		}

		if(in_parse) {
			json_eat_ws(string, index, 1, current_strlen);
			if(string[*index] != ',') {
				return json_handle_error(string, index,
					"json_parse_json_array(): array item not followed by a ','");
			}
			(*index)++;
			json_eat_ws(string, index, 1, current_strlen);
		}

		jsonObject* item = jsonNewObject(NULL);

		#ifndef STRICT_JSON_READ
		if(*index < current_strlen) {
			if(string[*index] == ',' || string[*index] == ']') {
				status = 0;
				set = 1;
			}
		}
		if(!set) status = _json_parse_string(string, index, item, current_strlen);

		#else
		status = _json_parse_string(string, index, item, current_strlen);
		#endif

		if(status) { jsonObjectFree(item); return status; }
		jsonObjectPush(obj, item);
		in_parse = 1;
		set = 0;
	}

	if(!done)
		return json_handle_error(string, index,
			"json_parse_json_array(): array not closed");

	return 0;
}


/* index should point to the character directly following the '{'.  when done
 * index will point to the character directly following the '}'
 */
int json_parse_json_object(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {
	if( ! string || !obj || ! index || *index >= current_strlen ) return -1;

	obj->type = JSON_HASH;
	int status;
	int in_parse = 0; /* true if we've already added one item to this object */
	int set = 0;
	int done = 0;

	while(*index < current_strlen) {

		json_eat_ws(string, index, 1, current_strlen);

		if(string[*index] == '}') {
			(*index)++;
			done = 1;
			break;
		}

		if(in_parse) {
			if(string[*index] != ',') {
				return json_handle_error(string, index,
					"json_parse_json_object(): object missing ',' between elements" );
			}
			(*index)++;
			json_eat_ws(string, index, 1, current_strlen);
		}

		/* first we grab the hash key */
		jsonObject* key_obj = jsonNewObject(NULL);
		status = _json_parse_string(string, index, key_obj, current_strlen);
		if(status) return status;

		if(key_obj->type != JSON_STRING) {
			return json_handle_error(string, index, 
				"_json_parse_json_object(): hash key not a string");
		}

		char* key = key_obj->value.s;

		json_eat_ws(string, index, 1, current_strlen);

		if(string[*index] != ':') {
			return json_handle_error(string, index, 
				"json_parse_json_object(): hash key not followed by ':' character");
		}

		(*index)++;

		/* now grab the value object */
		json_eat_ws(string, index, 1, current_strlen);
		jsonObject* value_obj = jsonNewObject(NULL);

#ifndef STRICT_JSON_READ
		if(*index < current_strlen) {
			if(string[*index] == ',' || string[*index] == '}') {
				status = 0;
				set = 1;
			}
		}
		if(!set)
			status = _json_parse_string(string, index, value_obj, current_strlen);

#else
		 status = _json_parse_string(string, index, value_obj, current_strlen);
#endif

		if(status) return status;

		/* put the data into the object and continue */
		jsonObjectSetKey(obj, key, value_obj);
		jsonObjectFree(key_obj);
		in_parse = 1;
		set = 0;
	}

	if(!done)
		return json_handle_error(string, index,
			"json_parse_json_object(): object not closed");

	return 0;
}



/* when done, index will point to the character after the closing quote */
int json_parse_json_string(char* string, unsigned long* index, jsonObject* obj, int current_strlen) {
	if( ! string || ! index || *index >= current_strlen ) return -1;

	int in_escape = 0;	
	int done = 0;
	growing_buffer* buf = osrf_buffer_init(64);

	while(*index < current_strlen) {

		char c = string[*index]; 

		switch(c) {

			case '\\':
				if(in_escape) {
					osrf_buffer_add(buf, "\\");
					in_escape = 0;
				} else 
					in_escape = 1;
				break;

			case '"':
				if(in_escape) {
					osrf_buffer_add(buf, "\"");
					in_escape = 0;
				} else 
					done = 1;
				break;

			case 't':
				if(in_escape) {
					osrf_buffer_add(buf,"\t");
					in_escape = 0;
				} else 
					osrf_buffer_add_char(buf, c);
				break;

			case 'b':
				if(in_escape) {
					osrf_buffer_add(buf,"\b");
					in_escape = 0;
				} else 
					osrf_buffer_add_char(buf, c);
				break;

			case 'f':
				if(in_escape) {
					osrf_buffer_add(buf,"\f");
					in_escape = 0;
				} else 
					osrf_buffer_add_char(buf, c);
				break;

			case 'r':
				if(in_escape) {
					osrf_buffer_add(buf,"\r");
					in_escape = 0;
				} else 
					osrf_buffer_add_char(buf, c);
				break;

			case 'n':
				if(in_escape) {
					osrf_buffer_add(buf,"\n");
					in_escape = 0;
				} else 
					osrf_buffer_add_char(buf, c);
				break;

			case 'u':
				if(in_escape) {
					(*index)++;

					if(*index >= (current_strlen - 4)) {
						osrf_buffer_free(buf);
						return json_handle_error(string, index,
							"json_parse_json_string(): truncated escaped unicode"); }

					/* ----------------------------------------------------------------------- */
					/* ----------------------------------------------------------------------- */
					/* The following chunk was borrowed with permission from 
						json-c http://oss.metaparadigm.com/json-c/ */
					unsigned char utf_out[3] = { '\0', '\0', '\0' };

					#define hexdigit(x) ( ((x) <= '9') ? (x) - '0' : ((x) & 7) + 9)

					unsigned int ucs_char =
						(hexdigit(string[*index] ) << 12) +
						(hexdigit(string[*index + 1]) << 8) +
						(hexdigit(string[*index + 2]) << 4) +
						hexdigit(string[*index + 3]);
	
					if (ucs_char < 0x80) {
						utf_out[0] = ucs_char;
						osrf_buffer_add(buf, (char*) utf_out);

					} else if (ucs_char < 0x800) {
						utf_out[0] = 0xc0 | (ucs_char >> 6);
						utf_out[1] = 0x80 | (ucs_char & 0x3f);
						osrf_buffer_add(buf, (char*) utf_out);

					} else {
						utf_out[0] = 0xe0 | (ucs_char >> 12);
						utf_out[1] = 0x80 | ((ucs_char >> 6) & 0x3f);
						utf_out[2] = 0x80 | (ucs_char & 0x3f);
						osrf_buffer_add(buf, (char*) utf_out);
					}
					/* ----------------------------------------------------------------------- */
					/* ----------------------------------------------------------------------- */

					(*index) += 3;
					in_escape = 0;

				} else {

					osrf_buffer_add_char(buf, c);
				}

				break;

			default:
				osrf_buffer_add_char(buf, c);
		}

		(*index)++;
		if(done) break;
	}

	jsonObjectSetString(obj, buf->buf);
	osrf_buffer_free(buf);
	return 0;
}


void json_eat_ws(char* string, unsigned long* index, int eat_all, int current_strlen) {
	if( ! string || ! index ) return;
	if(*index >= current_strlen)
		return;

	if( eat_all ) { /* removes newlines, etc */
		while(string[*index] == ' ' 	|| 
				string[*index] == '\n' 	||
				string[*index] == '\t') 
			(*index)++;
	}

	else	
		while(string[*index] == ' ') (*index)++;
}


/* index should be at the '*' character at the beginning of the comment.
 * when done, index will point to the first character after the final /
 */
int json_eat_comment(char* string, unsigned long* index, char** buffer, int parse_class, int current_strlen) {
	if( ! string || ! index || *index >= current_strlen ) return -1;
	

	if(string[*index] != '*' && string[*index] != '/' )
		return json_handle_error(string, index, 
			"json_eat_comment(): invalid character after /");

	/* chop out any // style comments */
	if(string[*index] == '/') {
		(*index)++;
		char c = string[*index];
		while(*index < current_strlen) {
			(*index)++;
			if(c == '\n') 
				return 0;
			c = string[*index];
		}
		return 0;
	}

	(*index)++;

	int on_star			= 0; /* true if we just saw a '*' character */

	/* we're just past the '*' */
	if(!parse_class) { /* we're not concerned with class hints */
		while(*index < current_strlen) {
			if(string[*index] == '/') {
				if(on_star) {
					(*index)++;
					return 0;
				}
			}

			if(string[*index] == '*') on_star = 1;
			else on_star = 0;

			(*index)++;
		}
		return 0;
	}



	growing_buffer* buf = osrf_buffer_init(64);

	int first_dash		= 0;
	int second_dash	= 0;

	int in_hint			= 0;
	int done				= 0;

	/*--S hint--*/   /* <-- Hints  look like this */
	/*--E hint--*/

	while(*index < current_strlen) {
		char c = string[*index];

		switch(c) {

			case '-':
				on_star = 0;
				if(first_dash)	second_dash = 1;
				else						first_dash = 1;
				break;

			case 'S':
				on_star = 0;
				if(second_dash && !in_hint) {
					(*index)++;
					json_eat_ws(string, index, 1, current_strlen);
					(*index)--; /* this will get incremented at the bottom of the loop */
					in_hint = 1;
					break;
				} 

				if(second_dash && in_hint) {
					osrf_buffer_add_char(buf, c);
					break;
				}

			case 'E':
				on_star = 0;
				if(second_dash && !in_hint) {
					(*index)++;
					json_eat_ws(string, index, 1, current_strlen);
					(*index)--; /* this will get incremented at the bottom of the loop */
					in_hint = 1;
					break;
				}

				if(second_dash && in_hint) {
					osrf_buffer_add_char(buf, c);
					break;
				}

			case '*':
				on_star = 1;
				break;

			case '/':
				if(on_star) 
					done = 1;
				else
				on_star = 0;
				break;

			default:
				on_star = 0;
				if(in_hint)
					osrf_buffer_add_char(buf, c);
		}

		(*index)++;
		if(done) break;
	}

	if( buf->n_used > 0 && buffer)
		*buffer = osrf_buffer_data(buf);

	osrf_buffer_free(buf);
	return 0;
}

int json_handle_error(char* string, unsigned long* index, char* err_msg) {

	char buf[60];
	osrf_clearbuf(buf, sizeof(buf));

	if(*index > 30)
		strncpy( buf, string + (*index - 30), sizeof(buf) - 1 );
	else
		strncpy( buf, string, sizeof(buf) - 1 );

	buf[ sizeof(buf) - 1 ] = '\0';

	fprintf(stderr, 
			"\nError parsing json string at charracter %c "
			"(code %d) and index %ld\nString length: %d\nMsg:\t%s\nNear:\t%s\nFull String:\t%s\n", 
			string[*index], string[*index], *index, current_strlen, err_msg, buf, string );

	return -1;
}

char* legacy_jsonObjectToJSON( const jsonObject* obj ) {

	if(obj == NULL) return strdup("null");

	growing_buffer* buf = osrf_buffer_init(64);

	/* add class hints if we have a class name */
	if(obj->classname) {
		osrf_buffer_add(buf,"/*--S ");
		osrf_buffer_add(buf,obj->classname);
		osrf_buffer_add(buf, "--*/");
	}

	switch( obj->type ) {

		case JSON_BOOL: 
			if(obj->value.b) osrf_buffer_add(buf, "true"); 
			else osrf_buffer_add(buf, "false"); 
			break;

		case JSON_NUMBER: {
			osrf_buffer_add(buf, obj->value.s);
			break;
		}

		case JSON_NULL:
			osrf_buffer_add(buf, "null");
			break;

		case JSON_STRING:
			osrf_buffer_add(buf, "\"");
			char* data = obj->value.s;
			int len = strlen(data);
			
			char* output = uescape(data, len, 1);
			osrf_buffer_add(buf, output);
			free(output);
			osrf_buffer_add(buf, "\"");
			break;

		case JSON_ARRAY:
			osrf_buffer_add(buf, "[");
			int i;
			for( i = 0; i!= obj->size; i++ ) {
				const jsonObject* x = jsonObjectGetIndex(obj,i);
				char* data = legacy_jsonObjectToJSON(x);
				osrf_buffer_add(buf, data);
				free(data);
				if(i != obj->size - 1)
					osrf_buffer_add(buf, ",");
			}
			osrf_buffer_add(buf, "]");
			break;	

		case JSON_HASH:
	
			osrf_buffer_add(buf, "{");
			jsonIterator* itr = jsonNewIterator(obj);
			jsonObject* tmp;
	
			while( (tmp = jsonIteratorNext(itr)) ) {

				osrf_buffer_add(buf, "\"");

				const char* key = itr->key;
				int len = strlen(key);
				char* output = uescape(key, len, 1);
				osrf_buffer_add(buf, output);
				free(output);

				osrf_buffer_add(buf, "\":");
				char* data =  legacy_jsonObjectToJSON(tmp);
				osrf_buffer_add(buf, data);
				if(jsonIteratorHasNext(itr))
					osrf_buffer_add(buf, ",");
				free(data);
			}

			jsonIteratorFree(itr);
			osrf_buffer_add(buf, "}");
			break;
		
			default:
				fprintf(stderr, "Unknown object type %d\n", obj->type);
				break;
				
	}

	/* close out the object hint */
	if(obj->classname) {
		osrf_buffer_add(buf, "/*--E ");
		osrf_buffer_add(buf, obj->classname);
		osrf_buffer_add(buf, "--*/");
	}

	char* data = osrf_buffer_data(buf);
	osrf_buffer_free(buf);
	return data;
}



static jsonObjectNode* makeNode(jsonObject* obj, unsigned long index, char* key) {
    jsonObjectNode* node = safe_malloc(sizeof(jsonObjectNode));
    node->item = obj;
    node->index = index;
    node->key = key;
    return node;
}

jsonObjectIterator* jsonNewObjectIterator(const jsonObject* obj) {
	if(!obj) return NULL;
	jsonObjectIterator* itr = safe_malloc(sizeof(jsonObjectIterator));
    itr->iterator = jsonNewIterator(obj);
	itr->obj = obj;
    itr->done = 0;
    itr->current = NULL;
	return itr;
}

jsonObjectNode* jsonObjectIteratorNext( jsonObjectIterator* itr ) {
	if(itr == NULL || itr->done) return NULL;

    if(itr->current) free(itr->current);
    jsonObject* next = jsonIteratorNext(itr->iterator);
    if(next == NULL) {
        itr->current = NULL;
        itr->done = 1;
        return NULL;
    }
    /* Lp 1243841: Remove compiler const warning. */
    char *k = (char *) itr->iterator->key;
    itr->current = makeNode(next, itr->iterator->index, k);
    return itr->current;
}

void jsonObjectIteratorFree(jsonObjectIterator* iter) { 
    if(iter->current) free(iter->current);
    jsonIteratorFree(iter->iterator);
	free(iter);
}

int jsonObjectIteratorHasNext(const jsonObjectIterator* itr) {
	return (itr && itr->current);
}



/*
 * Basic JSON test module.  Needs more strenous tests....
 *
 */
#include <stdio.h>
#include <opensrf/osrf_json.h>

static void speedTest();


int main(int argc, char* argv[]) {
    /* XXX add support for command line test type specification */
    speedTest(); 
    return 0; 
}



static void speedTest() {

    /* creates a giant json object, generating JSON strings
     * of subobjects as it goes. */

    int i,k;
    int count = 50;
    char buf[16];
    char* jsonString;

    jsonObject* array;
    jsonObject* dupe;
    jsonObject* hash = jsonNewObject(NULL);

    for(i = 0; i < count; i++) {
        
        snprintf(buf, sizeof(buf), "key_%d", i);

        array = jsonNewObject(NULL);
        for(k = 0; k < count + i; k++) {
            jsonObjectPush(array, jsonNewNumberObject(k));
            jsonObjectPush(array, jsonNewObject(NULL));
            jsonObjectPush(array, jsonNewObjectFmt("str %d-%d", i, k));
        }
        jsonObjectSetKey(hash, buf, array);

        jsonString = jsonObjectToJSON(hash);
        printf("%s\n\n", jsonString);
        dupe = jsonParseString(jsonString);

        jsonObjectFree(dupe);
        free(jsonString);
    }

    jsonObjectFree(hash);
}


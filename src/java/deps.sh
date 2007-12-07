# ----------------------------------------------------------------
# Utility script for fetching the OpenSRF Java dependencies
# ----------------------------------------------------------------

. deps.inc
STAX=stax-api-1.0.1.jar
WSTX=wstx-lgpl-3.2.1.jar
MEMCACHE=java_memcached-release_1.5.1.jar
JSON=json.zip
JSON_ZIP=json.zip

STAX_URL=http://woodstox.codehaus.org/$STAX
WSTX_URL=http://woodstox.codehaus.org/3.2.1/$WSTX
MEMCACHE_URL=http://img.whalin.com/memcached/jdk5/standard/$MEMCACHE
JSON_URL=http://www.json.org/java/$JSON

JAVAC="javac -J-Xmx256m"
JAVA="java -Xmx256m"

mkdir -p deps
if [ ! -f deps/$STAX ]; then wget $STAX_URL -O deps/$STAX; fi 
if [ ! -f deps/$WSTX ]; then wget $WSTX_URL -O deps/$WSTX; fi
if [ ! -f deps/$MEMCACHE ]; then wget $MEMCACHE_URL -O deps/$MEMCACHE; fi
if [ ! -f deps/$JSON ]; then 
    mkdir -p deps 
    cd deps 
    wget "$JSON_URL"
    unzip $JSON && $JAVAC org/json/*.java; 
    jar cf json.jar org
fi


if [ -n "$INSTALLDIR" ]; then
    cp deps/*.jar "$INSTALLDIR"/;
else
    echo ""
    echo "if you provide an INSTALLDIR setting, the script will go ahead and copy the jars into place"
    echo "example: INSTALLDIR=/path/to/java $0"
    echo ""
fi

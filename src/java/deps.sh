# ----------------------------------------------------------------
# Utility script for fetching the OpenSRF Java dependencies
# ----------------------------------------------------------------

MEMCACHE=java_memcached-release_2.0.1.jar
MEMCACHE_URL=http://img.whalin.com/memcached/jdk6/standard/$MEMCACHE
JSON_URL=https://github.com/douglascrockford/JSON-java/zipball/master
JSON_ZIP=json.zip
JSON_JAR=json.jar

mkdir -p deps
if [ ! -f deps/$MEMCACHE ]; then wget $MEMCACHE_URL -O deps/$MEMCACHE; fi
if [ ! -f deps/$JSON_JAR ]; then 
    cd deps 
    wget "$JSON_URL" -O $JSON_ZIP
    unzip $JSON_ZIP
    mkdir -p org/json/
    cp douglascrockford*/*.java org/json/
    javac org/json/*.java
    jar cf $JSON_JAR org/json/*.class
fi


if [ -n "$INSTALLDIR" ]; then
    cp deps/*.jar "$INSTALLDIR"/;
else
    echo ""
    echo "if you provide an INSTALLDIR setting, the script will go ahead and copy the jars into place"
    echo "example: INSTALLDIR=/path/to/java $0"
    echo ""
fi

echo ""
echo "To compile OpenSRF java:"
echo ""
echo "CLASSPATH=deps/$MEMCACHE:deps/$JSON_JAR javac org/opensrf/*.java org/opensrf/net/xmpp/*.java org/opensrf/util/*.java org/opensrf/test/*.java"
echo ""

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <malloc.h>
#include "opensrf/utils.h"
#include "opensrf/osrf_json.h"

struct timeval diff_timeval( const struct timeval * begin,
	const struct timeval * end );

static const char sample_json[] =
	"{\"menu\": {\"id\": \"file\", \"value\": \"File\","
	"\"popup\": { \"menuitem\": [ {\"value\": \"New\", "
	"\"onclick\": \"CreateNewDoc()\"},"
	"{\"value\": \"Open\", \"onclick\": \"OpenDoc()\"}, "
	"{\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}]}}}";

int main( void ) {
	int rc = 0;

	struct timezone tz = { 240, 1 };
	struct timeval begin_timeval;
	struct timeval end_timeval;

	gettimeofday( &begin_timeval, &tz );

	long i;
	jsonObject * pObj = NULL;

	for( i = 10000000; i; --i )
	{
//		pObj = jsonParse( sample_json );
		pObj = jsonNewObject( NULL );
		jsonObject * p1 = jsonNewObject( NULL );
		jsonObject * p2 = jsonNewObject( NULL );
		jsonObjectFree( p1 );
		jsonObjectFree( p2 );
		jsonObjectFree( pObj );
	}

	jsonObjectFreeUnused();
	
	gettimeofday( &end_timeval, &tz );

	struct timeval elapsed = diff_timeval( &begin_timeval, &end_timeval );

	printf( "Elapsed time: %ld seconds, %ld microseconds\n",
			(long) elapsed.tv_sec, (long) elapsed.tv_usec );

	struct rlimit rlim;
	if( getrlimit( RLIMIT_DATA, &rlim ) )
		printf( "Error calling getrlimit\n" );
	else
		printf( "Address space: %lu\n", (unsigned long) rlim.rlim_cur );

	malloc_stats();

	return rc;
}

struct timeval diff_timeval( const struct timeval * begin, const struct timeval * end )
{
	struct timeval diff;

	diff.tv_sec = end->tv_sec - begin->tv_sec;
	diff.tv_usec = end->tv_usec - begin->tv_usec;

	if( diff.tv_usec < 0 )
	{
		diff.tv_usec += 1000000;
		--diff.tv_sec;
	}

	return diff;

}

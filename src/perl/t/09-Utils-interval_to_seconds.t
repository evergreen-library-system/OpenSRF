#!perl 


use DateTime::Format::ISO8601;
use Test::More tests => 10;

BEGIN {
	use_ok( 'OpenSRF::Utils' );
}

is (OpenSRF::Utils::interval_to_seconds('1 second'), 1);
is (OpenSRF::Utils::interval_to_seconds('1 minute'), 60);
is (OpenSRF::Utils::interval_to_seconds('1 hour'), 3600);
is (OpenSRF::Utils::interval_to_seconds('1 day'), 86400);
is (OpenSRF::Utils::interval_to_seconds('1 week'), 604800);
is (OpenSRF::Utils::interval_to_seconds('1 month'), 2628000);

# With context, no DST change
is (OpenSRF::Utils::interval_to_seconds('1 month',
    DateTime::Format::ISO8601->new->parse_datetime('2017-02-04T23:59:59-04')), 2419200);

# With context, with DST change
is (OpenSRF::Utils::interval_to_seconds('1 month',
    DateTime::Format::ISO8601->new->parse_datetime('2017-02-14T23:59:59-04')), 2415600);

is (OpenSRF::Utils::interval_to_seconds('1 year'), 31536000);
is (OpenSRF::Utils::interval_to_seconds('1 year 1 second'), 31536001);

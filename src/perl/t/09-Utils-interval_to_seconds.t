#!perl -T

use Test::More tests => 9;

BEGIN {
	use_ok( 'OpenSRF::Utils' );
}

is (OpenSRF::Utils::interval_to_seconds('1 second'), 1);
is (OpenSRF::Utils::interval_to_seconds('1 minute'), 60);
is (OpenSRF::Utils::interval_to_seconds('1 hour'), 3600);
is (OpenSRF::Utils::interval_to_seconds('1 day'), 86400);
is (OpenSRF::Utils::interval_to_seconds('1 week'), 604800);
is (OpenSRF::Utils::interval_to_seconds('1 month'), 2628000);
is (OpenSRF::Utils::interval_to_seconds('1 year'), 31536000);
is (OpenSRF::Utils::interval_to_seconds('1 year 1 second'), 31536001);

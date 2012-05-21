#!perl -T

use Test::More tests => 2;

BEGIN {
	use_ok( 'OpenSRF::Utils::Cache' );
}

is (OpenSRF::Utils::Cache::_clean_cache_key('ac.jacket.large.9780415590211 (hbk.)'), 'ac.jacket.large.9780415590211(hbk.)');

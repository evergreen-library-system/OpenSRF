#!perl -T

use Test::More tests => 1;

BEGIN {
	use_ok( 'OpenSRF' );
}

diag( "Testing OpenSRF $OpenSRF::VERSION, Perl $], $^X" );

#!perl -T

use Test::More tests => 4;

BEGIN {
	use_ok( 'OpenSRF::Application' );
}

use_ok( 'OpenSRF::Application::Client' );
use_ok( 'OpenSRF::Application::Persist' );
use_ok( 'OpenSRF::Application::Settings' );

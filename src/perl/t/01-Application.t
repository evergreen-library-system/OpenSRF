#!perl -T

use Test::More tests => 9;

BEGIN {
	use_ok( 'OpenSRF::Application' );
}

use_ok( 'OpenSRF::Application::Client' );
use_ok( 'OpenSRF::Application::Persist' );
use_ok( 'OpenSRF::Application::Settings' );
use_ok( 'OpenSRF::Application::Validator' );
use_ok( 'OpenSRF::Application::Validator::Base' );
use_ok( 'OpenSRF::Application::Validator::EmailAddress::DNS' );
use_ok( 'OpenSRF::Application::Validator::EmailAddress::Regex' );
use_ok( 'OpenSRF::Application::Validator::Invalid' );

#!perl -T

use Test::More tests => 8;

BEGIN {
	use_ok( 'OpenSRF::Utils' );
}

use_ok( 'OpenSRF::Utils::Cache' );
use_ok( 'OpenSRF::Utils::Config' );
use_ok( 'OpenSRF::Utils::JSON' );
use_ok( 'OpenSRF::Utils::Logger' );
use_ok( 'OpenSRF::Utils::LogServer' );
use_ok( 'OpenSRF::Utils::SettingsClient' );
use_ok( 'OpenSRF::Utils::SettingsParser' );

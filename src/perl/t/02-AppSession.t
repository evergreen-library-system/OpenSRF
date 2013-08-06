#!perl -T

use strict;
use warnings;
use Test::More tests => 3;

BEGIN {
	use_ok( 'OpenSRF::AppSession' );
}

my $locale = OpenSRF::AppSession->default_locale('fr-CA');
is($locale, 'fr-CA', 'got back the default locale we set');
$locale = OpenSRF::AppSession->reset_locale();
is($locale, 'en-US', 'got back en-US after reset of default locale');

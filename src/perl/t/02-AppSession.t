#!perl -T

use strict;
use warnings;
use Test::More tests => 4;

BEGIN {
	use_ok( 'OpenSRF::AppSession' );
}

my $locale = OpenSRF::AppSession->default_locale('fr-CA');
is($locale, 'fr-CA', 'got back the default locale we set');
$locale = OpenSRF::AppSession->reset_locale();
is($locale, 'en-US', 'got back en-US after reset of default locale');

my $subreq = OpenSRF::AppSubrequest->new();
$subreq->respond('a');
$subreq->respond('b');
$subreq->respond_complete();
$subreq->respond('c');
my @responses = $subreq->responses();
is_deeply(\@responses, ['a', 'b'], 'further responses ignored after respond_complete() is called');

#!perl -T

use Test::More tests => 2;

BEGIN {
	use_ok( 'OpenSRF::AppSession' );
}

my $subreq = OpenSRF::AppSubrequest->new();
$subreq->respond('a');
$subreq->respond('b');
$subreq->respond_complete();
$subreq->respond('c');
my @responses = $subreq->responses();
is_deeply(\@responses, ['a', 'b'], 'further responses ignored after respond_complete() is called');

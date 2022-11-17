#!perl -T

use Test::More tests => 7;

BEGIN {
	use_ok( 'OpenSRF::Transport' );
}

use_ok( 'OpenSRF::Transport::Listener' );
use_ok( 'OpenSRF::Transport::PeerHandle' );
use_ok( 'OpenSRF::Transport::Redis::Client' );
use_ok( 'OpenSRF::Transport::Redis::Message' );
use_ok( 'OpenSRF::Transport::Redis::PeerConnection' );
use_ok( 'OpenSRF::Transport::Redis::BusConnection' );

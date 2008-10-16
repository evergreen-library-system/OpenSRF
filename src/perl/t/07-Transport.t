#!perl -T

use Test::More tests => 10;

BEGIN {
	use_ok( 'OpenSRF::Transport' );
}

use_ok( 'OpenSRF::Transport::Listener' );
use_ok( 'OpenSRF::Transport::PeerHandle' );
use_ok( 'OpenSRF::Transport::SlimJabber' );
use_ok( 'OpenSRF::Transport::SlimJabber::Client' );
use_ok( 'OpenSRF::Transport::SlimJabber::Inbound' );
use_ok( 'OpenSRF::Transport::SlimJabber::MessageWrapper' );
use_ok( 'OpenSRF::Transport::SlimJabber::PeerConnection' );
use_ok( 'OpenSRF::Transport::SlimJabber::XMPPMessage' );
use_ok( 'OpenSRF::Transport::SlimJabber::XMPPReader' );

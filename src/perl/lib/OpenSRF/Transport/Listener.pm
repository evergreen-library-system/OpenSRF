package OpenSRF::Transport::Listener;
use base 'OpenSRF';
use OpenSRF::Utils::Logger qw(:level);

=head1 Description

This is the empty class that acts as the subclass of the transport listener.  My API
includes

new( $app )
	create a new Listener with appname $app

initialize()
	Perform any transport layer connections/authentication.

listen()
	Block, wait for, and process incoming messages

=cut

1;

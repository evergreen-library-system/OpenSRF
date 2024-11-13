package OpenSRF::Transport;
use strict; use warnings;
use base 'OpenSRF';
use Time::HiRes qw/time/;
use OpenSRF::AppSession;
use OpenSRF::Utils::JSON;
use OpenSRF::Utils::Logger qw(:level);
use OpenSRF::DomainObject::oilsResponse qw/:status/;
use OpenSRF::EX qw/:try/;

#------------------ 
# --- These must be implemented by all Transport subclasses
# -------------------------------------------

=head2 get_peer_client

Returns the name of the package responsible for client communication

=cut

sub get_peer_client { shift()->alert_abstract(); } 

=head2 get_msg_envelope

Returns the name of the package responsible for parsing incoming messages

=cut

sub get_msg_envelope { shift()->alert_abstract(); } 

# -------------------------------------------

our $message_envelope;
my $logger = "OpenSRF::Utils::Logger"; 

=head2 handler( $data )

Creates a new Message, extracts the remote_id, session_id, and message body
from the message.  Then, creates or retrieves the AppSession object with the session_id and remote_id. 
Finally, creates the message document from the body of the message and calls
the handler method on the message document.

=cut

sub handler {
	my $start_time = time();
	my ($class, $service, $msg) = @_;

	my $remote_id = $msg->from;
	my $sess_id	= $msg->thread;
	my $body = $msg->body;
	my $type = $msg->type;

    $logger->internal("Transport:handler() received message with thread: $sess_id");

	$logger->set_osrf_xid($msg->osrf_xid);

	if (defined($type) and $type eq 'error') {
		throw OpenSRF::EX::Session ("$remote_id IS NOT CONNECTED TO THE NETWORK!!!");

	}

	# See if the app_session already exists.  If so, make 
	# sure the sender hasn't changed if we're a server
	my $app_session = OpenSRF::AppSession->find( $sess_id );
	if( $app_session and $app_session->endpoint == $app_session->SERVER() and
			$app_session->remote_id ne $remote_id ) {

	    my $c = OpenSRF::Utils::SettingsClient->new();
        if($c->config_value("apps", $app_session->service, "migratable")) {
            $logger->debug("service is migratable, new client is $remote_id");
        } else {

		    $logger->warn("Backend Gone or invalid sender");
		    my $res = OpenSRF::DomainObject::oilsBrokenSession->new();
		    $res->status( "Backend Gone or invalid sender, Reconnect" );
		    $app_session->status( $res );
		    return 1;
        }
	} 

    $logger->internal(
        "Building app session with ses=$sess_id remote=$remote_id service=$service");

	# Retrieve or build the app_session as appropriate (server_build decides which to do)
	$app_session = OpenSRF::AppSession->server_build( $sess_id, $remote_id, $service );

	if( ! $app_session ) {
		throw OpenSRF::EX::Session ("Transport::handler(): No AppSession object returned from server_build()");
	}

	# Create a document from the JSON contained within the message 
	my $doc; 
	eval { $doc = OpenSRF::Utils::JSON->JSON2perl($body); };
	if( $@ ) {

		$logger->warn("Received bogus JSON: $@");
		$logger->warn("Bogus JSON data: $body");
		my $res = OpenSRF::DomainObject::oilsXMLParseError->new( status => "JSON Parse Error --- $body\n\n$@" );

		$app_session->status($res);
		#$app_session->kill_me;
		return 1;
	}

	$logger->transport( "Transport::handler() creating \n$body", INTERNAL );

	# We need to disconnect the session if we got a bus error on the client 
    # side.  For server side, we'll just tear down the session and go away.
	if (defined($type) and $type eq 'error') {
		# If we're a server
		if( $app_session->endpoint == $app_session->SERVER() ) {
			$app_session->kill_me;
			return 1;
		} else {
			$app_session->reset;
			$app_session->state( $app_session->DISCONNECTED );
			# below will lead to infinite looping, should return an exception
			#$app_session->push_resend( $app_session->app_request( 
			#		$doc->documentElement->firstChild->threadTrace ) );
			$logger->debug(
				"Got error on client connection $remote_id, nothing we can do..", ERROR );
			return 1;
		}
	}

	# cycle through and pass each oilsMessage contained in the message
	# up to the message layer for processing.
	for my $msg (@$doc) {

		next unless (	$msg && UNIVERSAL::isa($msg => 'OpenSRF::DomainObject::oilsMessage'));

		OpenSRF::AppSession->ingress($msg->sender_ingress);

		if( $app_session->endpoint == $app_session->SERVER() ) {

			try {  

				if( ! $msg->handler( $app_session ) ) { return 0; }
				$logger->info(sprintf("Message processing duration: %.3f", (time() - $start_time)));

			} catch Error with {

				my $e = shift;
				my $res = OpenSRF::DomainObject::oilsServerError->new();
				$res->status( $res->status . "\n$e");
				$app_session->status($res) if $res;
				$app_session->kill_me;
				return 0;

			};

		} else { 

			if( ! $msg->handler( $app_session ) ) { return 0; } 
			$logger->debug(sub{return sprintf("Response processing duration: %.3f", (time() - $start_time)) });

		}
	}

	return $app_session;
}

1;

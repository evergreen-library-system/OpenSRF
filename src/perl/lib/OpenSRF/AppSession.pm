package OpenSRF::AppSession;
use OpenSRF::DomainObject::oilsMessage;
use OpenSRF::DomainObject::oilsMethod;
use OpenSRF::DomainObject::oilsResponse qw/:status/;
use OpenSRF::Transport::PeerHandle;
use OpenSRF::Utils::JSON;
use OpenSRF::Utils::Logger qw(:level);
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Utils::Config;
use OpenSRF::EX;
use OpenSRF;
use Exporter;
use Encode;
use base qw/Exporter OpenSRF/;
use Time::HiRes qw( time usleep );
use POSIX ();
use warnings;
use strict;

our @EXPORT_OK = qw/CONNECTING INIT_CONNECTED CONNECTED DISCONNECTED CLIENT SERVER/;
our %EXPORT_TAGS = ( state => [ qw/CONNECTING INIT_CONNECTED CONNECTED DISCONNECTED/ ],
		 endpoint => [ qw/CLIENT SERVER/ ],
);

my $logger = "OpenSRF::Utils::Logger";
my $_last_locale = 'en-US';
our $current_ingress = 'opensrf';

# Get/set the locale used by all new client sessions 
# for the current process.  This is primarily useful 
# for clients that wish to make a series of opensrf 
# calls and don't wish to set the locale for each new 
# AppSession object.
#
# The caller should reset the locale when done using 
# reset_locale(), as the locale will otherwise persist 
# for the current process until set/reset again.
#
# This is not for SERVER processes, since they 
# adopt the locale of their respective callers.
sub default_locale {
    my ($class, $locale) = @_;
    $_last_locale = $locale if $locale;
    return $_last_locale;
}
sub reset_locale {
    my ($class) = @_;
    return $_last_locale = 'en-US';
}

sub ingress {
    my ($class, $ingress) = @_;
    $current_ingress = $ingress if $ingress;
    return $current_ingress;
}

our %_CACHE;
our @_RESEND_QUEUE;

sub CONNECTING { return 3 };
sub INIT_CONNECTED { return 4 };
sub CONNECTED { return 1 };
sub DISCONNECTED { return 2 };

sub CLIENT { return 2 };
sub SERVER { return 1 };

sub find {
	return undef unless (defined $_[1]);
	return $_CACHE{$_[1]} if (exists($_CACHE{$_[1]}));
}

sub transport_connected {
	my $self = shift;
	if( ! exists $self->{peer_handle} || ! $self->{peer_handle} ) {
		return 0;
	}
	return $self->{peer_handle}->tcp_connected();
}

sub connected {
	my $self = shift;
	return $self->state == CONNECTED;
}
# ----------------------------------------------------------------------------
# Clears the transport buffers
# call this if you are not through with the sesssion, but you want 
# to have a clean slate.  You shouldn't have to call this if
# you are correctly 'recv'ing all of the data from a request.
# however, if you don't want all of the data, this will
# slough off any excess
#  * * Note: This will delete data for all sessions using this transport
# handle.  For example, all client sessions use the same handle.
# ----------------------------------------------------------------------------
sub buffer_reset {

	my $self = shift;
	if( ! exists $self->{peer_handle} || ! $self->{peer_handle} ) { 
		return 0;
	}
	$self->{peer_handle}->buffer_reset();
}


# when any incoming data is received, this method is called.
sub server_build {
	my $class = shift;
	$class = ref($class) || $class;

	my $sess_id = shift;
	my $remote_id = shift;
	my $service = shift;

	warn "Missing args to server_build():\n" .
		"sess_id: $sess_id, remote_id: $remote_id, service: $service\n" 
		unless ($sess_id and $remote_id and $service);

	return undef unless ($sess_id and $remote_id and $service);

	if ( my $thingy = $class->find($sess_id) ) {
		$thingy->remote_id( $remote_id );
		return $thingy;
	}

	if( $service eq "client" ) {
		#throw OpenSRF::EX::PANIC ("Attempting to build a client session as a server" .
		#	" Session ID [$sess_id], remote_id [$remote_id]");

		warn "Attempting to build a client session as ".
				"a server Session ID [$sess_id], remote_id [$remote_id]";

		$logger->debug("Attempting to build a client session as ".
				"a server Session ID [$sess_id], remote_id [$remote_id]", ERROR );

		return undef;
	}

	my $config_client = OpenSRF::Utils::SettingsClient->new();
	my $stateless = $config_client->config_value("apps", $service, "stateless");
    $stateless = 1 if $service eq 'router';

	#my $max_requests = $conf->$service->max_requests;
	my $max_requests	= $config_client->config_value("apps",$service,"max_requests");
	$logger->debug( "Max Requests for $service is $max_requests", INTERNAL ) if (defined $max_requests);

	$logger->transport( "AppSession creating new session: $sess_id", INTERNAL );

	my $self = bless { recv_queue  => [],
			   request_queue  => [],
               force_recycle => 0,
			   requests  => 0,
			   session_data  => {},
			   callbacks  => {},
			   endpoint    => SERVER,
			   state       => CONNECTING, 
			   session_id  => $sess_id,
			   remote_id	=> $remote_id,
				peer_handle => OpenSRF::Transport::PeerHandle->retrieve($service),
				max_requests => $max_requests,
				session_threadTrace => 0,
				service => $service,
				stateless => $stateless,
			 } => $class;

	return $_CACHE{$sess_id} = $self;
}

sub session_data {
	my $self = shift;
	my ($name, $datum) = @_;

	$self->{session_data}->{$name} = $datum if (defined $datum);
	return $self->{session_data}->{$name};
}

sub service { return shift()->{service}; }

sub continue_request {
	my $self = shift;
	$self->{'requests'}++;
	return 1 if (!$self->{'max_requests'});
	return $self->{'requests'} <= $self->{'max_requests'} ? 1 : 0;
}

sub last_sent_payload {
	my( $self, $payload ) = @_;
	if( $payload ) {
		return $self->{'last_sent_payload'} = $payload;
	}
	return $self->{'last_sent_payload'};
}

sub session_locale {
	my( $self, $type ) = @_;
	if( $type ) {
        $_last_locale = $type if ($self->endpoint == SERVER);
		return $self->{'session_locale'} = $type;
	}
	return $self->{'session_locale'};
}

sub last_sent_type {
	my( $self, $type ) = @_;
	if( $type ) {
		return $self->{'last_sent_type'} = $type;
	}
	return $self->{'last_sent_type'};
}

sub get_app_targets {
	my $app = shift;
    return ("opensrf:service:$app");
}

sub stateless {
	my $self = shift;
	my $state = shift;
	$self->{stateless} = $state if (defined $state);
	return $self->{stateless};
}

# When true, indicates the server drone should be killed (recycled)
# after the current session has completed.  This overrides the
# configured max_request value.
sub force_recycle {
    my ($self, $force) = @_;
    $self->{force_recycle} = $force if defined $force;
    return $self->{force_recycle};
}

# When we're a client and we want to connect to a remote service
sub create {
	my $class = shift;
	$class = ref($class) || $class;

	my $app = shift;
        my $api_level = shift;
	my $quiet = shift;
	my $locale = shift || $_last_locale;

	$api_level = 1 if (!defined($api_level));
			        
	$logger->debug( "AppSession creating new client session for $app", DEBUG );

	my $stateless = 0;
	my $c = OpenSRF::Utils::SettingsClient->new();
	# we can get an infinite loop if we're grabbing the settings and we
	# need the settings to grab the settings...
	if($app ne "opensrf.settings" || $c->has_config()) { 
		$stateless = $c->config_value("apps", $app, "stateless");
	}

    $stateless = 1 if $app eq 'router';

	my $sess_id = time . rand( $$ );
	while ( $class->find($sess_id) ) {
		$sess_id = time . rand( $$ );
	}

	
	my ($r_id) = get_app_targets($app);

	my $peer_handle = OpenSRF::Transport::PeerHandle->retrieve("client"); 
	if( ! $peer_handle ) {
		$peer_handle = OpenSRF::Transport::PeerHandle->retrieve("system_client");
	}

	my $self = bless { app_name    => $app,
			   request_queue  => [],
			   endpoint    => CLIENT,
			   state       => DISCONNECTED,#since we're init'ing
			   session_id  => $sess_id,
			   remote_id   => $r_id,
			   raise_error   => $quiet ? 0 : 1,
			   session_locale   => $locale,
			   api_level   => $api_level,
			   orig_remote_id   => $r_id,
				peer_handle => $peer_handle,
				session_threadTrace => 0,
				stateless		=> $stateless,
			 } => $class;

	$logger->debug( "Created new client session $app : $sess_id" );

	return $_CACHE{$sess_id} = $self;
}

sub raise_remote_errors {
	my $self = shift;
	my $err = shift;
	$self->{raise_error} = $err if (defined $err);
	return $self->{raise_error};
}

sub api_level {
	return shift()->{api_level};
}

sub app {
	return shift()->{app_name};
}

sub reset {
	my $self = shift;
	$self->remote_id($$self{orig_remote_id});
}

# 'connect' can be used as a constructor if called as a class method,
# or used to connect a session that has disconnectd if called against
# an existing session that seems to be disconnected, or was just built
# using 'create' above.

# connect( $app, username => $user, secret => $passwd );
#    OR
# connect( $app, sysname => $user, secret => $shared_secret );

# --- Returns undef if the connect attempt times out.
# --- Returns the OpenSRF::EX object if one is returned by the server
# --- Returns self if connected
sub connect {
	my $self = shift;
	my $class = ref($self) || $self;


	if ( ref( $self ) and  $self->state && $self->state == CONNECTED  ) {
		$logger->transport("AppSession already connected", DEBUG );
	} else {
		$logger->transport("AppSession not connected, connecting..", DEBUG );
	}
	return $self if ( ref( $self ) and  $self->state && $self->state == CONNECTED  );


	my $app = shift;
	my $api_level = shift;
	$api_level = 1 unless (defined $api_level);

	$self = $class->create($app, @_) if (!ref($self));

	return undef unless ($self);

	$self->{api_level} = $api_level;

	$self->reset;
	$self->state(CONNECTING);
	$self->send('CONNECT', "");


	# if we want to connect to settings, we may not have 
	# any data for the settings client to work with...
	# just using a default for now XXX

	my $time_remaining = 5;


#	my $client = OpenSRF::Utils::SettingsClient->new();
#	my $trans = $client->config_value("client_connection","transport_host");
#
#	if(!ref($trans)) {
#		$time_remaining = $trans->{connect_timeout};
#	} else {
#		# XXX for now, just use the first
#		$time_remaining = $trans->[0]->{connect_timeout};
#	}

	while ( $self->state != CONNECTED  and $time_remaining > 0 ) {
		my $starttime = time;
		$self->queue_wait($time_remaining);
		my $endtime = time;
		$time_remaining -= ($endtime - $starttime);
	}

	return undef unless($self->state == CONNECTED);

	$self->stateless(0);

	return $self;
}

sub finish {
	my $self = shift;
	if( ! $self->session_id ) {
		return 0;
	}
}

sub unregister_callback {
	my $self = shift;
	my $type = shift;
	my $cb = shift;
	if (exists $self->{callbacks}{$type}) {
		delete $self->{callbacks}{$type}{$cb};
		return $cb;
	}
	return undef;
}

sub register_callback {
	my $self = shift;
	my $type = shift;
	my $cb = shift;
	my $cb_key = "$cb";
	$self->{callbacks}{$type}{$cb_key} = $cb;
	return $cb_key;
}

sub kill_me {
	my $self = shift;
	if( ! $self->session_id ) { return 0; }

	# run each 'death' callback;
	if (exists $self->{callbacks}{death}) {
		for my $sub (values %{$self->{callbacks}{death}}) {
			$sub->($self);
		}
	}

	$self->disconnect;
	$logger->transport(sub{return "AppSession killing self: " . $self->session_id() }, DEBUG );
	delete $_CACHE{$self->session_id};
	delete($$self{$_}) for (keys %$self);
}

sub disconnect {
	my $self = shift;

	# run each 'disconnect' callback;
	if (exists $self->{callbacks}{disconnect}) {
		for my $sub (values %{$self->{callbacks}{disconnect}}) {
			$sub->($self);
		}
	}

	if ( !$self->stateless and $self->state != DISCONNECTED ) {
		$self->send('DISCONNECT', "") if ($self->endpoint == CLIENT);
		$self->state( DISCONNECTED ); 
	}

	$self->reset;
}

sub request {
	my $self = shift;
	my $meth = shift;
	return unless $self;

   # tell the logger to create a new xid - the logger will decide if it's really necessary
   $logger->mk_osrf_xid;

	my $method;
	if (!ref $meth) {
		$method = new OpenSRF::DomainObject::oilsMethod ( method => $meth );
	} else {
		$method = $meth;
	}
	
	$method->params( @_ );

	$self->send('REQUEST',$method);
}

sub full_request {
	my $self = shift;
	my $meth = shift;

	my $method;
	if (!ref $meth) {
		$method = new OpenSRF::DomainObject::oilsMethod ( method => $meth );
	} else {
		$method = $meth;
	}
	
	$method->params( @_ );

	$self->send(CONNECT => '', REQUEST => $method, DISCONNECT => '');
}

sub send {
	my $self = shift;
	my @payload_list = @_; # this is a Domain Object

	return unless ($self and $self->{peer_handle});

	$logger->debug( "In send", INTERNAL );
	
	my $tT;

	if( @payload_list % 2 ) { $tT = pop @payload_list; }

	if( ! @payload_list ) {
		$logger->debug( "payload_list param is incomplete in AppSession::send()", ERROR );
		return undef; 
	}

	my @doc = ();

	my $disconnect = 0;
	my $connecting = 0;

	while( @payload_list ) {

		my ($msg_type, $payload) = ( shift(@payload_list), shift(@payload_list) ); 

		if ($msg_type eq 'DISCONNECT' ) {
			$disconnect++;
			if( $self->state == DISCONNECTED && !$connecting) {
				next;
			}
		}

		if( $msg_type eq "CONNECT" ) { 
			$connecting++; 
		}

		my $msg = OpenSRF::DomainObject::oilsMessage->new();
		$msg->type($msg_type);
	
		no warnings;
		$msg->threadTrace( $tT || int($self->session_threadTrace) || int($self->last_threadTrace) );
		use warnings;
	
		if ($msg->type eq 'REQUEST') {
			if ( !defined($tT) || $self->last_threadTrace != $tT ) {
				$msg->update_threadTrace;
				$self->session_threadTrace( $msg->threadTrace );
				$tT = $self->session_threadTrace;
				OpenSRF::AppRequest->new($self, $payload);
			}
		}
	
		$msg->api_level($self->api_level);
		$msg->payload($payload) if $payload;

        my $locale = $self->session_locale;
		$msg->sender_locale($locale) if ($locale);

		$msg->sender_ingress($current_ingress);
	
		push @doc, $msg;

	
		$logger->debug(sub{return "AppSession sending ".$msg->type." to ".$self->remote_id.
			" with threadTrace [".$msg->threadTrace."]" });

	}
	
	if ($self->endpoint == CLIENT and ! $disconnect) {
		$self->queue_wait(0);


		if($self->stateless && $self->state != CONNECTED) {
			$self->reset;
			$logger->debug("AppSession is stateless in send", INTERNAL );
		}

		if( !$self->stateless and $self->state != CONNECTED ) {

			$logger->debug( "Sending connect before request 1", INTERNAL );

			unless (($self->state == CONNECTING && $connecting )) {
				$logger->debug( "Sending connect before request 2", INTERNAL );
				my $v = $self->connect();
				if( ! $v ) {
					$logger->debug( "Unable to connect to remote service in AppSession::send()", ERROR );
					return undef;
				}
				if( ref($v) and $v->can("class") and $v->class->isa( "OpenSRF::EX" ) ) {
					return $v;
				}
			}
		}

	} 
	my $json = OpenSRF::Utils::JSON->perl2JSON(\@doc);

    my $recipient = $self->remote_id;

    if ($self->endpoint == CLIENT and $self->state != CONNECTED) {
        # Send new requests to our router
        my $conf = OpenSRF::Utils::Config->current;
        my $domain = $conf->bootstrap->domain;
        $recipient = "opensrf:router:$domain";
    }

    $logger->internal("AppSession sending doc to=$recipient: $json");

    $self->{peer_handle}->send_to( 
        $recipient,
        to     => $self->remote_id,
        thread => $self->session_id,
        body   => $json
    );

	if( $disconnect) {
		$self->state( DISCONNECTED );
	}

	my $req = $self->app_request( $tT );
	$req->{_start} = time;
	return $req
}

sub app_request {
	my $self = shift;
	my $tT = shift;
	
	return undef unless (defined $tT);
	my ($req) = grep { $_->threadTrace == $tT } @{ $self->{request_queue} };

	return $req;
}

sub remove_app_request {
	my $self = shift;
	my $req = shift;
	
	my @list = grep { $_->threadTrace != $req->threadTrace } @{ $self->{request_queue} };

	$self->{request_queue} = \@list;
}

sub endpoint {
	return $_[0]->{endpoint};
}


sub session_id {
	my $self = shift;
	return $self->{session_id};
}

sub push_queue {
	my $self = shift;
	my $resp = shift;
	my $req = $self->app_request($resp->[1]);
	return $req->push_queue( $resp->[0] ) if ($req);
	push @{ $self->{recv_queue} }, $resp->[0];
}

sub last_threadTrace {
	my $self = shift;
	my $new_last_threadTrace = shift;

	my $old_last_threadTrace = $self->{last_threadTrace};
	if (defined $new_last_threadTrace) {
		$self->{last_threadTrace} = $new_last_threadTrace;
		return $new_last_threadTrace unless ($old_last_threadTrace);
	}

	return $old_last_threadTrace;
}

sub session_threadTrace {
	my $self = shift;
	my $new_last_threadTrace = shift;

	my $old_last_threadTrace = $self->{session_threadTrace};
	if (defined $new_last_threadTrace) {
		$self->{session_threadTrace} = $new_last_threadTrace;
		return $new_last_threadTrace unless ($old_last_threadTrace);
	}

	return $old_last_threadTrace;
}

sub last_message_type {
	my $self = shift;
	my $new_last_message_type = shift;

	my $old_last_message_type = $self->{last_message_type};
	if (defined $new_last_message_type) {
		$self->{last_message_type} = $new_last_message_type;
		return $new_last_message_type unless ($old_last_message_type);
	}

	return $old_last_message_type;
}

sub last_message_api_level {
	my $self = shift;
	my $new_last_message_api_level = shift;

	my $old_last_message_api_level = $self->{last_message_api_level};
	if (defined $new_last_message_api_level) {
		$self->{last_message_api_level} = $new_last_message_api_level;
		return $new_last_message_api_level unless ($old_last_message_api_level);
	}

	return $old_last_message_api_level;
}

sub remote_id {
	my $self = shift;
	my $new_remote_id = shift;

	my $old_remote_id = $self->{remote_id};
	if (defined $new_remote_id) {
		$self->{remote_id} = $new_remote_id;
		return $new_remote_id unless ($old_remote_id);
	}

	return $old_remote_id;
}

sub client_auth {
	return undef;
	my $self = shift;
	my $new_ua = shift;

	my $old_ua = $self->{client_auth};
	if (defined $new_ua) {
		$self->{client_auth} = $new_ua;
		return $new_ua unless ($old_ua);
	}

	return $old_ua->cloneNode(1);
}

sub state {
	my $self = shift;
	my $new_state = shift;

	my $old_state = $self->{state};
	if (defined $new_state) {
		$self->{state} = $new_state;
		return $new_state unless ($old_state);
	}

	return $old_state;
}

sub DESTROY {
	my $self = shift;
	delete $$self{$_} for keys %$self;
	return undef;
}

sub recv {
	my $self = shift;
	my @proto_args = @_;
	my %args;

	if ( @proto_args ) {
		if ( !(@proto_args % 2) ) {
			%args = @proto_args;
		} elsif (@proto_args == 1) {
			%args = ( timeout => @proto_args );
		}
	}

	#$logger->debug(sub{return ref($self). " recv_queue before wait: " . $self->_print_queue() }, INTERNAL );

	if( exists( $args{timeout} ) ) {
		$args{timeout} = int($args{timeout});
		$self->{recv_timeout} = $args{timeout};
	}

	#$args{timeout} = 0 if ($self->complete);

	if(defined($args{timeout})) {
		$logger->debug( ref($self) ."->recv with timeout " . $args{timeout}, INTERNAL );
	}

	my $avail = @{ $self->{recv_queue} };
	$self->{remaining_recv_timeout} = $self->{recv_timeout};

	if (!$args{count}) {
		if (wantarray) {
			$args{count} = $avail;
		} else {
			$args{count} = 1;
		}
	}

	while ( $self->{remaining_recv_timeout} > 0 and $avail < $args{count} ) {
			last if $self->complete;
			my $starttime = time;
			$self->queue_wait($self->{remaining_recv_timeout});
			my $endtime = time;
			if ($self->{timeout_reset}) {
				$self->{timeout_reset} = 0;
			} else {
				$self->{remaining_recv_timeout} -= ($endtime - $starttime)
			}
			$avail = @{ $self->{recv_queue} };
	}

    $self->timed_out(1) if ( $self->{remaining_recv_timeout} <= 0 );

	my @list;
	while ( my $msg = shift @{ $self->{recv_queue} } ) {
		push @list, $msg;
		last if (scalar(@list) >= $args{count});
	}

	$logger->debug(sub{return "Number of matched responses: " . @list }, DEBUG );
	$self->queue_wait(0); # check for statuses
	
	return $list[0] if (!wantarray);
	return @list;
}

sub timed_out {
    my $self = shift;
    my $out = shift;
    $self->{timed_out} = $out if (defined $out);
    return $self->{timed_out};
}

sub push_resend {
	my $self = shift;
	push @OpenSRF::AppSession::_RESEND_QUEUE, @_;
}

sub flush_resend {
	my $self = shift;
	$logger->debug(sub{return "Resending..." . @_RESEND_QUEUE }, INTERNAL );
	while ( my $req = shift @OpenSRF::AppSession::_RESEND_QUEUE ) {
		$req->resend unless $req->complete;
	}
}


sub queue_wait {
	my $self = shift;
	if( ! $self->{peer_handle} ) { return 0; }
	my $timeout = shift || 0;
	$logger->debug( "Calling queue_wait($timeout)" , INTERNAL );
	my $o = $self->{peer_handle}->process($timeout);
	$self->flush_resend;
	return $o;
}

sub _print_queue {
	my( $self ) = @_;
	my $string = "";
	foreach my $msg ( @{$self->{recv_queue}} ) {
		$string = $string . $msg->toString(1) . "\n";
	}
	return $string;
}

sub status {
	my $self = shift;
	return unless $self;
	$self->send( 'STATUS', @_ );
}

sub reset_request_timeout {
	my $self = shift;
	my $tt = shift;
	my $req = $self->app_request($tt);
	$req->{remaining_recv_timeout} = $req->{recv_timeout};
	$req->{timout_reset} = 1;
}

#-------------------------------------------------------------------------------

package OpenSRF::AppRequest;
use base qw/OpenSRF::AppSession/;
use OpenSRF::Utils::Logger qw/:level/;
use OpenSRF::DomainObject::oilsResponse qw/:status/;
use Time::HiRes qw/time usleep/;

sub new {
	my $class = shift;
	$class = ref($class) || $class;

	my $session = shift;
	my $threadTrace = $session->session_threadTrace || $session->last_threadTrace;
	my $payload = shift;
	
	my $self = {	session			=> $session,
			threadTrace		=> $threadTrace,
			payload			=> $payload,
			complete		=> 0,
			resp_count		=> 0,
			max_bundle_count	=> 0,
			current_bundle_count=> 0,
			max_chunk_size		=> 0,
			max_bundle_size		=> 0,
			current_bundle_size	=> 0,
			current_bundle		=> [],
			timeout_reset		=> 0,
			recv_timeout		=> 30,
			remaining_recv_timeout	=> 30,
			recv_queue		=> [],
			part_recv_buffer=> '',
	};

	bless $self => $class;

	push @{ $self->session->{request_queue} }, $self;

	return $self;
}

sub max_bundle_count {
	my $self = shift;
	my $value = shift;
	$self->{max_bundle_count} = $value if (defined($value));
	return $self->{max_bundle_count};
}

sub max_bundle_size {
	my $self = shift;
	my $value = shift;
	$self->{max_bundle_size} = $value if (defined($value));
	return $self->{max_bundle_size};
}

sub max_chunk_size {
	my $self = shift;
	my $value = shift;
	$self->{max_chunk_size} = $value if (defined($value));
	return $self->{max_chunk_size};
}

sub recv_timeout {
	my $self = shift;
	my $timeout = shift;
	if (defined $timeout) {
		$self->{recv_timeout} = $timeout;
		$self->{remaining_recv_timeout} = $timeout;
	}
	return $self->{recv_timeout};
}

sub queue_size {
	my $size = @{$_[0]->{recv_queue}};
	return $size;
}
	
sub send {
	my $self = shift;
	return unless ($self and $self->session and !$self->complete);
	$self->session->send(@_);
}

sub finish {
	my $self = shift;
	return unless $self->session;
	$self->session->remove_app_request($self);
	delete($$self{$_}) for (keys %$self);
}

sub session {
	return shift()->{session};
}

sub complete {
	my $self = shift;
	my $complete = shift;
	return $self->{complete} if ($self->{complete});
	if (defined $complete) {
		$self->{complete} = $complete;
		$self->{_duration} = time - $self->{_start} if ($self->{complete});
	} else {
		$self->session->queue_wait(0);
	}
    $self->completing(0) if ($self->{complete});
	return $self->{complete};
}

sub completing {
	my $self = shift;
	my $value = shift;
	$self->{_completing} = $value if (defined($value));
	return $self->{_completing};
}

sub duration {
	my $self = shift;
	$self->wait_complete;
	return $self->{_duration};
}

sub wait_complete {
	my $self = shift;
	my $timeout = shift || 10;
	my $time_remaining = $timeout;

	while ( ! $self->complete  and $time_remaining > 0 ) {
		my $starttime = time;
		$self->queue_wait($time_remaining);
		my $endtime = time;
		$time_remaining -= ($endtime - $starttime);
	}

	return $self->complete;
}

sub threadTrace {
	return shift()->{threadTrace};
}

sub push_queue {
	my $self = shift;
	my $resp = shift;
	if( !$resp ) { return 0; }
	if( UNIVERSAL::isa($resp, "Error")) {
		$self->{failed} = $resp;
		$self->complete(1);
		#return; eventually...
	}

	if( UNIVERSAL::isa($resp, "OpenSRF::DomainObject::oilsResult::Partial")) {
		$self->{part_recv_buffer} .= $resp->content;
		return 1;
	} elsif( UNIVERSAL::isa($resp, "OpenSRF::DomainObject::oilsResult::PartialComplete")) {
		if ($self->{part_recv_buffer}) {
			$resp = new OpenSRF::DomainObject::oilsResult;
			$resp->content( OpenSRF::Utils::JSON->JSON2perl( $self->{part_recv_buffer} ) );
			$self->{part_recv_buffer} = '';
		} 
	}

	push @{ $self->{recv_queue} }, $resp;
}

sub failed {
	my $self = shift;
	return $self->{failed};
}

sub queue_wait {
	my $self = shift;
	return $self->session->queue_wait(@_)
}

sub payload { return shift()->{payload}; }

sub resend {
	my $self = shift;
	return unless ($self and $self->session and !$self->complete);
	OpenSRF::Utils::Logger->debug(sub{return "I'm resending the request for threadTrace ". $self->threadTrace }, DEBUG);
	return $self->session->send('REQUEST', $self->payload, $self->threadTrace );
}

sub status {
	my $self = shift;
	my $msg = shift;
	return unless ($self and $self->session and !$self->complete);
	$self->session->send( 'STATUS',$msg, $self->threadTrace );
}

# TODO stream_push only works when server sessions can accept RESULT 
# messages, which is no longer supported.  Create a new OpenSRF message
# type to support client-to-server streams.
#sub stream_push {
#	my $self = shift;
#	my $msg = shift;
#	$self->respond( $msg );
#}

sub respond {
	my $self = shift;
	my $msg = shift;
	return unless ($self and $self->session and !$self->complete);

    my $type = 'RESULT';
	my $response;
	if (ref($msg) && UNIVERSAL::isa($msg, 'OpenSRF::DomainObject::oilsResponse')) {
		$response = $msg;
        $type = 'STATUS' if UNIVERSAL::isa($response, 'OpenSRF::DomainObject::oilsStatus');

	} else {

        if ($self->max_chunk_size > 0) { # we might need to chunk
            my $str = OpenSRF::Utils::JSON->perl2JSON($msg);

            # XML can add a lot of length to a chunk due to escaping, so we
            # calculate chunk size based on an XML-escaped version of the message.
            # Example: If escaping doubles the length of the string then $ratio
            # will be 0.5 and we'll cut the chunk size for this message in half.

            my $raw_length = length(Encode::encode_utf8($str)); # count bytes
            my $escaped_length = $raw_length;
            $escaped_length += 11 * (() = ( $str =~ /"/g)); # 7 \s and &quot;
            $escaped_length += 4 * (() = ( $str =~ /&/g)); # &amp;
            $escaped_length += 3 * (() = ( $str =~ /[<>]/g)); # &lt; / &gt;

            my $chunk_size = $self->max_chunk_size;

            if ($escaped_length > $self->max_chunk_size) {
                $chunk_size = POSIX::floor(($raw_length / $escaped_length) * $self->max_chunk_size);
            }

            if ($raw_length > $chunk_size) { # send partials ("chunking")
                # First, send out any messages queued up to be sent as
                # a bundle; otherwise, the chunked message will get sent
                # out of order
                if ($self->{current_bundle_count} > 0) {
                    # ignore any errors; if there's a problem, it presumably
                    # will be reported when the partial-complete message
                    # is sent
                    $self->session->send( @{$self->{current_bundle}}, $self->threadTrace);
                    $self->{current_bundle} = [];
                    $self->{current_bundle_size} = 0;
                    $self->{current_bundle_count} = 0;
                }
                my $num_bytes = length(Encode::encode_utf8($str));
                for (my $i = 0; $i < $num_bytes; $i += $chunk_size) {
                    $response = new OpenSRF::DomainObject::oilsResult::Partial;
                    $response->content( substr($str, $i, $chunk_size) );
                    $self->session->send($type, $response, $self->threadTrace);
                }
                # This triggers reconstruction on the remote end
                $response = new OpenSRF::DomainObject::oilsResult::PartialComplete;
                return $self->session->send($type, $response, $self->threadTrace);
            }
        }

        # message failed to exceed max chunk size OR chunking disabled
        $response = new OpenSRF::DomainObject::oilsResult;
        $response->content($msg);
    }

    if ($self->{max_bundle_count} > 0 or $self->{max_bundle_size} > 0) { # we are bundling, and we need to test the size or count

        $self->{current_bundle_size} += length(
            Encode::encode_utf8(OpenSRF::Utils::JSON->perl2JSON($response)));
        push @{$self->{current_bundle}}, $type, $response;  
        $self->{current_bundle_count}++;

        if ( $self->completing ||
                ($self->{max_bundle_size}  && $self->{current_bundle_size}  >= $self->{max_bundle_size} ) ||
                ($self->{max_bundle_count} && $self->{current_bundle_count} >= $self->{max_bundle_count})
        ) { # send chunk and reset
            my $send_res = $self->session->send( @{$self->{current_bundle}}, $self->threadTrace);
            $self->{current_bundle} = [];
            $self->{current_bundle_size} = 0;
            $self->{current_bundle_count} = 0;
            return $send_res;
        } else { # not at a chunk yet, just queue it up
            return $self->session->app_request( $self->threadTrace );
        }
    }

	$self->session->send($type, $response, $self->threadTrace);
}

sub respond_complete {
	my $self = shift;
	my $msg = shift;
	return unless ($self and $self->session and !$self->complete);

    $self->respond($msg) if (defined($msg));

    $self->completing(1);
    $self->respond(
        OpenSRF::DomainObject::oilsConnectStatus->new(
            statusCode => STATUS_COMPLETE(),
            status => 'Request Complete'
        )
    );
	$self->complete(1);
}

sub register_death_callback {
	my $self = shift;
	my $cb = shift;
	$self->session->register_callback( death => $cb );
}


# utility method.  checks to see of the request failed.
# if so, throws an OpenSRF::EX::ERROR. if everything is
# ok, it returns the content of the request
sub gather {
	my $self = shift;
	my $finish = shift;
	$self->wait_complete;
	my $resp = $self->recv( timeout => 60 );
	if( $self->failed() ) { 
		throw OpenSRF::EX::ERROR
			($self->failed()->stringify());
	}
	if(!$resp) { return undef; }
	my $content = $resp->content;
	if($finish) { $self->finish();}
	return $content;
}


package OpenSRF::AppSubrequest;
use base 'OpenSRF::AppRequest';

sub respond {
	my $self = shift;
	return if $self->complete;

	my $resp = shift;
    return $self->SUPER::respond($resp) if $self->respond_directly;

	push @{$$self{resp}}, $resp if (defined $resp);
}

sub respond_complete {
	my $self = shift;
    return $self->SUPER::respond_complete(@_) if $self->respond_directly;
	$self->respond(@_);
	$self->complete(1);
}

sub new {
    my $class = shift;
    $class = ref($class) || $class;
    my $self = bless({
        complete        => 0,
        respond_directly=> 0,  # use the passed session directly (RD mode)
        resp            => [],
        threadTrace     => 0,  # needed for respond in RD mode
        max_chunk_count => 0,  # needed for respond in RD mode
        max_chunk_size  => 0,  # needed for respond in RD mode
        max_bundle_size	=> 0,
        current_bundle  => [], # needed for respond_complete in RD mode
        current_bundle_count=> 0,
        current_bundle_size	=> 0,
        max_bundle_count	=> 0,
        @_
    }, $class);
    if ($self->session) {
        # steal the thread trace from the parent session for RD mode
        $self->{threadTrace} = $self->session->session_threadTrace || $self->session->last_threadTrace;
    }
    return $self;
}

sub responses { @{$_[0]->{resp}} }

sub respond_directly {
	my $x = shift;
	my $s = shift;
	$x->{respond_directly} = $s if (defined $s);
	return $x->session && $x->{respond_directly};
}

sub session {
	my $x = shift;
	my $s = shift;
	$x->{session} = $s if ($s);
	return $x->{session};
}

sub complete {
	my $x = shift;
	my $c = shift;
	$x->{complete} = $c if ($c);
    $x->completing(0) if ($c);
	return $x->{complete};
}

sub completing {
	my $self = shift;
	my $value = shift;
	$self->{_completing} = $value if (defined($value));
	return $self->{_completing};
}

sub status {}


1;


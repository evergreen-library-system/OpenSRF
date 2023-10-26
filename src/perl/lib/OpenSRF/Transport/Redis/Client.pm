package OpenSRF::Transport::Redis::Client;
use strict;
use warnings;
use Redis;
use Time::HiRes q/time/;
use OpenSRF::Utils::JSON;
use OpenSRF::Utils::Logger qw/$logger/;
use OpenSRF::Transport;
use OpenSRF::Transport::Redis::Message;
use OpenSRF::Transport::Redis::BusConnection;

# There will only be one Client per process, but each client may
# have multiple connections.
my $_singleton;
sub retrieve { return $_singleton; }

sub new {
    my ($class, $service, $force) = @_;

    return $_singleton if $_singleton && !$force;

    my $self = {
        service => $service,
        connections => {},
    };

    bless($self, $class);

    my $conf = OpenSRF::Utils::Config->current;
    my $domain = $conf->bootstrap->domain;
    my $username = $conf->bootstrap->username;
    my $router_name = $conf->bootstrap->router_name || 'router';

    # Create a connection for our primary node.
    $self->add_connection($domain);
    $self->{primary_domain} = $domain;

    if ($service) {
        # If we're a service, this is where we listen for service-level requests.
        # User $router_name instead of our username as the destination 
        # for API calls for this domain, managed by this router.
        # E.g. this allows 1.) direct to drone delivery and 2.) multiple
        # listeners for the same service to run on a single router_name+domain
        # segment.
        $self->{service_address} = "opensrf:service:$username:$domain:$service";
    }

    return $_singleton = $self;
}

sub reset {                                                                    
    return unless $_singleton;
    $logger->debug("Redis client disconnecting on reset()");
    $_singleton->disconnect;
    $_singleton = undef;
} 

sub connection_type {
    my $self = shift;
    return $self->{connection_type};
}

sub add_connection {
    my ($self, $domain) = @_;

    my $conf = OpenSRF::Utils::Config->current;

    my $username = $conf->bootstrap->username;
    my $password = $conf->bootstrap->passwd;
    my $port = $conf->bootstrap->port;

    # Assumes other connection parameters are the same across
    # Redis instances, apart from the hostname.
    my $connection = OpenSRF::Transport::Redis::BusConnection->new(
        $domain, $port, $username, $password, 
        $self->service ne 'client' ? $self->service : undef
    );

    $connection->set_address();
    $self->{connections}->{$domain} = $connection;
    
    $connection->connect;

    return $connection;
}

sub get_connection {
    my ($self, $domain) = @_;

    my $con = $self->{connections}->{$domain};

    return $con if $con;

    eval { $con = $self->add_connection($domain) };

    if ($@) {
        $logger->error("Could not connect to bus on node: $domain : $@");
        return undef;
    }

    return $con;
}

# Contains a value if this is a service client.
# Undef for standalone clients.
sub service {
    my $self = shift;
    return $self->{service};
}

# Contains a value if this is a service client.
# Undef for standalone clients.
sub service_address {
    my $self = shift;
    return $self->{service_address};
}

sub primary_domain {
    my $self = shift;
    return $self->{primary_domain};
}

sub primary_connection {
    my $self = shift;
    return $self->{connections}->{$self->primary_domain};
}

sub disconnect {
    my ($self, $domain) = @_;

    for my $domain (keys %{$self->{connections}}) {
        my $con = $self->{connections}->{$domain};
        $con->disconnect($self->primary_domain eq $domain);
    }

    $self->{connections} = {};
    $_singleton = undef;
}

sub connected {
    my $self = shift;
    return $self->primary_connection && $self->primary_connection->connected;
}

sub tcp_connected {
    my $self = shift;
    return $self->connected;
}


# Send a message to $recipient regardless of what's in the 'to'
# field of the message.
sub send_to {
    my ($self, $recipient, @msg_parts) = @_;

    my $msg = OpenSRF::Transport::Redis::Message->new(@msg_parts);

    $msg->body(OpenSRF::Utils::JSON->JSON2perl($msg->body));

    $msg->osrf_xid($logger->get_osrf_xid);
    $msg->from($self->primary_connection->address);

    my $msg_json = $msg->to_json;
    my $con = $self->primary_connection;

    if ($recipient =~ /^opensrf:client/o || $recipient =~ /^opensrf:router/o) {
        # Clients may be lurking on remote nodes.
        # Make sure we have a connection to said node.

        # opensrf:client:username:domain:...
        my (undef, undef, undef, $domain) = split(/:/, $recipient);

        $con = $self->get_connection($domain);
        if (!$con) {
            $logger->error("Cannot send message to node $domain: $msg_json");
            return;
        }
    }

    $con->send($recipient, $msg_json);
}

sub send {
    my $self = shift;
    my %msg_parts = @_;
    return $self->send_to($msg_parts{to}, %msg_parts);
}

sub process {
    my ($self, $timeout, $for_service) = @_;

    $timeout ||= 0;

    # Redis does not support fractional timeouts.
    $timeout = 1 if ($timeout > 0 && $timeout < 1);

    $timeout = int($timeout);

    if (!$self->connected) {
        # We can't do anything without a primary bus connection.
        # Sleep a short time to avoid die/fork storms, then
        # get outta here.
        $logger->error("We have no primary bus connection");
        sleep 5;
        die "Exiting on lack of primary bus connection";
    }

    return $self->recv($timeout, $for_service);
}

# $timeout=0 means check for data without blocking
# $timeout=-1 means block indefinitely.
sub recv {
    my ($self, $timeout, $for_service) = @_;

    my $dest_stream = $for_service ? $self->{service_address} : undef;

    my $resp = $self->primary_connection->recv($timeout, $dest_stream);

    return undef unless $resp;

    my $msg = OpenSRF::Transport::Redis::Message->new(json => $resp);

    return undef unless $msg;

    $logger->internal("recv()'ed thread=" . $msg->thread);

    # The message body is doubly encoded as JSON.
    $msg->body(OpenSRF::Utils::JSON->perl2JSON($msg->body));

    return $msg;
}


sub flush_socket {
    my $self = shift;
    # Remove all messages from our personal stream
    if (my $con = $self->primary_connection) {
        $con->redis->del($con->address);
    }
    return 1;
}

1;



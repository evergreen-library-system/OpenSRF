package OpenSRF::Transport::Redis::BusConnection;
use strict;
use warnings;
use Redis;
use Net::Domain qw/hostfqdn/;
use OpenSRF::Utils::Logger qw/$logger/;

# domain doubles as the host of the Redis instance.
sub new {
    my ($class, $domain, $port, $username, $password, $service) = @_;

    $logger->debug("Creating new bus connection $domain:$port user=$username");

    my $self = {
        domain => $domain || 'localhost',
        port => $port || 6379,
        username => $username,
        password => $password,
        service => $service
    };

    return bless($self, $class);
}

sub redis {
    my $self = shift;
    return $self->{redis};
}

sub connected {
    my $self = shift;
    return $self->redis ? 1 : 0;
}

sub domain {
    my $self = shift;
    return $self->{domain};
}

sub set_address {
    my ($self) = @_;

    my $address = sprintf(
        "opensrf:client:%s:%s:%s:%s:%s", 
        $self->{username}, 
        $self->{domain}, 
        hostfqdn(), 
        $$,
        int(rand(10_000_000))
    );

    $self->{address} = $address;
}

sub address {
    my $self = shift;
    return $self->{address};
}

sub connect {
    my $self = shift;

    return 1 if $self->redis;

    my $domain = $self->{domain};
    my $port = $self->{port};
    my $username = $self->{username}; 
    my $password = $self->{password}; 
    my $address = $self->{address};

    $logger->debug("Redis client connecting: ".
        "domain=$domain port=$port username=$username address=$address");

    # On disconnect, try to reconnect every second up to 60 seconds.
    my @connect_args = (
        server => "$domain:$port",
        reconnect => 60, 
        every => 1_000_000
    );

    $logger->debug("Connecting to bus: @connect_args");

    unless ($self->{redis} = Redis->new(@connect_args)) {
        die "Could not connect to Redis bus with @connect_args\n";
    }

    unless ($self->redis->auth($username, $password) eq 'OK') {
        die "Cannot authenticate with Redis instance user=$username\n";
    }

    $logger->debug("Auth'ed with Redis as $username OK : address=$address");

    return $self;
}

# Set del_stream to remove the stream and any attached consumer groups.
sub disconnect {
    my ($self, $del_stream) = @_;

    return unless $self->redis;

    $self->redis->del($self->address) if $del_stream;

    $self->redis->quit;

    delete $self->{redis};
}

sub send {
    my ($self, $dest_stream, $msg_json) = @_;
    
    $logger->internal("send(): to=$dest_stream : $msg_json");

    eval { $self->redis->rpush($dest_stream, $msg_json) };

    if ($@) { 
        $logger->error("RPUSH error: $@"); 
        $logger->error("BusConnection pausing for a few seconds after bus error");
        sleep(3);
    }
}

# $timeout=0 means check for data without blocking
# $timeout=-1 means block indefinitely.
#
# $dest_stream defaults to our bus address.  Otherwise, it would be
# the service-level address.
sub recv {
    my ($self, $timeout, $dest_stream) = @_;
    $dest_stream ||= $self->address;

    $logger->internal("Waiting for content at: $dest_stream");

    my $packet;

    if ($timeout == 0) {
        # Non-blocking list pop
        eval { $packet = $self->redis->lpop($dest_stream) };

    } else {
        # In Redis, timeout 0 means wait indefinitely
        eval { $packet = 
            $self->redis->blpop($dest_stream, $timeout == -1 ? 0 : $timeout) };
    }

    if ($@) {
        $logger->error("Redis list pop error: $@");
        return undef;
    }

    # Timed out waiting for data.
    return undef unless defined $packet;

    my $json = ref $packet eq 'ARRAY' ? $packet->[1] : $packet;

    # NOTE
    # $json will be a numeric string (typically "1", but not always)
    # when blpop is interrupted by a USR signal (and possibly others).
    # (Unclear why the response isn't simply undef or a failed eval).
    # This is not a valid message as far as OpenSRF is concerned.  Treat
    # it like a non-response.
    #
    # An OpenSRF message is at minimum over 100 bytes.  Discard anything
    # less than a fraction of that.
    return undef unless length($json) > 20;

    $logger->internal("recv() $json");

    return $json;
}

sub flush_socket {
    my $self = shift;
    # Remove all messages from my address
    $self->redis->del($self->address);
    return 1;
}

1;



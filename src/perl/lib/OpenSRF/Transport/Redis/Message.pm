package OpenSRF::Transport::Redis::Message;
use strict; use warnings;
use OpenSRF::Utils::Logger qw/$logger/;
use OpenSRF::Utils::JSON;
use OpenSRF::EX qw/:try/;
use strict; use warnings;

sub new {
    my ($class, %args) = @_;
    my $self = bless({}, $class);

    if ($args{json}) {
        $self->from_json($args{json});

    } else {
        $self->{to} = $args{to} || '';
        $self->{from} = $args{from} || '';
        $self->{thread} = $args{thread} || '';
        $self->{body} = $args{body} || '';
        $self->{osrf_xid} = $args{osrf_xid} || '';
        $self->{router_command} = $args{router_command} || '';
        $self->{router_class} = $args{router_class} || '';
        $self->{router_reply} = $args{router_reply} || '';
    }

    return $self;
}

sub to {
    my($self, $to) = @_;
    $self->{to} = $to if defined $to;
    return $self->{to};
}
sub from {
    my($self, $from) = @_;
    $self->{from} = $from if defined $from;
    return $self->{from};
}
sub thread {
    my($self, $thread) = @_;
    $self->{thread} = $thread if defined $thread;
    return $self->{thread};
}
sub body {
    my($self, $body) = @_;
    $self->{body} = $body if defined $body;
    return $self->{body};
}

sub status {
    my($self, $status) = @_;
    $self->{status} = $status if defined $status;
    return $self->{status};
}
sub type {
    my($self, $type) = @_;
    $self->{type} = $type if defined $type;
    return $self->{type};
}

sub err_type {}
sub err_code {}

sub osrf_xid {
    my($self, $osrf_xid) = @_;
    $self->{osrf_xid} = $osrf_xid if defined $osrf_xid;
    return $self->{osrf_xid};
}

sub router_command {
    my($self, $router_command) = @_;
    $self->{router_command} = $router_command if defined $router_command;
    return $self->{router_command};
}

sub router_class {
    my($self, $router_class) = @_;
    $self->{router_class} = $router_class if defined $router_class;
    return $self->{router_class};
}

sub router_reply {
    my($self, $router_reply) = @_;
    $self->{router_reply} = $router_reply if defined $router_reply;
    return $self->{router_reply};
}

sub to_json {
    my $self = shift;

    my $hash = {
        to => $self->{to},
        from => $self->{from},
        thread => $self->{thread},
        body => $self->{body}
    };

    # Some values are optional.
    # Avoid cluttering the JSON with undef values.
    for my $key (qw/osrf_xid router_command router_class router_reply/) {
        $hash->{$key} = $self->{$key} if defined $self->{$key} && $self->{$key} ne '';
    }

    return OpenSRF::Utils::JSON->perl2JSON($hash);
}

sub from_json {
    my $self = shift;
    my $json = shift;
    my $hash;

    eval { $hash = OpenSRF::Utils::JSON->JSON2perl($json); };

    if ($@) {
        $logger->error("Redis::Message received invalid JSON: $@ : $json");
        return undef;
    }

    $self->{$_} = $hash->{$_} for keys %$hash;
}

1;

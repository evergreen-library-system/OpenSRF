package OpenSRF::Transport::SlimJabber::XMPPMessage;
use strict; use warnings;
use OpenSRF::Utils::Logger qw/$logger/;
use OpenSRF::EX qw/:try/;
use strict; use warnings;
use XML::LibXML;

use constant JABBER_MESSAGE =>
    "<message to='%s' from='%s' router_command='%s' router_class='%s' osrf_xid='%s'>".
    "<thread>%s</thread><body>%s</body></message>";

sub new {
    my $class = shift;
    my %args = @_;
    my $self = bless({}, $class);

    if($args{xml}) {
        $self->parse_xml($args{xml});

    } else {
        $self->{to} = $args{to} || '';
        $self->{from} = $args{from} || '';
        $self->{thread} = $args{thread} || '';
        $self->{body} = $args{body} || '';
        $self->{osrf_xid} = $args{osrf_xid} || '';
        $self->{router_command} = $args{router_command} || '';
        $self->{router_class} = $args{router_class} || '';
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
sub err_type {
    my($self, $err_type) = @_;
    $self->{err_type} = $err_type if defined $err_type;
    return $self->{err_type};
}
sub err_code {
    my($self, $err_code) = @_;
    $self->{err_code} = $err_code if defined $err_code;
    return $self->{err_code};
}
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


sub to_xml {
    my $self = shift;

    my $body = $self->{body};
    $body =~ s/&/&amp;/sog;
    $body =~ s/</&lt;/sog;
    $body =~ s/>/&gt;/sog;

    return sprintf(
        JABBER_MESSAGE,
        $self->{to},
        $self->{from},
        $self->{router_command},
        $self->{router_class},
        $self->{osrf_xid},
        $self->{thread},
        $body
    );
}

sub parse_xml {
    my($self, $xml) = @_;
    my($doc, $err);

    try {
        $doc = XML::LibXML->new->parse_string($xml);
    } catch Error with {
        my $err = shift;
        $logger->error("Error parsing message xml: $xml --- $err");
    };
    throw $err if $err;

    my $root = $doc->documentElement;

    $self->{body} = $root->findnodes('/message/body').'';
    $self->{thread} = $root->findnodes('/message/thread').'';
    $self->{from} = $root->getAttribute('router_from');
    $self->{from} = $root->getAttribute('from') unless $self->{from};
    $self->{to} = $root->getAttribute('to');
    $self->{type} = $root->getAttribute('type');
    $self->{osrf_xid} = $root->getAttribute('osrf_xid');
}


1;

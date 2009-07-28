package OpenSRF::Transport::SlimJabber::XMPPReader;
use strict; use warnings;
use XML::Parser;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Time::HiRes qw/time/;
use OpenSRF::Transport::SlimJabber::XMPPMessage;
use OpenSRF::Utils::Logger qw/$logger/;

# -----------------------------------------------------------
# Connect, disconnect, and authentication messsage templates
# -----------------------------------------------------------
use constant JABBER_CONNECT =>
    "<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>";

use constant JABBER_BASIC_AUTH =>
    "<iq id='123' type='set'><query xmlns='jabber:iq:auth'>" .
    "<username>%s</username><password>%s</password><resource>%s</resource></query></iq>";

use constant JABBER_DISCONNECT => "</stream:stream>";


# -----------------------------------------------------------
# XMPP Stream states
# -----------------------------------------------------------
use constant DISCONNECTED   => 1;
use constant CONNECT_RECV   => 2;
use constant CONNECTED      => 3;


# -----------------------------------------------------------
# XMPP Message states
# -----------------------------------------------------------
use constant IN_NOTHING => 1;
use constant IN_BODY    => 2;
use constant IN_THREAD  => 3;
use constant IN_STATUS  => 4;


# -----------------------------------------------------------
# Constructor, getter/setters
# -----------------------------------------------------------
sub new {
    my $class = shift;
    my $socket = shift;

    my $self = bless({}, $class);

    $self->{queue} = [];
    $self->{stream_state} = DISCONNECTED;
    $self->{xml_state} = IN_NOTHING;
    $self->socket($socket);

    my $p = new XML::Parser(Handlers => {
        Start => \&start_element,
        End   => \&end_element,
        Char  => \&characters,
    });

    $self->parser($p->parse_start); # create a push parser
    $self->parser->{_parent_} = $self;
    $self->{message} = OpenSRF::Transport::SlimJabber::XMPPMessage->new;
    return $self;
}

sub push_msg {
    my($self, $msg) = @_; 
    push(@{$self->{queue}}, $msg) if $msg;
}

sub next_msg {
    my $self = shift;
    return shift @{$self->{queue}};
}

sub peek_msg {
    my $self = shift;
    return (@{$self->{queue}} > 0);
}

sub parser {
    my($self, $parser) = @_;
    $self->{parser} = $parser if $parser;
    return $self->{parser};
}

sub socket {
    my($self, $socket) = @_;
    $self->{socket} = $socket if $socket;
    return $self->{socket};
}

sub stream_state {
    my($self, $stream_state) = @_;
    $self->{stream_state} = $stream_state if $stream_state;
    return $self->{stream_state};
}

sub xml_state {
    my($self, $xml_state) = @_;
    $self->{xml_state} = $xml_state if $xml_state;
    return $self->{xml_state};
}

sub message {
    my($self, $message) = @_;
    $self->{message} = $message if $message;
    return $self->{message};
}


# -----------------------------------------------------------
# Stream and connection handling methods
# -----------------------------------------------------------

sub connect {
    my($self, $domain, $username, $password, $resource) = @_;
    
    $self->send(sprintf(JABBER_CONNECT, $domain));
    $self->wait(10);

    unless($self->{stream_state} == CONNECT_RECV) {
        $logger->error("No initial XMPP response from server");
        return 0;
    }

    $self->send(sprintf(JABBER_BASIC_AUTH, $username, $password, $resource));
    $self->wait(10);

    unless($self->connected) {
        $logger->error('XMPP connect failed');
        return 0;
    }

    return 1;
}

sub disconnect {
    my $self = shift;
    return unless $self->socket;
    if($self->tcp_connected) {
        $self->send(JABBER_DISCONNECT); 
        shutdown($self->socket, 2);
    }
    close($self->socket);
}

# -----------------------------------------------------------
# returns true if this stream is connected to the server
# -----------------------------------------------------------
sub connected {
    my $self = shift;
    return ($self->tcp_connected and $self->{stream_state} == CONNECTED);
}

# -----------------------------------------------------------
# returns true if the socket is connected
# -----------------------------------------------------------
sub tcp_connected {
    my $self = shift;
    return ($self->socket and $self->socket->connected);
}

# -----------------------------------------------------------
# sends pre-formated XML
# -----------------------------------------------------------
sub send {
    my($self, $xml) = @_;
    $self->{socket}->print($xml);
}

# -----------------------------------------------------------
# Puts a file handle into blocking mode
# -----------------------------------------------------------
sub set_block {
    my $fh = shift;
    my  $flags = fcntl($fh, F_GETFL, 0);
    $flags &= ~O_NONBLOCK;
    fcntl($fh, F_SETFL, $flags);
}


# -----------------------------------------------------------
# Puts a file handle into non-blocking mode
# -----------------------------------------------------------
sub set_nonblock {
    my $fh = shift;
    my  $flags = fcntl($fh, F_GETFL, 0);
    fcntl($fh, F_SETFL, $flags | O_NONBLOCK);
}


sub wait {
    my($self, $timeout) = @_;
     
    return $self->next_msg if $self->peek_msg;

    $timeout ||= 0;
    $timeout = undef if $timeout < 0;
    my $socket = $self->{socket};

    set_block($socket);
    
    # build the select readset
    my $infile = '';
    vec($infile, $socket->fileno, 1) = 1;
    return undef unless select($infile, undef, undef, $timeout);

    # now slurp the data off the socket
    my $buf;
    my $read_size = 1024;
    my $nonblock = 0;
    while(my $n = sysread($socket, $buf, $read_size)) {
        $self->{parser}->parse_more($buf) if $buf;
        if($n < $read_size or $self->peek_msg) {
            set_block($socket) if $nonblock;
            last;
        }
        set_nonblock($socket) unless $nonblock;
        $nonblock = 1;
    }

    return $self->next_msg;
}

# -----------------------------------------------------------
# Waits up to timeout seconds for a fully-formed XMPP
# message to arrive.  If timeout is < 0, waits indefinitely
# -----------------------------------------------------------
sub wait_msg {
    my($self, $timeout) = @_;
    my $xml;

    $timeout = 0 unless defined $timeout;

    if($timeout < 0) {
        while(1) {
            return $xml if $xml = $self->wait($timeout); 
        }

    } else {
        while($timeout >= 0) {
            my $start = time;
            return $xml if $xml = $self->wait($timeout); 
            $timeout -= time - $start;
        }
    }

    return undef;
}


# -----------------------------------------------------------
# SAX Handlers
# -----------------------------------------------------------


sub start_element {
    my($parser, $name, %attrs) = @_;
    my $self = $parser->{_parent_};

    if($name eq 'message') {

        my $msg = $self->{message};
        $msg->{to} = $attrs{'to'};
        $msg->{from} = $attrs{router_from} if $attrs{router_from};
        $msg->{from} = $attrs{from} unless $msg->{from};
        $msg->{osrf_xid} = $attrs{'osrf_xid'};
        $msg->{type} = $attrs{type};

    } elsif($name eq 'body') {
        $self->{xml_state} = IN_BODY;

    } elsif($name eq 'thread') {
        $self->{xml_state} = IN_THREAD;

    } elsif($name eq 'stream:stream') {
        $self->{stream_state} = CONNECT_RECV;

    } elsif($name eq 'iq') {
        if($attrs{type} and $attrs{type} eq 'result') {
            $self->{stream_state} = CONNECTED;
        }

    } elsif($name eq 'status') {
        $self->{xml_state } = IN_STATUS;

    } elsif($name eq 'stream:error') {
        $self->{stream_state} = DISCONNECTED;

    } elsif($name eq 'error') {
        $self->{message}->{err_type} = $attrs{'type'};
        $self->{message}->{err_code} = $attrs{'code'};
    }
}

sub characters {
    my($parser, $chars) = @_;
    my $self = $parser->{_parent_};
    my $state = $self->{xml_state};

    if($state == IN_BODY) {
        $self->{message}->{body} .= $chars;

    } elsif($state == IN_THREAD) {
        $self->{message}->{thread} .= $chars;

    } elsif($state == IN_STATUS) {
        $self->{message}->{status} .= $chars;
    }
}

sub end_element {
    my($parser, $name) = @_;
    my $self = $parser->{_parent_};
    $self->{xml_state} = IN_NOTHING;

    if($name eq 'message') {
        $self->push_msg($self->{message});
        $self->{message} = OpenSRF::Transport::SlimJabber::XMPPMessage->new;

    } elsif($name eq 'stream:stream') {
        $self->{stream_state} = DISCONNECTED;
    }
}


# read all the data on the jabber socket through the 
# parser and drop the resulting message
sub flush_socket {
	my $self = shift;
    return 0 unless $self->connected;

    while ($self->wait(0)) {
        # TODO remove this log line
        $logger->info("flushing data from socket...");
    }

    return $self->connected;
}



1;


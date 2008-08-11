package OpenSRF::Transport::SlimJabber::Client;

use strict;
use warnings;

use OpenSRF::EX;
use OpenSRF::Utils::Config;
use OpenSRF::Utils::Logger qw/$logger/;
use OpenSRF::Transport::SlimJabber::XMPPReader;
use OpenSRF::Transport::SlimJabber::XMPPMessage;
use IO::Socket::UNIX;
use FreezeThaw qw/freeze/;

sub DESTROY{
    shift()->disconnect;
}

=head1 NAME

OpenSRF::Transport::SlimJabber::Client

=head1 SYNOPSIS



=head1 DESCRIPTION



=cut

=head1 METHODS

=head2 new

=cut

sub new {
	my( $class, %params ) = @_;
    my $self = bless({}, ref($class) || $class);
    $self->params(\%params);
	return $self;
}

=head2 reader

=cut

sub reader {
    my($self, $reader) = @_;
    $self->{reader} = $reader if $reader;
    return $self->{reader};
}

=head2 params

=cut

sub params {
    my($self, $params) = @_;
    $self->{params} = $params if $params;
    return $self->{params};
}

=head2 socket

=cut

sub socket {
    my($self, $socket) = @_;
    $self->{socket} = $socket if $socket;
    return $self->{socket};
}

=head2 disconnect

=cut

sub disconnect {
    my $self = shift;
	$self->reader->disconnect if $self->reader;
}


=head2 gather

=cut

sub gather { 
    my $self = shift; 
    $self->process( 0 ); 
}

# -------------------------------------------------

=head2 tcp_connected

=cut

sub tcp_connected {
	my $self = shift;
    return $self->reader->tcp_connected if $self->reader;
    return 0;
}



=head2 send

=cut

sub send {
	my $self = shift;
    my $msg = OpenSRF::Transport::SlimJabber::XMPPMessage->new(@_);
    $self->reader->send($msg->to_xml);
}

=head2 initialize

=cut

sub initialize {

	my $self = shift;

	my $host	= $self->params->{host}; 
	my $port	= $self->params->{port}; 
	my $username	= $self->params->{username};
	my $resource	= $self->params->{resource};
	my $password	= $self->params->{password};

    my $jid = "$username\@$host/$resource";

	my $conf = OpenSRF::Utils::Config->current;

	my $tail = "_$$";
	$tail = "" if !$conf->bootstrap->router_name and $username eq "router";
    $resource = "$resource$tail";

    my $socket = IO::Socket::INET->new(
        PeerHost => $host,
        PeerPort => $port,
        Peer => $port,
        Proto  => 'tcp' );

    throw OpenSRF::EX::Jabber("Could not open TCP socket to Jabber server: $!")
	    unless ( $socket and $socket->connected );

    $self->socket($socket);
    $self->reader(OpenSRF::Transport::SlimJabber::XMPPReader->new($socket));
    $self->reader->connect($host, $username, $password, $resource);

    throw OpenSRF::EX::Jabber("Could not authenticate with Jabber server: $!")
	    unless ( $self->reader->connected );

	return $self;
}


=head2 construct

=cut

sub construct {
	my( $class, $app ) = @_;
	$class->peer_handle($class->new( $app )->initialize());
}


=head2 process

=cut

sub process {
	my($self, $timeout) = @_;

	$timeout ||= 0;
    $timeout = int($timeout);

	unless( $self->reader and $self->reader->connected ) {
        throw OpenSRF::EX::JabberDisconnected 
            ("This JabberClient instance is no longer connected to the server ");
	}

    return $self->reader->wait_msg($timeout);
}


=head2 flush_socket

Sets the socket to O_NONBLOCK, reads all of the data off of the
socket, the restores the sockets flags.  Returns 1 on success, 0 if
the socket isn't connected.

=cut

sub flush_socket {
	my $self = shift;
    return $self->reader->flush_socket;
}

1;



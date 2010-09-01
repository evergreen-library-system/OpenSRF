package OpenSRF::Transport::SlimJabber::PeerConnection;
use strict;
use base qw/OpenSRF::Transport::SlimJabber::Client/;
use OpenSRF::Utils::Config;
use OpenSRF::Utils::Logger qw(:level);
use OpenSRF::EX qw/:try/;

=head1 Description

Represents a single connection to a remote peer.  The 
Jabber values are loaded from the config file.  

Subclasses OpenSRF::Transport::SlimJabber::Client.

=cut

=head2 new()

	new( $appname );

	The $appname parameter tells this class how to find the correct
	Jabber username, password, etc to connect to the server.

=cut

our %apps_hash;
our $_singleton_connection;

sub retrieve { 
	my( $class, $app ) = @_;
	return $_singleton_connection;
}


sub new {
	my( $class, $app ) = @_;

	my $peer_con = $class->retrieve;
	return $peer_con if ($peer_con and $peer_con->tcp_connected);

	my $config = OpenSRF::Utils::Config->current;

	if( ! $config ) {
		throw OpenSRF::EX::Config( "No suitable config found for PeerConnection" );
	}

	my $conf			= OpenSRF::Utils::Config->current;
	my $domain = $conf->bootstrap->domain;
	my $h = $conf->env->hostname;
	OpenSRF::Utils::Logger->error("use of <domains/> is deprecated") if $conf->bootstrap->domains;

	my $username	= $conf->bootstrap->username;
	my $password	= $conf->bootstrap->passwd;
	my $port	= $conf->bootstrap->port;
	my $resource	= "${app}_drone_at_$h";
	my $host	= $domain; # XXX for now...

	if( $app eq "client" ) { $resource = "client_at_$h"; }

	OpenSRF::EX::Config->throw( "JPeer could not load all necessary values from config" )
		unless ( $username and $password and $resource and $host and $port );

	my $self = __PACKAGE__->SUPER::new( 
		username		=> $username,
		resource		=> $resource,
		password		=> $password,
		host			=> $host,
		port			=> $port,
		);	
					
	bless( $self, $class );

	$self->app($app);

	$_singleton_connection = $self;
	$apps_hash{$app} = $self;

	return $_singleton_connection;
	return $apps_hash{$app};
}

sub process {
	my $self = shift;
	my $val = $self->SUPER::process(@_);
	return 0 unless $val;
	return OpenSRF::Transport->handler($self->app, $val);
}

sub app {
	my $self = shift;
	my $app = shift;
	$self->{app} = $app if $app;
	return $self->{app};
}

1;


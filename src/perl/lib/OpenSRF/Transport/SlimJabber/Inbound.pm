package OpenSRF::Transport::SlimJabber::Inbound;
use strict;use warnings;
use base qw/OpenSRF::Transport::SlimJabber::Client/;
use OpenSRF::EX qw(:try);
use OpenSRF::Utils::Logger qw(:level);
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Utils::Config;
use Time::HiRes qw/usleep/;
use FreezeThaw qw/freeze/;

my $logger = "OpenSRF::Utils::Logger";

=head1 Description

This is the jabber connection where all incoming client requests will be accepted.
This connection takes the data, passes it off to the system then returns to take
more data.  Connection params are all taken from the config file and the values
retreived are based on the $app name passed into new().

This service should be loaded at system startup.

=cut

{
	my $unix_sock;
	sub unix_sock { return $unix_sock; }
	my $instance;

	sub new {
		my( $class, $app ) = @_;
		$class = ref( $class ) || $class;
		if( ! $instance ) {

			my $conf = OpenSRF::Utils::Config->current;
			my $domain = $conf->bootstrap->domain;
            $logger->error("use of <domains/> is deprecated") if $conf->bootstrap->domains;

			my $username	= $conf->bootstrap->username;
			my $password	= $conf->bootstrap->passwd;
			my $port			= $conf->bootstrap->port;
			my $host			= $domain;
			my $resource	= $app . '_listener_at_' . $conf->env->hostname;

            my $no_router = 0; # make this a config entry if we want to use it
			if($no_router) { 
			    # no router, only one listener running..
				$username = "router";
				$resource = $app; 
			}

			OpenSRF::Utils::Logger->transport("Inbound as $username, $password, $resource, $host, $port\n", INTERNAL );

			my $self = __PACKAGE__->SUPER::new( 
					username		=> $username,
					resource		=> $resource,
					password		=> $password,
					host			=> $host,
					port			=> $port,
					);

			$self->{app} = $app;
					
			my $client = OpenSRF::Utils::SettingsClient->new();
			my $f = $client->config_value("dirs", "sock");
			$unix_sock = join( "/", $f, 
					$client->config_value("apps", $app, "unix_config", "unix_sock" ));
			bless( $self, $class );
			$instance = $self;
		}
		return $instance;
	}

}

sub DESTROY {
	my $self = shift;
	for my $router (@{$self->{routers}}) {
		if($self->tcp_connected()) {
            $logger->info("disconnecting from router $router");
			$self->send( to => $router, body => "registering", 
				router_command => "unregister" , router_class => $self->{app} );
		}
	}
}

my $sig_pipe = 0;
	
sub listen {
	my $self = shift;
	
    $self->{routers} = [];

	try {

		my $conf = OpenSRF::Utils::Config->current;
        my $router_name = $conf->bootstrap->router_name;
		my $routers = $conf->bootstrap->routers;
        $logger->info("loading router info $routers");

        for my $router (@$routers) {
            if(ref $router) {
                if( !$router->{services} || 
                    !$router->{services}->{service} || 
                    ( 
                        ref($router->{services}->{service}) eq 'ARRAY' and 
                        grep { $_ eq $self->{app} } @{$router->{services}->{service}} )  ||
                    $router->{services}->{service} eq $self->{app}) {

                    my $name = $router->{name};
                    my $domain = $router->{domain};
                    my $target = "$name\@$domain/router";
                    push(@{$self->{routers}}, $target);
                    $logger->info( $self->{app} . " connecting to router $target");
                    $self->send( to => $target, body => "registering", router_command => "register" , router_class => $self->{app} );
                }
            } else {
                my $target = "$router_name\@$router/router";
                push(@{$self->{routers}}, $target);
                $logger->info( $self->{app} . " connecting to router $target");
                $self->send( to => $target, body => "registering", router_command => "register" , router_class => $self->{app} );
            }
        }
		
	} catch Error with {
        my $err = shift;
		$logger->error($self->{app} . ": No routers defined: $err");
		# no routers defined
	};

    my $app = $self->{app};

	$logger->info("$app inbound: going into listen loop" );

	while(1) {
	
		my $sock = $self->unix_sock();
		my $o;

		try {
			$o = $self->process(-1);

			if(!$o){
				$logger->error("$app inbound: received no data from the Jabber socket in process()");
				usleep(100000); # otherwise we loop and pound syslog logger with errors
			}

		} catch OpenSRF::EX::JabberDisconnected with {

			$logger->error("$app inbound: process lost its jabber connection.  Attempting to reconnect...");
			$self->initialize;
			$o = undef;
		};

        next unless $o;

        while(1) {
            # keep trying to deliver the message until we succeed

            my $socket = IO::Socket::UNIX->new( Peer => $sock  );

            unless($socket and $socket->connected) {
                $logger->error("$app inbound: unable to connect to inbound socket $sock: $!");
                usleep(50000); # 50 msec
                next;
            }

            # block until the pipe is ready for writing
            my $outfile = ''; 
            vec($outfile, $socket->fileno, 1) = 1;
            my $nfound = select(undef, $outfile, undef, undef);

            next unless $nfound; # should not happen since we're blocking

            if($nfound == -1) { # select failed
                $logger->error("$app inbound: unable to write to socket: $!");
                usleep(50000); # 50 msec
                next;
            }

            $sig_pipe = 0;
            local $SIG{'PIPE'} = sub { $sig_pipe = 1; };
            print $socket freeze($o);

            if($sig_pipe) {
                # The attempt to write to the socket failed.  Wait a short time then try again.
                # Don't bother closing the socket, it will only cause grief
                $logger->error("$app inbound: got SIGPIPE, will retry after a short wait..."); 
                usleep(50000); # 50 msec
                next;
            } 
                
            $socket->close;
            last;
        }
	}

    $logger->error("$app inbound: exited process loop");
}

1;


package OpenSRF::System;
use strict; use warnings;
use OpenSRF;
use base 'OpenSRF';
use OpenSRF::Utils::Logger qw($logger);
use OpenSRF::Transport::Listener;
use OpenSRF::Transport;
use OpenSRF::Utils;
use OpenSRF::EX qw/:try/;
use POSIX qw/setsid :sys_wait_h/;
use OpenSRF::Utils::Config; 
use OpenSRF::Utils::SettingsParser;
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Application;
use OpenSRF::Server;

my $bootstrap_config_file;
sub import {
    my( $self, $config ) = @_;
    $bootstrap_config_file = $config;
}

$| = 1;

sub DESTROY {}

sub load_bootstrap_config {
    return if OpenSRF::Utils::Config->current;

    die "Please provide a bootstrap config file to OpenSRF::System\n"
        unless $bootstrap_config_file;

    OpenSRF::Utils::Config->load(config_file => $bootstrap_config_file);
    OpenSRF::Utils::JSON->register_class_hint(name => "OpenSRF::Application", hint => "method", type => "hash", strip => ['session']);
    OpenSRF::Transport::PeerHandle->set_peer_client("OpenSRF::Transport::Redis::PeerConnection");
    OpenSRF::Application->server_class('client');
    # Read in a shared portion of the config file
    # for later use in log parameter redaction
    $OpenSRF::Application::shared_conf = OpenSRF::Utils::Config->load(
        'config_file' => OpenSRF::Utils::Config->current->FILE,
        'nocache' => 1,
        'force' => 1,
        'base_path' => '/config/shared'
    );
}

# ----------------------------------------------
# Bootstraps a single client connection.  
# named params are 'config_file' and 'client_name'
sub bootstrap_client {
    my $self = shift;

    my $con = OpenSRF::Transport::PeerHandle->retrieve;
    if ($con) {
        # flush the socket to force a non-blocking read
        # and to clear out any unanticipated leftovers
        eval { $con->flush_socket };
        return if $con->connected;
        $con->reset;
    }

    my %params = @_;

    $bootstrap_config_file = 
        $params{config_file} || $bootstrap_config_file;

    load_bootstrap_config();
    OpenSRF::Utils::Logger::set_config();
    OpenSRF::Transport::PeerHandle->construct;
}

sub connected {
    if (my $con = OpenSRF::Transport::PeerHandle->retrieve) {
        return 1 if $con->connected;
    }
    return 0;
}

sub run_service {
    my($class, $service, $pid_dir) = @_;

    $0 = "OpenSRF Listener [$service]";

    # temp connection to use for application initialization
    OpenSRF::System->bootstrap_client(client_name => "system_client");

    my $sclient = OpenSRF::Utils::SettingsClient->new;
    my $getval = sub { $sclient->config_value(apps => $service => @_); };

    my $impl = $getval->('implementation');

    OpenSRF::Application::server_class($service);
    OpenSRF::Application->application_implementation($impl);
    OpenSRF::Utils::JSON->register_class_hint(name => $impl, hint => $service, type => 'hash');
    OpenSRF::Application->application_implementation->initialize()
        if (OpenSRF::Application->application_implementation->can('initialize'));

    # kill the temp connection
    OpenSRF::Transport::PeerHandle->retrieve->disconnect;
    
    # if this service does not want stderr output, it will be redirected to /dev/null
    my $disable_stderr = $getval->('disable_stderr') || '';
    my $stderr_path = ($disable_stderr =~ /true/i) ? undef : $sclient->config_value(dirs => 'log');

    my $server = OpenSRF::Server->new(
        $service,
        keepalive => $getval->('keepalive') || 5,
        max_requests =>  $getval->(unix_config => 'max_requests') || 10000,
        max_children =>  $getval->(unix_config => 'max_children') || 20,
        min_children =>  $getval->(unix_config => 'min_children') || 1,
        min_spare_children =>  $getval->(unix_config => 'min_spare_children'),
        max_spare_children =>  $getval->(unix_config => 'max_spare_children'),
        stderr_log_path => $stderr_path
    );

    while(1) {
        eval { $server->run; };
        # we only arrive here if the server died a painful death
        $logger->error("server: died with error $@");
        $server->cleanup(1);
        $logger->info("server: restarting after fatal crash...");
        sleep 2;
    }
}


1;

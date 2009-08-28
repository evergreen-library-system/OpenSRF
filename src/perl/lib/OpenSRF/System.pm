package OpenSRF::System;
use strict; use warnings;
use OpenSRF;
use base 'OpenSRF';
use OpenSRF::Utils::Logger qw(:level);
use OpenSRF::Transport::Listener;
use OpenSRF::Transport;
use OpenSRF::UnixServer;
use OpenSRF::Utils;
use OpenSRF::EX qw/:try/;
use POSIX qw/setsid :sys_wait_h/;
use OpenSRF::Utils::Config; 
use OpenSRF::Utils::SettingsParser;
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Application;
use Net::Server::PreFork;

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
	OpenSRF::Utils::JSON->register_class_hint(name => "OpenSRF::Application", hint => "method", type => "hash");
	OpenSRF::Transport->message_envelope("OpenSRF::Transport::SlimJabber::MessageWrapper");
	OpenSRF::Transport::PeerHandle->set_peer_client("OpenSRF::Transport::SlimJabber::PeerConnection");
	OpenSRF::Transport::Listener->set_listener("OpenSRF::Transport::SlimJabber::Inbound");
	OpenSRF::Application->server_class('client');
}

# ----------------------------------------------
# Bootstraps a single client connection.  
# named params are 'config_file' and 'client_name'
sub bootstrap_client {
	my $self = shift;

	my $con = OpenSRF::Transport::PeerHandle->retrieve;
    return if $con and $con->tcp_connected;

	my %params = @_;

	$bootstrap_config_file = 
		$params{config_file} || $bootstrap_config_file;

	my $app = $params{client_name} || "client";

	load_bootstrap_config();
	OpenSRF::Utils::Logger::set_config();
	OpenSRF::Transport::PeerHandle->construct($app);
}

sub connected {
	if (my $con = OpenSRF::Transport::PeerHandle->retrieve) {
		return 1 if $con->tcp_connected;
	}
	return 0;
}

1;

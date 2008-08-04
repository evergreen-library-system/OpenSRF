#!/usr/bin/perl
# ---------------------------------------------------------------
# Copyright (C) 2008  Georgia Public Library Service
# Bill Erickson <erickson@esilibrary.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# ---------------------------------------------------------------
use strict; use warnings;
use Getopt::Long;
use Net::Domain qw/hostfqdn/;
use POSIX qw/setsid :sys_wait_h/;
use OpenSRF::Utils::Logger q/$logger/;
use OpenSRF::System;
use OpenSRF::Transport::PeerHandle;
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Transport::Listener;
use OpenSRF::Utils;
use OpenSRF::Utils::Config;

my $action = undef;
my $service = undef;
my $config = undef;
my $pid_dir = '/tmp';
my $no_daemon = 0;
my $help = 0;
my $sclient;
my $hostname = hostfqdn();

GetOptions(
    'action=s' => \$action,
    'service=s' => \$service,
    'config=s' => \$config,
    'pid_dir=s' => \$pid_dir,
    'no_daemon' => \$no_daemon,
    'help' => \$help,
);

my $pid_file = "$pid_dir/$service.pid" if $pid_dir and $service;

sub haltme {
    kill('INT', -$$); #kill all in process group
    exit;
};
$SIG{INT} = \&haltme;
$SIG{TERM} = \&haltme;

# stop a specific service
sub do_stop {
    if(-e $pid_file) {
        my $pid = `cat $pid_file`;
        kill('INT', $pid);
        waitpid($pid, 0);
        unlink $pid_file;
    } else {
        msg("$service not running");
    }
}

# start a specific service
sub do_start {

    OpenSRF::System->bootstrap_client(config_file => $config);

    die "Unable to bootstrap client for requests\n"
        unless OpenSRF::Transport::PeerHandle->retrieve;

    load_settings() if $service eq 'opensrf.settings';

    my $sclient = OpenSRF::Utils::SettingsClient->new;
    my $apps = $sclient->config_value("activeapps", "appname");
    OpenSRF::Transport::PeerHandle->retrieve->disconnect;

    if($apps) {
        $apps = [$apps] unless ref $apps;
        for my $app (@$apps) {
            if($app eq $service) {
                if($sclient->config_value('apps', $app, 'language') =~ /perl/i) {
                    do_daemon() unless $no_daemon;
                    launch_net_server();
                    launch_listener();
                    $0 = "OpenSRF controller [$service]";
                    while(my $pid = waitpid(-1, 0)) {
                        $logger->debug("Cleaning up Perl $service process $pid");
                    }
                }
            }
        }
    }

    msg("$service is not configured to run on $hostname");
}

# daemonize us
sub do_daemon {
    exit if OpenSRF::Utils::safe_fork();
    chdir('/');
    setsid();
    close STDIN;
    close STDOUT;
    close STDERR;
    `echo $$ > $pid_file`;
}

# parses the local settings file
sub load_settings {
    my $conf = OpenSRF::Utils::Config->current;
    my $cfile = $conf->bootstrap->settings_config;
    my $parser = OpenSRF::Utils::SettingsParser->new();
    $parser->initialize( $cfile );
    $OpenSRF::Utils::SettingsClient::host_config =
        $parser->get_server_config($conf->env->hostname);
}

# starts up the unix::server master process
sub launch_net_server {
    push @OpenSRF::UnixServer::ISA, 'Net::Server::PreFork';
    unless(OpenSRF::Utils::safe_fork()) {
        $0 = "OpenSRF Drone [$service]";
        OpenSRF::UnixServer->new($service)->serve;
        exit;
    }
    return 1;
}

# starts up the inbound listener process
sub launch_listener {
    unless(OpenSRF::Utils::safe_fork()) {
        $0 = "OpenSRF listener [$service]";
        OpenSRF::Transport::Listener->new($service)->initialize->listen;
        exit;
    }
    return 1;
}

sub msg {
    my $m = shift;
    print "* $m\n";
}

sub do_help {
    print <<HELP;

    Usage: perl $0 --pid_dir /var/run/opensrf --config /etc/opensrf/opensrf_core.xml --service opensrf.settings --action start

    --action <action>
        Actions include start, stop, restart, and start_all, stop_all, and restart_all

    --service <service>
        Specifies which OpenSRF service to control

    --config <file>
        OpenSRF configuration file 
        
    --pid_dir <dir>
        Directory where process-specific PID files are kept
        
    --no_daemon
        Do not detach and run as a daemon process.  Useful for debugging.
        
    --help
        Print this help message
HELP
exit;
}


do_help() if $help or not $action;
do_start() if $action eq 'start';
do_stop() if $action eq 'stop';
do_stop() and do_start() if $action eq 'restart';



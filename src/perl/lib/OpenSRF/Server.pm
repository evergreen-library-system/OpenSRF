# ----------------------------------------------------------------
# Copyright (C) 2010 Equinox Software, Inc.
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
# ----------------------------------------------------------------
package OpenSRF::Server;
use strict;
use warnings;
use OpenSRF::Transport;
use OpenSRF::Application;
use OpenSRF::Utils::Config;
use OpenSRF::Transport::PeerHandle;
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Utils::Logger qw($logger);
use OpenSRF::Transport::SlimJabber::Client;
use Encode;
use POSIX qw/:sys_wait_h :errno_h/;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use IO::Select;
use Socket;
our $chatty = 1; # disable for production

use constant STATUS_PIPE_DATA_SIZE => 12;

sub new {
    my($class, $service, %args) = @_;
    my $self = bless(\%args, $class);

    $self->{service}        = $service; # service name
    $self->{num_children}   = 0; # number of child processes
    $self->{osrf_handle}    = undef; # xmpp handle
    $self->{routers}        = []; # list of registered routers
    $self->{active_list}    = []; # list of active children
    $self->{idle_list}      = []; # list of idle children
    $self->{pid_map}        = {}; # map of child pid to child for cleaner access

    $self->{stderr_log} = $self->{stderr_log_path} . "/${service}_stderr.log" 
        if $self->{stderr_log_path};

    $self->{min_spare_children} ||= 0;

    $self->{max_spare_children} = $self->{min_spare_children} + 1 if
        $self->{max_spare_children} and
        $self->{max_spare_children} <= $self->{min_spare_children};

    return $self;
}

# ----------------------------------------------------------------
# Disconnects from routers and waits for child processes to exit.
# ----------------------------------------------------------------
sub cleanup {
    my $self = shift;
    my $no_exit = shift;

    $logger->info("server: shutting down and cleaning up...");

    # don't get sidetracked by signals while we're cleaning up.
    # it could result in unexpected behavior with list traversal
    $SIG{CHLD} = 'IGNORE';

    # terminate the child processes
    $self->kill_child($_) for
        (@{$self->{idle_list}}, @{$self->{active_list}});

    # de-register routers
    $self->unregister_routers;

    $self->{osrf_handle}->disconnect;

    # clean up our dead children
    $self->reap_children(1);

    exit(0) unless $no_exit;
}


# ----------------------------------------------------------------
# Waits on the jabber socket for inbound data from the router.
# Each new message is passed off to a child process for handling.
# At regular intervals, wake up for min/max spare child maintenance
# ----------------------------------------------------------------
sub run {
    my $self = shift;

	$logger->set_service($self->{service});

    $SIG{$_} = sub { $self->cleanup; } for (qw/INT TERM QUIT/);
    $SIG{CHLD} = sub { $self->reap_children(); };

    $self->spawn_children;
    $self->build_osrf_handle;
    $self->register_routers;
    my $wait_time = 1;

    # main server loop
    while(1) {

        $self->check_status;
        $self->{child_died} = 0;

        my $msg = $self->{osrf_handle}->process($wait_time);

        # we woke up for any reason, reset the wait time to allow
        # for idle maintenance as necessary
        $wait_time = 1;

        if($msg) {

            if(my $child = pop(@{$self->{idle_list}})) {

                # we have an idle child to handle the request
                $chatty and $logger->internal("server: passing request to idle child $child");
                push(@{$self->{active_list}}, $child);
                $self->write_child($child, $msg);

            } elsif($self->{num_children} < $self->{max_children}) {

                # spawning a child to handle the request
                $chatty and $logger->internal("server: spawning child to handle request");
                $self->write_child($self->spawn_child(1), $msg);

            } else {

                $logger->warn("server: no children available, waiting...");
                $self->check_status(1); # block until child is available

                my $child = pop(@{$self->{idle_list}});
                push(@{$self->{active_list}}, $child);
                $self->write_child($child, $msg);
            }

        } else {

            # don't perform idle maint immediately when woken by SIGCHLD
            unless($self->{child_died}) {

                # when we hit equilibrium, there's no need for regular
                # maintenance, so set wait_time to 'forever'
                $wait_time = -1 if 
                    !$self->perform_idle_maintenance and # no maintenance performed this time
                    @{$self->{active_list}} == 0; # no active children 
            }
        }
    }
}

# ----------------------------------------------------------------
# Launch a new spare child or kill an extra spare child.  To
# prevent large-scale spawning or die-offs, spawn or kill only
# 1 process per idle maintenance loop.
# Returns true if any idle maintenance occurred, 0 otherwise
# ----------------------------------------------------------------
sub perform_idle_maintenance {
    my $self = shift;

    $chatty and $logger->internal(
        sprintf(
            "server: %d idle, %d active, %d min_spare, %d max_spare in idle maintenance",
            scalar(@{$self->{idle_list}}), 
            scalar(@{$self->{active_list}}),
            $self->{min_spare_children},
            $self->{max_spare_children}
        )
    );

    # spawn 1 spare child per maintenance loop if necessary
    if( $self->{min_spare_children} and
        $self->{num_children} < $self->{max_children} and
        scalar(@{$self->{idle_list}}) < $self->{min_spare_children} ) {

        $chatty and $logger->internal("server: spawning spare child");
        $self->spawn_child;
        return 1;

    # kill 1 excess spare child per maintenance loop if necessary
    } elsif($self->{max_spare_children} and
            $self->{num_children} > $self->{min_children} and
            scalar(@{$self->{idle_list}}) > $self->{max_spare_children} ) {

        $chatty and $logger->internal("server: killing spare child");
        $self->kill_child;
        return 1;
    }

    return 0;
}

sub kill_child {
    my $self = shift;
    my $child = shift || pop(@{$self->{idle_list}}) or return;
    $chatty and $logger->internal("server: killing child $child");
    kill('TERM', $child->{pid});
}

# ----------------------------------------------------------------
# Jabber connection inbound message arrive on.
# ----------------------------------------------------------------
sub build_osrf_handle {
    my $self = shift;

    my $conf = OpenSRF::Utils::Config->current;
    my $username = $conf->bootstrap->username;
    my $password = $conf->bootstrap->passwd;
    my $domain = $conf->bootstrap->domain;
    my $port = $conf->bootstrap->port;
    my $resource = $self->{service} . '_listener_' . $conf->env->hostname;

    $logger->debug("server: inbound connecting as $username\@$domain/$resource on port $port");

    $self->{osrf_handle} =
        OpenSRF::Transport::SlimJabber::Client->new(
            username => $username,
            resource => $resource,
            password => $password,
            host => $domain,
            port => $port,
        );

    $self->{osrf_handle}->initialize;
}


# ----------------------------------------------------------------
# Sends request data to a child process
# ----------------------------------------------------------------
sub write_child {
    my($self, $child, $msg) = @_;
    my $xml = decode_utf8($msg->to_xml);
    syswrite($child->{pipe_to_child}, encode_utf8($xml));
}

# ----------------------------------------------------------------
# Checks to see if any child process has reported its availability
# In blocking mode, blocks until a child has reported.
# ----------------------------------------------------------------
sub check_status {
    my($self, $block) = @_;

    return unless @{$self->{active_list}};

    my $read_set = IO::Select->new;
    $read_set->add($_->{pipe_to_child}) for @{$self->{active_list}};

    my @pids;

    while (1) {

        # if can_read or sysread is interrupted while bloking, go back and 
        # wait again until we have at least 1 free child

        if(my @handles = $read_set->can_read(($block) ? undef : 0)) {
            my $pid = '';
            for my $pipe (@handles) {
                sysread($pipe, $pid, STATUS_PIPE_DATA_SIZE) or next;
                push(@pids, int($pid));
            }
        }

        last unless $block and !@pids;
    }

    return unless @pids;

    $chatty and $logger->internal("server: ".scalar(@pids)." children reporting for duty: (@pids)");

    my $child;
    my @new_actives;

    # move the children from the active list to the idle list
    for my $proc (@{$self->{active_list}}) {
        if(grep { $_ == $proc->{pid} } @pids) {
            push(@{$self->{idle_list}}, $proc);
        } else {
            push(@new_actives, $proc);
        }
    }

    $self->{active_list} = [@new_actives];

    $chatty and $logger->internal(sprintf(
        "server: %d idle and %d active children after status update",
            scalar(@{$self->{idle_list}}), scalar(@{$self->{active_list}})));
}

# ----------------------------------------------------------------
# Cleans up any child processes that have exited.
# In shutdown mode, block until all children have washed ashore
# ----------------------------------------------------------------
sub reap_children {
    my($self, $shutdown) = @_;
    $self->{child_died} = 1;

    while(1) {

        my $pid = waitpid(-1, ($shutdown) ? 0 : WNOHANG);
        last if $pid <= 0;

        $chatty and $logger->internal("server: reaping child $pid");

        my $child = $self->{pid_map}->{$pid};

        close($child->{pipe_to_parent});
        close($child->{pipe_to_child});

        $self->{active_list} = [ grep { $_->{pid} != $pid } @{$self->{active_list}} ];
        $self->{idle_list} = [ grep { $_->{pid} != $pid } @{$self->{idle_list}} ];

        $self->{num_children}--;
        delete $self->{pid_map}->{$pid};
        delete $child->{$_} for keys %$child; # destroy with a vengeance
    }

    $self->spawn_children unless $shutdown;

    $chatty and $logger->internal(sprintf(
        "server: %d idle and %d active children after reap_children",
            scalar(@{$self->{idle_list}}), scalar(@{$self->{active_list}})));

}

# ----------------------------------------------------------------
# Spawn up to max_children processes
# ----------------------------------------------------------------
sub spawn_children {
    my $self = shift;
    $self->spawn_child while $self->{num_children} < $self->{min_children};
}

# ----------------------------------------------------------------
# Spawns a new child.  If $active is set, the child goes directly
# into the active_list.
# ----------------------------------------------------------------
sub spawn_child {
    my($self, $active) = @_;

    my $child = OpenSRF::Server::Child->new($self);

    # socket for sending message data to the child
    if(!socketpair(
        $child->{pipe_to_child},
        $child->{pipe_to_parent},
        AF_UNIX, SOCK_STREAM, PF_UNSPEC)) {
            $logger->error("server: error creating data socketpair: $!");
            return undef;
    }

    $child->{pipe_to_child}->autoflush(1);
    $child->{pipe_to_parent}->autoflush(1);

    $child->{pid} = fork();

    if($child->{pid}) { # parent process
        $self->{num_children}++;
        $self->{pid_map}->{$child->{pid}} = $child;

        if($active) {
            push(@{$self->{active_list}}, $child);
        } else {
            push(@{$self->{idle_list}}, $child);
        }

        $chatty and $logger->internal("server: server spawned child $child with ".$self->{num_children}." total children");

        return $child;

    } else { # child process

        $SIG{$_} = 'DEFAULT' for (qw/INT TERM QUIT HUP/);

        if($self->{stderr_log}) {

            $chatty and $logger->internal("server: redirecting STDERR to " . $self->{stderr_log});

            close STDERR;
            unless( open(STDERR, '>>' . $self->{stderr_log}) ) {
                $logger->error("server: unable to open STDERR log file: " . $self->{stderr_log} . " : $@");
                open STDERR, '>/dev/null'; # send it back to /dev/null
            }
        }

        $child->{pid} = $$;
        eval {
            $child->init;
            $child->run;
            OpenSRF::Transport::PeerHandle->retrieve->disconnect;
        };
        $logger->error("server: child process died: $@") if $@;
        exit(0);
    }
}

# ----------------------------------------------------------------
# Sends the register command to the configured routers
# ----------------------------------------------------------------
sub register_routers {
    my $self = shift;

    my $conf = OpenSRF::Utils::Config->current;
    my $routers = $conf->bootstrap->routers;
    my $router_name = $conf->bootstrap->router_name;
    my @targets;

    for my $router (@$routers) {
        if(ref $router) {

            if( !$router->{services} ||
                !$router->{services}->{service} ||
                (
                    ref($router->{services}->{service}) eq 'ARRAY' and
                    grep { $_ eq $self->{service} } @{$router->{services}->{service}}
                )  || $router->{services}->{service} eq $self->{service}) {

                my $name = $router->{name};
                my $domain = $router->{domain};
                push(@targets, "$name\@$domain/router");
            }

        } else {
            push(@targets, "$router_name\@$router/router");
        }
    }

    foreach (@targets) {
        $logger->info("server: registering with router $_");
        $self->{osrf_handle}->send(
            to => $_,
            body => 'registering',
            router_command => 'register',
            router_class => $self->{service}
        );
    }

    $self->{routers} = \@targets;
}

# ----------------------------------------------------------------
# Sends the unregister command to any routers we have registered
# with.
# ----------------------------------------------------------------
sub unregister_routers {
    my $self = shift;
    return unless $self->{osrf_handle}->tcp_connected;

	for my $router (@{$self->{routers}}) {
        $logger->info("server: disconnecting from router $router");
        $self->{osrf_handle}->send(
            to => $router,
            body => "unregistering",
            router_command => "unregister",
            router_class => $self->{service}
        );
    }
}


package OpenSRF::Server::Child;
use strict;
use warnings;
use OpenSRF::Transport;
use OpenSRF::Application;
use OpenSRF::Transport::PeerHandle;
use OpenSRF::Transport::SlimJabber::XMPPMessage;
use OpenSRF::Utils::Logger qw($logger);
use OpenSRF::DomainObject::oilsResponse qw/:status/;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Time::HiRes qw(time);
use POSIX qw/:sys_wait_h :errno_h/;

use overload '""' => sub { return '[' . shift()->{pid} . ']'; };

sub new {
    my($class, $parent) = @_;
    my $self = bless({}, $class);
    $self->{pid} = 0; # my process ID
    $self->{parent} = $parent; # Controller parent process
    $self->{num_requests} = 0; # total serviced requests
    return $self;
}

sub set_nonblock {
    my($self, $fh) = @_;
    my  $flags = fcntl($fh, F_GETFL, 0);
    fcntl($fh, F_SETFL, $flags | O_NONBLOCK);
}

sub set_block {
    my($self, $fh) = @_;
    my  $flags = fcntl($fh, F_GETFL, 0);
    $flags &= ~O_NONBLOCK;
    fcntl($fh, F_SETFL, $flags);
}

# ----------------------------------------------------------------
# Connects to Jabber and runs the application child_init
# ----------------------------------------------------------------
sub init {
    my $self = shift;
    my $service = $self->{parent}->{service};
    $0 = "OpenSRF Drone [$service]";
    OpenSRF::Transport::PeerHandle->construct($service);
	OpenSRF::Application->application_implementation->child_init
		if (OpenSRF::Application->application_implementation->can('child_init'));
}

# ----------------------------------------------------------------
# Waits for messages from the parent process, handles the message,
# then goes into the keepalive loop if this is a stateful session.
# When max_requests is hit, the process exits.
# ----------------------------------------------------------------
sub run {
    my $self = shift;
    my $network = OpenSRF::Transport::PeerHandle->retrieve;

    # main child run loop.  Ends when this child hits max requests.
    while(1) {

        my $data = $self->wait_for_request or next;

        # Update process name to show activity
        my $orig_name = $0;
        $0 = "$0*";

        # Discard extraneous data from the jabber socket
        if(!$network->flush_socket()) {
            $logger->error("server: network disconnected!  child dropping request and exiting: $data");
            exit;
        }

        my $session = OpenSRF::Transport->handler(
            $self->{parent}->{service},
            OpenSRF::Transport::SlimJabber::XMPPMessage->new(xml => $data)
        );

        $self->keepalive_loop($session);

        last if ++$self->{num_requests} == $self->{parent}->{max_requests};

        # Tell the parent process we are available to process requests
        $self->send_status;

        # Repair process name
        $0 = $orig_name;
    }

    $chatty and $logger->internal("server: child process shutting down after reaching max_requests");

	OpenSRF::Application->application_implementation->child_exit
		if (OpenSRF::Application->application_implementation->can('child_exit'));
}

# ----------------------------------------------------------------
# waits for a request data on the parent pipe and returns it.
# ----------------------------------------------------------------
sub wait_for_request {
    my $self = shift;

    my $data = '';
    my $read_size = 1024;
    my $nonblock = 0;

    while(1) {
        # Start out blocking, when data is available, read it all

        my $buf = '';
        my $n = sysread($self->{pipe_to_parent}, $buf, $read_size);

        unless(defined $n) {
            $logger->error("server: error reading data pipe: $!") unless EAGAIN == $!; 
            last;
        }

        last if $n <= 0; # no data left to read

        $data .= $buf;

        last if $n < $read_size; # done reading all data

        $self->set_nonblock($self->{pipe_to_parent}) unless $nonblock;
        $nonblock = 1;
    }

    $self->set_block($self->{pipe_to_parent}) if $nonblock;
    return $data;
}


# ----------------------------------------------------------------
# If this is a stateful opensrf session, wait up to $keepalive
# seconds for subsequent requests from the client
# ----------------------------------------------------------------
sub keepalive_loop {
    my($self, $session) = @_;
    my $keepalive = $self->{parent}->{keepalive};

    while($session->state and $session->state == $session->CONNECTED) {

        unless( $session->queue_wait($keepalive) ) {

            # client failed to disconnect before timeout
            $logger->info("server: no request was received in $keepalive seconds, exiting stateful session");

            my $res = OpenSRF::DomainObject::oilsConnectStatus->new(
                status => "Disconnected on timeout",
                statusCode => STATUS_TIMEOUT
            );

            $session->status($res);
            $session->state($session->DISCONNECTED);
            last;
        }
    }

    $chatty and $logger->internal("server: child done with request(s)");
    $session->kill_me;
}

# ----------------------------------------------------------------
# Report our availability to our parent process
# ----------------------------------------------------------------
sub send_status {
    my $self = shift;
    syswrite(
        $self->{pipe_to_parent},
        sprintf("%*s", OpenSRF::Server::STATUS_PIPE_DATA_SIZE, $self->{pid})
    );
}


1;

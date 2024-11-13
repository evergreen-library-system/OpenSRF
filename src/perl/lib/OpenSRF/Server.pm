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
use OpenSRF::Transport::Redis::Client;
use Encode;
use POSIX qw/:sys_wait_h :errno_h/;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Time::HiRes qw/usleep/;
use IO::Select;
use Socket;
our $chatty = 1; # disable for production

use constant STATUS_PIPE_DATA_SIZE => 12;
use constant WRITE_PIPE_DATA_SIZE  => 12;

sub new {
    my($class, $service, %args) = @_;
    my $self = bless(\%args, $class);

    $self->{service}        = $service; # service name
    $self->{num_children}   = 0; # number of child processes
    $self->{osrf_handle}    = undef; # xmpp handle
    $self->{routers}        = []; # list of registered routers
    $self->{active_list}    = []; # list of active children
    $self->{idle_list}      = []; # list of idle children
    $self->{zombie_list}    = []; # list of reaped children to clean up
    $self->{sighup_pending} = [];
    $self->{pid_map}        = {}; # map of child pid to child for cleaner access
    $self->{sig_pipe}       = 0;  # true if last syswrite failed

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
    my $graceful = shift;

    $logger->info("server: shutting down and cleaning up...");

    # de-register routers
    $self->unregister_routers;

    if ($graceful) {
        # graceful shutdown waits for all active 
        # children to complete their in-process tasks.

        while (@{$self->{active_list}}) {
            $logger->info("server: graceful shutdown with ".
                @{$self->{active_list}}." active children...");

            # block until a child is becomes available
            $logger->info("waiting for child procs to clear in graceful shutdown");
            $self->check_status(1);
        }
        $logger->info("server: all clear for graceful shutdown");
    }

    # don't get sidetracked by signals while we're cleaning up.
    # it could result in unexpected behavior with list traversal
    $SIG{CHLD} = 'IGNORE';

    # terminate the child processes
    $self->kill_child($_) for
        (@{$self->{idle_list}}, @{$self->{active_list}});

    $self->{osrf_handle}->disconnect;

    # clean up our dead children
    $self->reap_children(1);

    exit(0) unless $no_exit;
}

# ----------------------------------------------------------------
# SIGHUP handler.  Kill all idle children.  Copy list of active
# children into sighup_pending list for later cleanup.
# ----------------------------------------------------------------
sub handle_sighup {
    my $self = shift;
    $logger->info("server: caught SIGHUP; reloading children");

    # reload the opensrf config
    # note: calling ::Config->load() results in ever-growing
    # package names, which eventually causes an exception
    OpenSRF::Utils::Config->current->_load(
        force => 1,
        config_file => OpenSRF::Utils::Config->current->FILE
    );

    # force-reload the logger config
    OpenSRF::Utils::Logger::set_config(1);

    # copy active list into pending list for later cleanup
    $self->{sighup_pending} = [ @{$self->{active_list}} ];

    # idle_list will be modified as children are reaped.
    my @idle = @{$self->{idle_list}};

    # idle children are the reaper's plaything
    $self->kill_child($_) for @idle;
}

# ----------------------------------------------------------------
# Waits on the redis socket for inbound data from the router.
# Each new message is passed off to a child process for handling.
# At regular intervals, wake up for min/max spare child maintenance
# ----------------------------------------------------------------
sub run {
    my $self = shift;

    $logger->set_service($self->{service});

    $SIG{$_} = sub { $self->cleanup; } for (qw/INT QUIT/);
    $SIG{TERM} = sub { $self->cleanup(0, 1); };
    $SIG{CHLD} = sub { $self->reap_children(); };
    $SIG{HUP} = sub { $self->handle_sighup(); };
    $SIG{USR1} = sub { $self->unregister_routers; };
    $SIG{USR2} = sub { $self->register_routers; };

    $self->spawn_children;
    $self->build_osrf_handle;
    $self->register_routers;
    my $wait_time = 1;

    # main server loop
    while (1) {

        $self->check_status;
        $self->squash_zombies;
        $self->{child_died} = 0;

        my $msg = $self->{osrf_handle}->process($wait_time, $self->{service});

        # we woke up for any reason, reset the wait time to allow
        # for more frequent idle maintenance checks.
        $wait_time = 1;

        if($msg) {

            if ($msg->type and $msg->type eq 'error') {
                $logger->info("server: Listener received an XMPP error ".
                    "message.  Likely a bounced message. sender=".$msg->from);

            } elsif(my $child = pop(@{$self->{idle_list}})) {

                # we have an idle child to handle the request
                $chatty and $logger->internal("server: passing request to idle child $child");
                push(@{$self->{active_list}}, $child);
                $self->write_child($child, $msg);

            } elsif($self->{num_children} < $self->{max_children}) {

                # spawning a child to handle the request
                $chatty and $logger->internal("server: spawning child to handle request");
                $self->write_child($self->spawn_child(1), $msg);

            } else {
                $logger->warn("server: no children available, waiting... consider increasing " .
                    "max_children for this application higher than $self->{max_children} ".
                    "in the OpenSRF configuration if this message occurs frequently");
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
                #
                # Avoid indefinite waiting here -- Redis client
                # gracefully handles interrupts and immediately goes
                # back to listening after the signal handler is
                # complete.  In our case, the signal handler may include
                # un-registering with routers, which requires the Redis
                # client to wait for an ACK from the Redis server.
                # However, it will never receive the ack because our
                # client is already blocking on an BLPOP call wiating
                # for a new request.  In future, we could replace
                # signals with messages sent directly to listeners
                # telling them to shutdown.
                $wait_time = 5 if 
                    !$self->perform_idle_maintenance and # no maintenance performed this time
                    @{$self->{active_list}} == 0; # no active children 
            }
        }
    }
}

# ----------------------------------------------------------------
# Finish destroying objects for reaped children
# ----------------------------------------------------------------
sub squash_zombies {
    my $self = shift;

    my $squashed = 0;
    while (my $child = shift @{$self->{zombie_list}}) {
        delete $child->{$_} for keys %$child; # destroy with a vengeance
        $squashed++;
    }
    $chatty and $logger->internal("server: squashed $squashed zombies");
}

# ----------------------------------------------------------------
# Launch a new spare child or kill an extra spare child.  To
# prevent large-scale spawning or die-offs, spawn or kill only
# 1 process per idle maintenance loop.
# Returns true if any idle maintenance occurred, 0 otherwise
# ----------------------------------------------------------------
sub perform_idle_maintenance {
    my $self = shift;

    $chatty and $logger->internal(sub{return
        sprintf(
            "server: %d idle, %d active, %d min_spare, %d max_spare in idle maintenance",
            scalar(@{$self->{idle_list}}), 
            scalar(@{$self->{active_list}}),
            $self->{min_spare_children},
            $self->{max_spare_children}
        )
    });

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
# Redis connection inbound message arrive on.
# ----------------------------------------------------------------
sub build_osrf_handle {
    my $self = shift;
    $self->{osrf_handle} =
        OpenSRF::Transport::Redis::Client->new($self->{service});
}


# ----------------------------------------------------------------
# Sends request data to a child process
# ----------------------------------------------------------------
sub write_child {
    my($self, $child, $msg) = @_;
    my $json = $msg->to_json;

    # tell the child how much data to expect, minus the header
    my $write_size;
    {use bytes; $write_size = length($json)}
    $write_size = sprintf("%*s", WRITE_PIPE_DATA_SIZE, $write_size);

    for (0..2) {

        $self->{sig_pipe} = 0;
        local $SIG{'PIPE'} = sub { $self->{sig_pipe} = 1; };

        # In rare cases a child can die between creation and first
        # write, typically a result of a bus connect error.  Before
        # sending data to each child, confirm it's still alive.  If it's
        # not, log the error and drop the message to prevent the parent
        # process from dying.
        # When a child dies, all of its attributes are deleted,
        # so the lack of a pid means the child is dead.
        if (!$child->{pid}) {
            $logger->error("server: child is dead in write_child(). ".
                "unable to send message: $json");
            return; # avoid syswrite crash
        }

        # send message to child data pipe
        syswrite($child->{pipe_to_child}, $write_size . $json);

        last unless $self->{sig_pipe};
        $logger->error("server: got SIGPIPE writing to $child, retrying...");
        usleep(50000); # 50 msec
    }

    $logger->error("server: unable to send request message to child $child") if $self->{sig_pipe};
}

# ----------------------------------------------------------------
# Checks to see if any child process has reported its availability
# In blocking mode, blocks until a child has reported.
# ----------------------------------------------------------------
sub check_status {
    my($self, $block) = @_;

    return unless @{$self->{active_list}};

    my @pids;

    while (1) {

        # if can_read or sysread is interrupted while bloking, go back and 
        # wait again until we have at least 1 free child

        # refresh the read_set handles in case we lost a child in the previous iteration
        my $read_set = IO::Select->new;

        # Copy the array so there's no chance we try to reference a
        # free'd array ref in the loop below as a result of child
        # process maintenance.
        my @active = @{$self->{active_list}};

        $read_set->add($_->{pipe_to_child}) for
            grep {$_ && $_->{pipe_to_child}} @active;

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

    $chatty and $logger->internal(sub{return "server: ".scalar(@pids)." children reporting for duty: (@pids)" });

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

    $chatty and $logger->internal(sub{return sprintf(
        "server: %d idle and %d active children after status update",
            scalar(@{$self->{idle_list}}), scalar(@{$self->{active_list}})) });

    # some children just went from active to idle. let's see 
    # if any of them need to be killed from a previous sighup.

    for my $child (@{$self->{sighup_pending}}) {
        if (grep {$_ == $child->{pid}} @pids) {

            $chatty and $logger->internal(
                "server: killing previously-active ".
                "child after receiving SIGHUP: $child");

            # remove the pending child
            $self->{sighup_pending} = [
                grep {$_->{pid} != $child->{pid}} 
                    @{$self->{sighup_pending}}
            ];

            # kill the pending child
            $self->kill_child($child)
        }
    }
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

        # since we may be in the middle of check_status(),
        # stash the remnants of the child for later cleanup
        # after check_status() has finished; otherwise, we may crash
        # the parent with a "Use of freed value in iteration" error
        push @{ $self->{zombie_list} }, $child;
    }

    $self->spawn_children unless $shutdown;

    $chatty and $logger->internal(sub{return sprintf(
        "server: %d idle and %d active children after reap_children",
            scalar(@{$self->{idle_list}}), scalar(@{$self->{active_list}})) });
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

        $chatty and $logger->internal(sub{return "server: server spawned child $child with ".$self->{num_children}." total children" });

        return $child;

    } else { # child process

        # recover default handling for any signal whose handler 
        # may have been adopted from the parent process.
        $SIG{$_} = 'DEFAULT' for qw/TERM INT QUIT HUP CHLD USR1 USR2/;

        if($self->{stderr_log}) {

            $chatty and $logger->internal(sub{return "server: redirecting STDERR to " . $self->{stderr_log} });

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
    my $router_name = $conf->bootstrap->router_name || 'router';
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
                push(@targets, "opensrf:router:$router_name:$domain");
            }

        } else {
            # $router here == $domain
            push(@targets, "opensrf:router:$router_name:$router");
        }
    }

    foreach (@targets) {
        $logger->info("server: registering with router $_");
        $self->{osrf_handle}->send(
            to => $_,
            body => '"[]"',
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
            body => '"[]"',
            router_command => "unregister",
            router_class => $self->{service}
        );

        $logger->info("Unregister sent to $router");
    }
}


package OpenSRF::Server::Child;
use strict;
use warnings;
use OpenSRF::Transport;
use OpenSRF::Application;
use OpenSRF::Transport::PeerHandle;
use OpenSRF::Transport::Redis::Message;
use OpenSRF::Utils::Logger qw($logger);
use OpenSRF::DomainObject::oilsResponse qw/:status/;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Time::HiRes qw(time usleep);
use POSIX qw/:sys_wait_h :errno_h/;

use overload '""' => sub { return '[' . shift()->{pid} . ']'; };

sub new {
    my($class, $parent) = @_;
    my $self = bless({}, $class);
    $self->{pid} = 0; # my process ID
    $self->{parent} = $parent; # Controller parent process
    $self->{num_requests} = 0; # total serviced requests
    $self->{sig_pipe} = 0;  # true if last syswrite failed
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
# Connects to the bus and runs the application child_init
# ----------------------------------------------------------------
sub init {
    my $self = shift;
    my $service = $self->{parent}->{service};
    $0 = "OpenSRF Drone [$service]";
    OpenSRF::Transport::PeerHandle->construct($service, 1);
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

        # Discard extraneous data from the bus
        if(!$network->flush_socket()) {
            $logger->error("server: network disconnected!  child dropping request and exiting: $data");
            exit;
        }

        my $session = OpenSRF::Transport->handler(
            $self->{parent}->{service},
            OpenSRF::Transport::Redis::Message->new(json => $data)
        );

        my $recycle = $self->keepalive_loop($session);

        last if ++$self->{num_requests} == $self->{parent}->{max_requests};

        if ($recycle) {
            $chatty && $logger->internal(
                "server: child exiting early on force_recycle");
            last;
        }

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

    my $data = ''; # final request data
    my $buf_size = 4096; # default linux pipe_buf (atomic window, not total size)
    my $read_pipe = $self->{pipe_to_parent};
    my $bytes_needed; # size of the data we are about to receive
    my $bytes_recvd; # number of bytes read so far
    my $first_read = 1; # true for first loop iteration
    my $read_error;

    while (1) {

        # wait for some data to start arriving
        my $read_set = IO::Select->new;
        $read_set->add($read_pipe);
    
        while (1) {
            # if can_read is interrupted while blocking, 
            # go back and wait again until it succeeds.
            last if $read_set->can_read;
        }

        # parent started writing, let's start reading
        $self->set_nonblock($read_pipe);

        while (1) {
            # read all of the available data

            my $buf = '';
            my $nbytes = sysread($self->{pipe_to_parent}, $buf, $buf_size);

            unless(defined $nbytes) {
                if ($! != EAGAIN) {
                    $logger->error("server: error reading data from parent: $!.  ".
                        "bytes_needed=$bytes_needed; bytes_recvd=$bytes_recvd; data=$data");
                    $read_error = 1;
                }
                last;
            }

            last if $nbytes <= 0; # no more data available for reading

            $bytes_recvd += $nbytes;
            $data .= $buf;
        }

        $self->set_block($self->{pipe_to_parent});
        return undef if $read_error;

        # extract the data size and remove the header from the final data
        if ($first_read) {
            my $wps_size = OpenSRF::Server::WRITE_PIPE_DATA_SIZE;
            $bytes_needed = int(substr($data, 0, $wps_size)) + $wps_size;
            $data = substr($data, $wps_size);
            $first_read = 0;
        }


        if ($bytes_recvd == $bytes_needed) {
            # we've read all the data. Nothing left to do
            last;
        }

        $logger->info("server: child process read all available pipe data.  ".
            "waiting for more data from parent.  bytes_needed=$bytes_needed; bytes_recvd=$bytes_recvd");
    }

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

    # Capture the recycle option value before it's clobbered.
    # The option may be set at any point along the life of the 
    # session.  Once set, it remains set unless 
    # $session->force_recycle(0) is explicitly called.
    my $recycle = $session->force_recycle;

    $session->kill_me;
    return $recycle;
}

# ----------------------------------------------------------------
# Report our availability to our parent process
# ----------------------------------------------------------------
sub send_status {
    my $self = shift;

    for (0..2) {

        $self->{sig_pipe} = 0;
        local $SIG{'PIPE'} = sub { $self->{sig_pipe} = 1; };

        syswrite(
            $self->{pipe_to_parent},
            sprintf("%*s", OpenSRF::Server::STATUS_PIPE_DATA_SIZE, $self->{pid})
        );

        last unless $self->{sig_pipe};
        $logger->error("server: $self got SIGPIPE writing status to parent, retrying...");
        usleep(50000); # 50 msec
    }

    $logger->error("server: $self unable to send status to parent") if $self->{sig_pipe};
}


1;

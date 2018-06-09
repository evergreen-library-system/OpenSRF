#!/usr/bin/perl
# --------------------------------------------------------------------------
# Copyright (C) 2018 King County Library Service
# Bill Erickson <berickxx@gmail.com>
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
# --------------------------------------------------------------------------
#
# Synopsis:
#
# $ sudo cpan Net::Async::WebSocket::Client; 
# $ sudo cpan IO::Async::SSL;
# $ time perl test-stateful.pl wss://localhost:443/osrf-websocket-translator
#
# --------------------------------------------------------------------------
use strict;
use warnings;
use IO::Async::Loop;
use Net::Async::WebSocket::Client;
use OpenSRF::Utils::JSON;

my $fork_count = 5;
my $batch_size = 1000;

# allow the script to run easily on test VMs.
use IO::Socket::SSL;
IO::Socket::SSL::set_ctx_defaults(SSL_verify_mode => 0);

package StatefulBatch;

sub new {
    my $class = shift;
    my $self = {
        client => undef,
        loop => undef,
        thread => undef,
        sent_count => 0,
        in_connect => 0
    };

    return bless($self, $class);
}

sub send_connect {
    my $self = shift;
    $self->{in_connect} = 1;

    my $thread = $self->{thread} = rand(); # reset on connect
    my $msg = <<MSG;
    {"service":"open-ils.auth","thread":"$thread","osrf_msg":[{"__c":"osrfMessage","__p":{"threadTrace":"0","locale":"en-US","tz":"America/New_York","type":"CONNECT"}}]}
MSG

    $self->{client}->send_text_frame($msg);
}

sub send_request {
    my $self = shift;
    $self->{in_connect} = 0;

    my $thread = $self->{thread};
    my $msg = <<MSG;
{"service":"open-ils.auth","thread":"$thread","osrf_msg":[{"__c":"osrfMessage","__p":{"threadTrace":1,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"opensrf.system.echo","params":["EC asldi asldif asldfia sdflias dflasdif alsdif asldfias dlfiasd flasidf alsdif alsdif asldfia sldfias dlfias dflaisd flasidf lasidf alsdif asldif asldif asldif asldif asldif asldfia sldfia sdlfias dlfias dfliasd flasidf lasidf alsdif asldif alsdif asldif asldif aslidf alsdif alsidf alsdif asldif asldif asldif asldif asldif asldif alsdif alsdif alsidf alsidf alsdif asldif asldif asldfi asldfi asldif asldif asldfi asldfias ldfaisdf lasidf alsdif asldif asldfi asdlfias dHO ME"]}},"locale":"en-US","tz":"America/New_York","api_level":1}}]}
MSG

    $self->{client}->send_text_frame($msg);
}

sub send_disconnect {
    my $self = shift;

    my $thread = $self->{thread};
    my $msg = <<MSG;
    {"service":"open-ils.auth","thread":"$thread","osrf_msg":[{"__c":"osrfMessage","__p":{"threadTrace":"2","locale":"en-US","tz":"America/New_York","type":"DISCONNECT"}}]}
MSG
    $self->{client}->send_text_frame($msg);
}


sub on_message {
    my ($self, $frame) = @_;

    my $msg = OpenSRF::Utils::JSON->JSON2perl($frame);
    my $type = $msg->{osrf_msg}->[0]->{type};

    if ($self->{in_connect}) {
        my $msg = OpenSRF::Utils::JSON->JSON2perl($frame);
        if ($type ne 'STATUS') {
            die "Received unexpected message type: $type : $frame\n";
        }
        $self->send_request;

    } else {

        if ($type ne 'RESULT') {
            die "Received unexpected message type: $type : $frame\n";
        }

        # disconnect messages do not return replies
        $self->send_disconnect;

        print "[$$] completed ".$self->{sent_count} . " of $batch_size\n";

        if ($self->{sent_count}++ >= $batch_size) {
            $self->{loop}->stop;
            return;
        }

        $self->send_connect;
    }
}

package main;

my $url = $ARGV[0] or die "WS URL REQUIRED\n";


for (1..$fork_count) {

    if (fork() == 0) {
        my $tester = StatefulBatch->new;

        $tester->{client} = Net::Async::WebSocket::Client->new(
            on_text_frame => sub {
                my ($client, $frame) = @_;
                $tester->on_message($frame);
            }
        );

        $tester->{loop} = IO::Async::Loop->new;
        $tester->{loop}->add($tester->{client});
        $tester->{client}->connect(
            url => $url, on_connected => sub {$tester->send_connect});
        $tester->{loop}->run;
        exit(0);
    }
}

# exit after all children have exited
while (wait() > -1) {}


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
# $ time perl tester.pl wss://localhost:443/osrf-websocket-translator
#
# --------------------------------------------------------------------------
use strict;
use warnings;
use IO::Async::Loop;
use Net::Async::WebSocket::Client;

# allow the script to run easily on test VMs.
use IO::Socket::SSL;
IO::Socket::SSL::set_ctx_defaults(SSL_verify_mode => 0);

my $client;
my $loop;
my $send_batches = 1000;
my $batches_sent = 0;
my $send_wanted = 5; # per batch
my $send_count = 0;
my $recv_count = 0;

sub send_one_msg {
	my $thread = rand();
    $send_count++;

	my $osrf_msg = <<MSG;
{"service":"open-ils.auth","thread":"$thread","osrf_msg":[{"__c":"osrfMessage","__p":{"threadTrace":0,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"opensrf.system.echo","params":["EC asldi asldif asldfia sdflias dflasdif alsdif asldfias dlfiasd flasidf alsdif alsdif asldfia sldfias dlfias dflaisd flasidf lasidf alsdif asldif asldif asldif asldif asldif asldfia sldfia sdlfias dlfias dfliasd flasidf lasidf alsdif asldif alsdif asldif asldif aslidf alsdif alsidf alsdif asldif asldif asldif asldif asldif asldif alsdif alsdif alsidf alsidf alsdif asldif asldif asldfi asldfi asldif asldif asldfi asldfias ldfaisdf lasidf alsdif asldif asldfi asdlfias dHO ME"]}},"locale":"en-US","tz":"America/New_York","api_level":1}}]}
MSG

	$client->send_text_frame($osrf_msg);
    print "batch=$batches_sent sent=$send_count received=$recv_count\n";
}

my $on_message = sub {
    my ($self, $frame) = @_;
    $recv_count++;
    print "batch=$batches_sent sent=$send_count received=$recv_count\n";

    if ($send_count == $send_wanted && $send_count == $recv_count) {
        # once every request in the current batch has received
        # a reply, kick off a new batch.
        send_next_batch();
    }
};

sub send_next_batch {

    if ($batches_sent == $send_batches) {
        $loop->stop;
        return;
    }

    $batches_sent++;
    $send_count = 0;
    $recv_count = 0;
    for (1..$send_wanted) {
        send_one_msg();
    }
}

my $url = $ARGV[0] or die "WS URL REQUIRED\n";

$client = Net::Async::WebSocket::Client->new(on_text_frame => $on_message);
$loop = IO::Async::Loop->new;
$loop->add($client);
$client->connect(url => $url, on_connected => sub {send_next_batch()});
$loop->run;


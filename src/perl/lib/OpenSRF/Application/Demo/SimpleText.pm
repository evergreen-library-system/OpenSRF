#!/usr/bin/perl

# Copyright (C) 2009 Dan Scott <dscott@laurentian.ca>

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

=head1 NAME

OpenSRF::Application::Demo::SimpleText - simple text methods for demonstrating OpenSRF

=head1 SYNOPSIS

Via srfsh:
  request opensrf.simple-text opensrf.simple-text.reverse "foo"
returns "oof"

  request opensrf.simple-text opensrf.simple-text.substring "foobar", 3
returns "bar"

  request opensrf.simple-text opensrf.simple-text.split "This is a test", " "
returns ["This", "is", "a", "test"]

  request opensrf.simple-text opensrf.simple-text.statistics ["foo bar ala", "the cats"]
returns:

Received Data: {
  "length":19,
  "word_count":5
}

Via Perl:
  my $session = OpenSRF::AppSession->create("opensrf.simple-text");
  my $request = $session->request("opensrf.simple-text.reverse", [ "foo" ] )->gather();
  $request = $session->request("opensrf.simple-text.substring", [ "foobar", 3 ] )->gather();
  $request = $session->request("opensrf.simple-text.split", [ "This is a test", " " ] )->gather();
  $session->disconnect();

  # $request is a reference to the returned values

=head1 AUTHOR

Dan Scott, dscott@laurentian.ca

=cut

package OpenSRF::Application::Demo::SimpleText;

use strict;
use warnings;

# All OpenSRF applications must be based on OpenSRF::Application or
# a subclass thereof.  Makes sense, eh?
use OpenSRF::Application;
use base qw/OpenSRF::Application/;

# This is the client class, used for connecting to open-ils.storage
use OpenSRF::AppSession;

# This is an extension of Error.pm that supplies some error types to throw
use OpenSRF::EX qw(:try);

# This is a helper class for querying the OpenSRF Settings application ...
use OpenSRF::Utils::SettingsClient;

# ... and here we have the built in logging helper ...
use OpenSRF::Utils::Logger qw($logger);

# ... and this manages cached results for us ...
use OpenSRF::Utils::Cache;

my $prefix = "opensrf.simple-text"; # Prefix for caching values
my $cache;
my $cache_timeout;

# initialize() is invoked once, when the OpenSRF service is first started
# We don't need caching for these methods, but it's useful copy/paste
# code for more advanced services
sub initialize {
    $cache = OpenSRF::Utils::Cache->new('global');
    my $sclient = OpenSRF::Utils::SettingsClient->new();
    $cache_timeout = $sclient->config_value(
        "apps", "opensrf.simple-text", "app_settings", "cache_timeout" ) || 300;
}

# child_init() is invoked every time a new child process is created
# We don't need any per-child initialization, so this is empty
sub child_init { }

# accept and return a simple string
sub text_reverse {
    my $self = shift;
    my $conn = shift;
    my $text = shift;

    my $reversed_text = scalar reverse($text);
    return $reversed_text;
    
    return undef;
}

__PACKAGE__->register_method(
    method    => 'text_reverse',
    api_name  => 'opensrf.simple-text.reverse',
    api_level => 1,
    argc      => 1,
    signature => {
        desc     => <<"         DESC",
Returns the input string in reverse order
         DESC
        'params' => [ {
                name => 'text',
                desc => 'The string to reverse',
                type => 'string' 
            },
        ],
        'return' => {
            desc => 'Returns the input string in reverse order',
            type => 'string'
        }
    }
);

# accept string, return an array (note: return by reference)
sub text_split {
    my $self = shift;
    my $conn = shift;
    my $text = shift;
    my $delimiter = shift || ' ';

    my @split_text = split $delimiter, $text;
    foreach my $string (@split_text) {
        $conn->respond( $string );
    }
    
    return undef;
}

__PACKAGE__->register_method(
    method    => 'text_split',
    api_name  => 'opensrf.simple-text.split',
    api_level => 1,
    argc      => 2,
    stream    => 1,
    signature => {
        desc     => <<"         DESC",
Splits a string by a given delimiter (space by default) and returns an array of the split strings
         DESC
        'params' => [ {
                name => 'text',
                desc => 'The string to split',
                type => 'string' 
            }, {
                name => 'delimiter',
                desc => 'The delimiter to split the string with',
                type => 'string' 
            },
        ],
        'return' => {
            desc => 'Splits a string by a given delimiter (space by default) and returns an array of the split strings',
            type => 'array'
        }
    }
);

# accept string and optional arguments, return a string
sub text_substring {
    my $self = shift;
    my $conn = shift;
    my $text = shift || '';
    my $start_pos = shift || 0;
    my $end_pos = shift;

    my $subtext;
    if ($end_pos) {
	$subtext = substr($text, $start_pos, $end_pos);
    } else {
	$subtext = substr($text, $start_pos);
    }
    return $subtext;
}

__PACKAGE__->register_method(
    method    => 'text_substring',
    api_name  => 'opensrf.simple-text.substring',
    api_level => 1,
    argc      => 1,
    signature => {
        desc     => <<"         DESC",
Returns a substring of the input string
         DESC
        'params' => [ {
                name => 'text',
                desc => 'The string to process',
                type => 'string' 
            }, {
                name => 'start_pos',
                desc => 'The start position for the substring (default 0)',
                type => 'int' 
            }, {
                name => 'end_pos',
                desc => 'The end position for the substring (optional)',
                type => 'int' 
            },
        ],
        'return' => {
            desc => 'Returns a substring of the input string',
            type => 'string'
        }
    }
);

# accept an array, return a hash (note: return by reference)
sub text_statistics {
    my $self = shift;
    my $conn = shift;
    my $aref = shift || '';

    my %stats;

    my $length = 0;
    my $word_count = 0;

    foreach my $entry (@$aref) {
        $length += length($entry);
        $word_count += scalar (split /\s/, $entry);
    }
    $stats{'length'} = $length;
    $stats{'word_count'} = $word_count;

    return \%stats;
}

__PACKAGE__->register_method(
    method    => 'text_statistics',
    api_name  => 'opensrf.simple-text.statistics',
    api_level => 1,
    argc      => 1,
    signature => {
        desc     => <<"         DESC",
Returns the statistics for an array of strings
         DESC
        'params' => [ {
                name => 'text',
                desc => 'The array of strings to process',
                type => 'array' 
            }
        ],
        'return' => {
            desc => 'Returns the statistics for an array of strings',
            type => 'hash'
        }
    }
);

1;

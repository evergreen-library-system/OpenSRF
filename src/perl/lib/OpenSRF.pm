package OpenSRF;
use warnings;
use strict;
use Error;
require UNIVERSAL::require;

# $Revision$

=head1 NAME

OpenSRF - Top level class for OpenSRF perl modules.

=head1 VERSION

Version 2.4.1

=cut

our $VERSION = "2.42";

=head1 METHODS

=head2 alert_abstract

This method is called by abstract methods to ensure that the process
dies when an undefined abstract method is called.

=cut

sub alert_abstract() {
	my $c = shift;
	my $class = ref( $c ) || $c;
	my ($file, $line, $method) = (caller(1))[1..3];
	die " * Call to abstract method $method at $file, line $line";
}

=head2 class

Returns the scalar value of its caller.

=cut

sub class { return scalar(caller); }

1;

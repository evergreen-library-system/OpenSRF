package OpenSRF;
use warnings;
use strict;
use Error;
require UNIVERSAL::require;

# $Revision$

=head1 NAME

OpenSRF - Top level class for OpenSRF perl modules.

=head1 VERSION

Version 2.5.0-beta

=cut

our $VERSION = "2.50_3";

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

=head2 OSRF_APACHE_REQUEST_OBJ

Gets and sets the Apache request object when running inside mod_perl.
This allows other parts of OpenSRF to investigate the state of the
remote connection, such as whether the client has disconnected, and
react accordingly.

=cut

our $_OARO;
sub OSRF_APACHE_REQUEST_OBJ {
	my $self = shift;
	my $a = shift;
	$_OARO = $a if $a;
	return $_OARO;
}

1;

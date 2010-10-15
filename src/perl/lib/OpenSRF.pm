package OpenSRF;

use strict;
use vars qw/$AUTOLOAD/;

use Error;
require UNIVERSAL::require;

# $Revision$

=head1 NAME

OpenSRF - Top level class for OpenSRF perl modules.

=head1 VERSION

Version 1.6.1

=cut

our $VERSION = "1.6.1";

=head1 METHODS

=head2 AUTOLOAD

Traps methods calls for methods that have not been defined so they
don't propogate up the class hierarchy.

=cut

sub AUTOLOAD {
	my $self = shift;
	my $type = ref($self) || $self;
	my $name = $AUTOLOAD;
	my $otype = ref $self;

	my ($package, $filename, $line) = caller;
	my ($package1, $filename1, $line1) = caller(1);
	my ($package2, $filename2, $line2) = caller(2);
	my ($package3, $filename3, $line3) = caller(3);
	my ($package4, $filename4, $line4) = caller(4);
	my ($package5, $filename5, $line5) = caller(5);
	$name =~ s/.*://;   # strip fully-qualified portion
	warn <<"	WARN";
****
** ${name}() isn't there.  Please create me somewhere (like in $type)!
** Error at $package ($filename), line $line
** Call Stack (5 deep):
** 	$package1 ($filename1), line $line1
** 	$package2 ($filename2), line $line2
** 	$package3 ($filename3), line $line3
** 	$package4 ($filename4), line $line4
** 	$package5 ($filename5), line $line5
** Object type was $otype
****
	WARN
}



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

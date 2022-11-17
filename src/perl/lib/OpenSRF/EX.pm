package OpenSRF::EX;
use Error qw(:try);
use base qw( OpenSRF Error );
use OpenSRF::Utils::Logger;

my $log = "OpenSRF::Utils::Logger";
$Error::Debug = 1;

sub new {
	my( $class, $message ) = @_;
	$class = ref( $class ) || $class;
	my $self = {};
	$self->{'msg'} = ${$class . '::ex_msg_header'} .": $message";
	return bless( $self, $class );
}	

sub message() { return $_[0]->{'msg'}; }

sub DESTROY{}


=head1 OpenSRF::EX

Top level exception.  This class logs an exception when it is thrown.  Exception subclasses
should subclass one of OpenSRF::EX::INFO, NOTICE, WARN, ERROR, CRITICAL, and PANIC and provide
a new() method that takes a message and a message() method that returns that message.

=cut

=head2 Synopsis


	throw OpenSRF::EX::Transport ("I Am Dying");

	OpenSRF::EX::InvalidArg->throw( "Another way" );

	my $je = OpenSRF::EX::Transport->new( "I Cannot Connect" );
	$je->throw();


	See OpenSRF/EX.pm for example subclasses.

=cut

# Log myself and throw myself

#sub message() { shift->alert_abstract(); }

#sub new() { shift->alert_abstract(); }

sub throw() {

	my $self = shift;

	if( ! ref( $self ) || scalar( @_ ) ) {
		$self = $self->new( @_ );
	}

	if(		$self->class->isa( "OpenSRF::EX::INFO" )	||
				$self->class->isa( "OpenSRF::EX::NOTICE" ) ||
				$self->class->isa( "OpenSRF::EX::WARN" ) ) {

		$log->debug(sub{return $self->stringify() }, $log->DEBUG );
	}

	else{ $log->debug(sub{return $self->stringify() }, $log->ERROR ); }
	
	$self->SUPER::throw;
}


sub stringify() {
	my $self = shift;
	my($package, $file, $line) = get_caller();
	my $name = ref($self);
	my $msg = $self->message();

    my ($sec,$min,$hour,$mday,$mon,$year) = localtime();
    $year += 1900; $mon += 1;
    my $date = sprintf(
        '%s-%0.2d-%0.2dT%0.2d:%0.2d:%0.2d',
        $year, $mon, $mday, $hour, $min, $sec);

    return "Exception: $name $date $package $file:$line $msg\n";
}


# --- determine the originating caller of this exception
sub get_caller() {

	my $package = caller();
	my $x = 0;
	while( $package->isa( "Error" ) || $package =~ /^Error::/ ) { 
		$package = caller( ++$x );
	}
	return (caller($x));
}




# -------------------------------------------------------------------
# -------------------------------------------------------------------

# Top level exception subclasses defining the different exception
# levels.

# -------------------------------------------------------------------

package OpenSRF::EX::INFO;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System INFO";

# -------------------------------------------------------------------

package OpenSRF::EX::NOTICE;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System NOTICE";

# -------------------------------------------------------------------

package OpenSRF::EX::WARN;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System WARNING";

# -------------------------------------------------------------------

package OpenSRF::EX::ERROR;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System ERROR";

# -------------------------------------------------------------------

package OpenSRF::EX::CRITICAL;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System CRITICAL";

# -------------------------------------------------------------------

package OpenSRF::EX::PANIC;
use base qw(OpenSRF::EX);
our $ex_msg_header = "System PANIC";

# -------------------------------------------------------------------
# -------------------------------------------------------------------

# Some basic exceptions

# -------------------------------------------------------------------

package OpenSRF::EX::Transport;
use base 'OpenSRF::EX::ERROR';
our $ex_msg_header = "Transport Exception";



# -------------------------------------------------------------------
package OpenSRF::EX::InvalidArg;
use base 'OpenSRF::EX::ERROR';
our $ex_msg_header = "Invalid Arg Exception";

=head2 OpenSRF::EX::InvalidArg

Thrown where an argument to a method was invalid or not provided

=cut


# -------------------------------------------------------------------
package OpenSRF::EX::Socket;
use base 'OpenSRF::EX::ERROR';
our $ex_msg_header = "Socket Exception";

=head2 OpenSRF::EX::Socket

Thrown when there is a network layer exception

=cut



# -------------------------------------------------------------------
package OpenSRF::EX::Config;
use base 'OpenSRF::EX::PANIC';
our $ex_msg_header = "Config Exception";

=head2 OpenSRF::EX::Config

Thrown when a package requires a config option that it cannot retrieve
or the config file itself cannot be loaded

=cut


# -------------------------------------------------------------------
package OpenSRF::EX::User;
use base 'OpenSRF::EX::ERROR';
our $ex_msg_header = "User Exception";

=head2 OpenSRF::EX::User

Thrown when an error occurs due to user identification information

=cut

package OpenSRF::EX::Session;
use base 'OpenSRF::EX::ERROR';
our $ex_msg_header = "Session Error";


1;

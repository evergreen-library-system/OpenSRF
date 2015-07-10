package OpenSRF::Utils::Logger;
use strict;
use vars qw($AUTOLOAD @EXPORT_OK %EXPORT_TAGS);
use Exporter;
use Unix::Syslog qw(:macros :subs);
use base qw/OpenSRF Exporter/;
use FileHandle;
use Time::HiRes qw(gettimeofday);
use OpenSRF::Utils::Config;
use Fcntl;

=head1

Logger code

my $logger = OpenSRF::Utils::Logger;
$logger->error( $msg );

For backwards compability, a log level may also be provided to each log
function thereby overriding the level defined by the function.

i.e. $logger->error( $msg, WARN );  # logs at log level WARN

=cut

@EXPORT_OK = qw/ NONE ERROR WARN INFO DEBUG INTERNAL /;
push @EXPORT_OK, '$logger';

%EXPORT_TAGS = ( level => [ qw/ NONE ERROR WARN INFO DEBUG INTERNAL / ], logger => [ '$logger' ] );

my $config;             # config handle
my $loglevel = INFO();  # global log level
my $logfile;            # log file
my $facility;           # syslog facility
my $actfac;             # activity log syslog facility
my $actfile;            # activity log file
my $service;            # syslog service name.  default provided below.
my $service_tag = '';   # default service tag
my $syslog_enabled = 0; # is syslog enabled?
my $act_syslog_enabled = 0; # is syslog enabled?
my $logfile_enabled = 1;    # are we logging to a file?
my $act_logfile_enabled = 1;# are we logging to a file?
my $max_log_msg_len = 1536; # SYSLOG default maximum is 2048

our $logger = "OpenSRF::Utils::Logger";

# log levels
sub ACTIVITY { return -1; }
sub NONE     { return 0; }
sub ERROR    { return 1; }
sub WARN     { return 2; }
sub INFO     { return 3; }
sub DEBUG    { return 4; }
sub INTERNAL { return 5; }
sub ALL      { return 100; }

my $isclient;  # true if we control the osrf_xid

# load up our config options
sub set_config {
    my $force = shift;

    return if defined $config and !$force;

    $config = OpenSRF::Utils::Config->current;
    if( !defined($config) ) {
        $loglevel = INFO();
        warn "*** Logger found no config.  Using STDERR ***\n";
        return;
    }

    $loglevel =  $config->bootstrap->loglevel; 

    if ($config->bootstrap->loglength) {
        $max_log_msg_len = $config->bootstrap->loglength;
    }

    $service_tag = $config->bootstrap->logtag;

    $logfile = $config->bootstrap->logfile;
    if($logfile =~ /^syslog/) {
        $syslog_enabled = 1;
        $logfile_enabled = 0;
        $logfile = $config->bootstrap->syslog;
        $facility = $logfile;
        $logfile = undef;
        $facility = _fac_to_const($facility);
        # OSRF_ADOPT_SYSLOG means we assume syslog is already
        # opened w/ the correct values.  Don't clobber it.
        openlog($service, 0, $facility) unless $ENV{OSRF_ADOPT_SYSLOG};

    } else { $logfile = "$logfile"; }


    if($syslog_enabled) {
        # --------------------------------------------------------------
        # if we're syslogging, see if we have a special syslog facility 
        # for activity logging.  If not, use the syslog facility for
        # standard logging
        # --------------------------------------------------------------
        $act_syslog_enabled = 1;
        $act_logfile_enabled = 0;
        $actfac = $config->bootstrap->actlog || $config->bootstrap->syslog;
        $actfac = _fac_to_const($actfac);
        $actfile = undef;
    } else {
        # --------------------------------------------------------------
        # we're not syslogging, use any specified activity log file.
        # Fall back to the standard log file otherwise
        # --------------------------------------------------------------
        $act_syslog_enabled = 0;
        $act_logfile_enabled = 1;
        $actfile = $config->bootstrap->actlog || $config->bootstrap->logfile;
    }

    my $client = OpenSRF::Utils::Config->current->bootstrap->client();

    if ($ENV{OSRF_LOG_CLIENT} or $ENV{MOD_PERL}) {
        $isclient = 1;
        return;
    }

    if (!$client) {
        $isclient = 0;
        return;
    }
    $isclient = ($client =~ /^true$/iog) ?  1 : 0;
}

sub _fac_to_const {
    my $name = shift;
    return LOG_LOCAL0 unless $name;
    return LOG_LOCAL0 if $name =~ /local0/i;
    return LOG_LOCAL1 if $name =~ /local1/i;
    return LOG_LOCAL2 if $name =~ /local2/i;
    return LOG_LOCAL3 if $name =~ /local3/i;
    return LOG_LOCAL4 if $name =~ /local4/i;
    return LOG_LOCAL5 if $name =~ /local5/i;
    return LOG_LOCAL6 if $name =~ /local6/i;
    return LOG_LOCAL7 if $name =~ /local7/i;
    return LOG_LOCAL0;
}

sub is_syslog {
    set_config();
    return $syslog_enabled;
}

sub is_act_syslog {
    set_config();
    return $act_syslog_enabled;
}

sub is_filelog {
    set_config();
    return $logfile_enabled;
}

sub is_act_filelog {
    set_config();
    return $act_logfile_enabled;
}

sub set_service {
    my( $self, $svc ) = @_;
    return if $ENV{OSRF_ADOPT_SYSLOG};
    $service = $svc;    
    $service .= '/' . $service_tag if (defined $service_tag);    
    if( is_syslog() ) {
        closelog();
        openlog($service, 0, $facility);
    }
}

sub error {
    my( $self, $msg, $level ) = @_;
    $level = ERROR() unless defined ($level);
    _log_message( $msg, $level );
}

sub warn {
    my( $self, $msg, $level ) = @_;
    $level = WARN() unless defined ($level);
    _log_message( $msg, $level );
}

sub info {
    my( $self, $msg, $level ) = @_;
    $level = INFO() unless defined ($level);
    _log_message( $msg, $level );
}

sub debug {
    my( $self, $msg, $level ) = @_;
    $level = DEBUG() unless defined ($level);
    _log_message( $msg, $level );
}

sub internal {
    my( $self, $msg, $level ) = @_;
    $level = INTERNAL() unless defined ($level);
    _log_message( $msg, $level );
}

sub activity {
    my( $self, $msg ) = @_;
    _log_message( $msg, ACTIVITY() );
}

# for backward compability
sub transport {
    my( $self, $msg, $level ) = @_;
    $level = DEBUG() unless defined ($level);
    _log_message( $msg, $level );
}


# ----------------------------------------------------------------------
# creates a new xid if necessary
# ----------------------------------------------------------------------
my $osrf_xid = '';
my $osrf_xid_inc = 0;
sub mk_osrf_xid {
   return unless $isclient;
   $osrf_xid_inc++;
   return $osrf_xid = "$^T${$}$osrf_xid_inc";
}

sub set_osrf_xid { 
   return if $isclient; # if we're a client, we control our xid
   $osrf_xid = $_[1]; 
}

sub get_osrf_xid { return $osrf_xid; }
# ----------------------------------------------------------------------

   
sub _log_message {
    my( $msg, $level ) = @_;
    return if $level > $loglevel;

    # apply a sane default service name/tag
    $logger->set_service($0) unless $service;

    my $l; my $n; 
    my $fac = $facility;

    if ($level == ERROR())            {$l = LOG_ERR; $n = "ERR "; }
    elsif ($level == WARN())        {$l = LOG_WARNING; $n = "WARN"; }
    elsif ($level == INFO())        {$l = LOG_INFO; $n = "INFO"; }    
    elsif ($level == DEBUG())        {$l = LOG_DEBUG; $n = "DEBG"; }
    elsif ($level == INTERNAL())    {$l = LOG_DEBUG; $n = "INTL"; }
    elsif ($level == ACTIVITY())    {$l = LOG_INFO; $n = "ACT"; $fac = $actfac; }

    my( undef, $file, $line_no ) = caller(1);
   $file =~ s#/.*/##og;

    # help syslog with the formatting
    $msg =~ s/\%/\%\%/gso if( is_act_syslog() or is_syslog() );

    $msg = "[$n:"."$$".":$file:$line_no:$osrf_xid] $msg";

    # Trim the message to the configured maximum log message length
    $msg = substr($msg, 0, $max_log_msg_len); 

    # avoid clobbering the adopted syslog facility
    my $slog_flags = $ENV{OSRF_ADOPT_SYSLOG} ? $l : $fac | $l;

    if( $level == ACTIVITY() ) {
        if( is_act_syslog() ) { syslog( $slog_flags, $msg ); }
        elsif( is_act_filelog() ) { _write_file( $msg, 1 ); }

    } else {
        if( is_syslog() ) { syslog( $slog_flags, $msg ); }
        elsif( is_filelog() ) { _write_file($msg); }
    }

    return $msg;
}

sub _write_file {
    my ($msg, $isact) = @_;
    my $file = $isact ? $actfile : $logfile;
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);  
    $year += 1900; $mon += 1;

    if ($file) {
        sysopen( SINK, $file, O_NONBLOCK|O_WRONLY|O_APPEND|O_CREAT ) 
            or die "Cannot sysopen $file: $!";
    } else {
        open (SINK, ">&2");  # print to STDERR as warned
    }
    binmode(SINK, ':utf8');
    printf SINK "[%04d-%02d-%02d %02d:%02d:%02d] %s %s\n", $year, $mon, $mday, $hour, $min, $sec, $service, $msg;
    close( SINK );
}

1;


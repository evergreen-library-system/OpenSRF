use strict; use warnings;
package OpenSRF::Utils::SettingsClient;
use OpenSRF::Utils::SettingsParser;
use OpenSRF::System;
use OpenSRF::AppSession;
use OpenSRF::Utils::Config;
use OpenSRF::EX qw(:try);

use vars qw/$host_config/;


sub new {return bless({},shift());}
my $session;
$host_config = undef;

sub has_config {
	if($host_config) { return 1; }
	return 0;
}


# ------------------------------------
# utility method for grabbing config info
sub config_value {
	my($self,@keys) = @_;

	$self->grab_host_config unless $host_config;

	my $hash = $host_config;

	# XXX TO DO, check local config 'version', 
	# call out to settings server when necessary....
	try {
		for my $key (@keys) {
			if(!ref($hash) eq 'HASH'){
				return undef;
			}
			$hash = $hash->{$key};
		}

	} catch Error with {
		my $e = shift;
		throw OpenSRF::EX::Config ("No Config information for @keys : $e : $@");
	};

	return $hash;

}


# XXX make smarter and more robust...
sub grab_host_config {
	my $self = shift;
	my $reload = shift;

	my $bsconfig = OpenSRF::Utils::Config->current;
	die "No bootstrap config exists.  Have you bootstrapped?\n" unless $bsconfig;
	my $host = $bsconfig->env->hostname;

	$session = OpenSRF::AppSession->create( "opensrf.settings" ) unless $session;

	my $resp;
	my $req;
	try {

		if( ! ($session->connect()) ) {die "Settings Connect timed out\n";}
		$req = $session->request( "opensrf.settings.host_config.get", $host, $reload);
		$resp = $req->recv( timeout => 10 );

	} catch OpenSRF::EX with {

		if( ! ($session->connect()) ) {die "Settings Connect timed out\n";}
		$req = $session->request( "opensrf.settings.default_config.get", $reload );
		$resp = $req->recv( timeout => 10 );

	} catch Error with {

		my $e = shift;
		warn "Connection to Settings Failed  $e : $@ ***\n";
		die $e;

	} otherwise {

		my $e = shift;
		warn "Settings Retrieval Failed  $e : $@ ***\n";
		die $e;
	};

	if(!$resp) {
		warn "No Response from settings server...going to sleep\n";
		sleep;
	}

	if( $resp && UNIVERSAL::isa( $resp, "OpenSRF::EX" ) ) {
		throw $resp;
	}

	$host_config = $resp->content();

	if(!$host_config) {
		throw OpenSRF::EX::Config ("Unable to retrieve host config for $host" );
	}

	$req->finish();
	$session->disconnect();
	$session->finish;
	$session->kill_me();
}



1;

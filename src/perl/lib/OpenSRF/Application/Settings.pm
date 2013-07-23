package OpenSRF::Application::Settings;
use OpenSRF::Application;
use OpenSRF::Utils::SettingsParser;
use OpenSRF::Utils::Logger qw/$logger/;
use base 'OpenSRF::Application';

sub child_exit {
    $logger->debug("settings server child exiting...$$");
}


__PACKAGE__->register_method( method => 'get_host_config', api_name => 'opensrf.settings.host_config.get' );
sub get_host_config {
	my( $self, $client, $host, $reload ) = @_;
	my $parser = OpenSRF::Utils::SettingsParser->new();
	$parser->reload if $reload;
	return $parser->get_server_config($host);
}

__PACKAGE__->register_method( method => 'get_default_config', api_name => 'opensrf.settings.default_config.get' );
sub get_default_config {
	my( $self, $client, $reload ) = @_;
	my $parser = OpenSRF::Utils::SettingsParser->new();
	$parser->reload if $reload;
	return $parser->get_default_config();
}




__PACKAGE__->register_method( method => 'xpath_get', api_name => 'opensrf.settings.xpath.get' );

__PACKAGE__->register_method( 
		method	=> 'xpath_get', 
		api_name => 'opensrf.settings.xpath.get.raw' );

sub xpath_get {
	my($self, $client, $xpath, $reload) = @_;
	my $parser = OpenSRF::Utils::SettingsParser->new();
	$parser->reload if $reload;
	return $parser->_get_all( $xpath );
}


1;

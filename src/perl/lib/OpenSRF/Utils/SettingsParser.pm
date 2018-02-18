use strict; use warnings;
package OpenSRF::Utils::SettingsParser;
use OpenSRF::Utils::Config;
use OpenSRF::EX qw(:try);
use OpenSRF::Utils::Logger;
use XML::LibXML;

# logger is not yet exported when this mod is loaded
my $logger = 'OpenSRF::Utils::Logger';

sub DESTROY{}
my $doc;
my $settings_file; # /path/to/opensrf.xml

sub new { return bless({},shift()); }

# reload the configuration file
sub reload {
    my $self = shift;
    $logger->info("settings parser reloading '$settings_file'");
    $self->initialize;
}


# returns 0 if the config file could not be found or if there is a parse error
# returns 1 if successful
sub initialize {
	my ($self, $filename) = @_;

	$settings_file = $filename if $filename;
	return 0 unless $settings_file;

	my $parser = XML::LibXML->new();
	$parser->keep_blanks(0);

	my $err;
	try {
		$doc = $parser->parse_file( $settings_file );
	} catch Error with {
		$err = shift;
		$logger->error("Error parsing $settings_file : $err");
	};

	return $err ? 0 : 1;
}

sub _get { _get_overlay(@_) }

sub _get_overlay {
	my( $self, $xpath ) = @_;
	my @nodes = $doc->documentElement->findnodes( $xpath );
	
	my $base = XML2perl(shift(@nodes));
	my @overlays;
	for my $node (@nodes) {
		push @overlays, XML2perl($node);
	}

	for my $ol ( @overlays ) {
		$base = merge_perl($base, $ol);
	}
	
	return $base;
}

sub _get_all {
	my( $self, $xpath ) = @_;
	my @nodes = $doc->documentElement->findnodes( $xpath );
	
	my @overlays;
	for my $node (@nodes) {
		push @overlays, XML2perl($node);
	}

	return \@overlays;
}

sub merge_perl {
	my $base = shift;
	my $ol = shift;

	if (ref($ol)) {
		if (ref($ol) eq 'HASH') {
			for my $key (keys %$ol) {
				if (ref($$ol{$key}) and ref($$ol{$key}) eq ref($$base{$key})) {
					merge_perl($$base{$key}, $$ol{$key});
				} else {
					$$base{$key} = $$ol{$key};
				}
			}
		} else {
			for my $key (0 .. scalar(@$ol) - 1) {
				if (ref($$ol[$key]) and ref($$ol[$key]) eq ref($$base[$key])) {
					merge_perl($$base[$key], $$ol[$key]);
				} else {
					$$base[$key] = $$ol[$key];
				}
			}
		}
	} else {
		$base = $ol;
	}

	return $base;
}

sub _check_for_int {
	my $value = shift;
	return 0+$value if ($value =~ /^\d{1,10}$/o);
	return $value;
}

sub XML2perl {
	my $node = shift;
	my %output;

	return undef unless($node);

	for my $attr ( ($node->attributes()) ) {
		next unless($attr);
		$output{$attr->nodeName} = _check_for_int($attr->value);
	}

	my @kids = $node->childNodes;
	if (@kids == 1 && $kids[0]->nodeType == 3) {
			return _check_for_int($kids[0]->textContent);
	} else {
		for my $kid ( @kids ) {
			next if ($kid->nodeName =~ /^#?comment$/);
			if (exists $output{$kid->nodeName}) {
				if (ref $output{$kid->nodeName} ne 'ARRAY') {
					$output{$kid->nodeName} = [$output{$kid->nodeName}, XML2perl($kid)];
				} else {
					push @{$output{$kid->nodeName}}, XML2perl($kid);
				}
				next;
			}
			$output{$kid->nodeName} = XML2perl($kid);
		}
	}

	return \%output;
}


# returns the full config hash for a given server
sub get_server_config {
	my( $self, $server ) = @_;

    # Work around a Net::Domain bug that can result in fqdn like foo.example.com,bar.com
    my @servers = split /,/, $server;
    my $xpath = "/opensrf/default";
    foreach (@servers) {
        $xpath .= "|/opensrf/hosts/$_";
    }
	return $self->_get( $xpath );
}

sub get_default_config {
	my( $self, $server ) = @_;
	my $xpath = "/opensrf/default";
	return $self->_get( $xpath );
}

1;

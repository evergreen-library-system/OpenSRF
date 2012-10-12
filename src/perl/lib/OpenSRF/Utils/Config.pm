package OpenSRF::Utils::Config::Section;

no strict 'refs';

use vars qw/@ISA $AUTOLOAD/;
push @ISA, qw/OpenSRF::Utils/;

use OpenSRF::Utils (':common');
use Net::Domain qw/hostfqdn/;

our $VERSION = "1.000";

my %SECTIONCACHE;
my %SUBSECTION_FIXUP;

#use overload '""' => \&OpenSRF::Utils::Config::dump_ini;

sub SECTION {
	my $sec = shift;
	return $sec->__id(@_);
}

sub new {
	my $self = shift;
	my $class = ref($self) || $self;

	$self = bless {}, $class;

	$self->_sub_builder('__id');
	# Hard-code this to match old bootstrap.conf section name
	# This hardcoded value is later overridden if the config is loaded
	# with the 'base_path' option
	$self->__id('bootstrap');

	my $bootstrap = shift;

	foreach my $key (sort keys %$bootstrap) {
		$self->_sub_builder($key);
		$self->$key($bootstrap->{$key});
	}

	return $self;
}

package OpenSRF::Utils::Config;

use vars qw/@ISA $AUTOLOAD $VERSION $OpenSRF::Utils::ConfigCache/;
push @ISA, qw/OpenSRF::Utils/;

use FileHandle;
use XML::LibXML;
use OpenSRF::Utils (':common');  
use OpenSRF::Utils::Logger;
use Net::Domain qw/hostfqdn/;

#use overload '""' => \&OpenSRF::Utils::Config::dump_ini;

sub import {
	my $class = shift;
	my $config_file = shift;

	return unless $config_file;

	$class->load( config_file => $config_file);
}

sub dump_ini {
	no warnings;
        my $self = shift;
        my $string;
	my $included = 0;
	if ($self->isa('OpenSRF::Utils::Config')) {
		if (UNIVERSAL::isa(scalar(caller()), 'OpenSRF::Utils::Config' )) {
			$included = 1;
		} else {
			$string = "# Main File:  " . $self->FILE . "\n\n" . $string;
		}
	}
        for my $section ( ('__id', grep { $_ ne '__id' } sort keys %$self) ) {
		next if ($section eq 'env' && $self->isa('OpenSRF::Utils::Config'));
                if ($section eq '__id') {
			$string .= '['.$self->SECTION."]\n" if ($self->isa('OpenSRF::Utils::Config::Section'));
		} elsif (ref($self->$section)) {
                        if (ref($self->$section) =~ /ARRAY/o) {
                                $string .= "list:$section = ". join(', ', @{$self->$section}) . "\n";
			} elsif (UNIVERSAL::isa($self->$section,'OpenSRF::Utils::Config::Section')) {
				if ($self->isa('OpenSRF::Utils::Config::Section')) {
					$string .= "subsection:$section = " . $self->$section->SECTION . "\n";
					next;
				} else {
					next if ($self->$section->{__sub} && !$included);
					$string .= $self->$section . "\n";
				}
                        } elsif (UNIVERSAL::isa($self->$section,'OpenSRF::Utils::Config')) {
				$string .= $self->$section . "\n";
			}
		} else {
			next if $section eq '__sub';
                       	$string .= "$section = " . $self->$section . "\n";
		}
        }
	if ($included) {
		$string =~ s/^/## /gm;
		$string = "# Subfile:  " . $self->FILE . "\n#" . '-'x79 . "\n".'#include "'.$self->FILE."\"\n". $string;
	}

        return $string;
}

=head1 NAME
 
OpenSRF::Utils::Config
 

=head1 SYNOPSIS

  use OpenSRF::Utils::Config;

  my $config_obj = OpenSRF::Utils::Config->load( config_file   => '/config/file.cnf' );

  my $attrs_href = $config_obj->bootstrap();

  $config_obj->bootstrap->loglevel(0);

  open FH, '>'.$config_obj->FILE() . '.new';
  print FH $config_obj;
  close FH;

=head1 DESCRIPTION

This module is mainly used by other OpenSRF modules to load an OpenSRF
configuration file.  OpenSRF configuration files are XML files that
contain a C<< <config> >> root element and an C<< <opensrf> >> child
element (in XPath notation, C</config/opensrf/>). Each child element
is converted into a hash key=>value pair. Elements that contain other
XML elements are pushed into arrays and added as an array reference to
the hash. Scalar values have whitespace trimmed from the left and
right sides.

=head1 EXAMPLE

Given an OpenSRF configuration file named F<opensrf_core.xml> with the
following content:

  <?xml version='1.0'?>
  <config>
    <opensrf>
      <router_name>router</router_name>

      <routers> 
	<router>localhost</router>
	<router>otherhost</router>
      </routers>

      <logfile>/var/log/osrfsys.log</logfile>
    </opensrf>
  </config>

... calling C<< OpenSRF::Utils::Config->load(config_file =>
'opensrf_core.xml') >> will create a hash with the following
structure:

  {
    router_name => 'router',
    routers => ['localhost', 'otherhost'],
    logfile => '/var/log/osrfsys.log'
  }

You can retrieve any of these values by name from the bootstrap
section of C<$config_obj>; for example:

  $config_obj->bootstrap->router_name

=head1 NOTES

For compatibility with previous versions of the OpenSRF configuration
files, the C<load()> method by default loads the C</config/opensrf>
section with the hardcoded name of B<bootstrap>.

However, it is possible to load child elements of C<< <config> >> other
than C<< <opensrf> >> by supplying a C<base_path> argument which specifies
the node you wish to begin loading from (in XPath notation). Doing so
will also replace the hardcoded C<bootstrap> name with the node name of
the last member of the given path.  For example:

  my $config_obj = OpenSRF::Utils::Config->load(
      config_file => '/config/file.cnf'
      base_path => '/config/shared'
  );

  my $attrs_href = $config_obj->shared();

While it may be possible to load the entire file in this fashion (by
specifying an empty C<base_path>), doing so will break compatibility with
existing code which expects to find a C<bootstrap> member. Future
iterations of this module may extend its ability to parse the entire
OpenSRF configuration file in one pass while providing multiple base
sections named after the sibling elements of C</config/opensrf>.

Hashrefs of sections can be returned by calling a method of the object
of the same name as the section.  They can be set by passing a hashref
back to the same method.  Sections will B<NOT> be autovivicated,
though.


=head1 METHODS



=head2 OpenSRF::Utils::Config->load( config_file => '/some/config/file.cnf' )

Returns a OpenSRF::Utils::Config object representing the config file
that was loaded.  The most recently loaded config file (hopefully the
only one per app) is stored at $OpenSRF::Utils::ConfigCache. Use
OpenSRF::Utils::Config::current() to get at it.

=cut

sub load {
	my $pkg = shift;
	$pkg = ref($pkg) || $pkg;

	my %args = @_;

	(my $new_pkg = $args{config_file}) =~ s/\W+/_/g;
	$new_pkg .= "::$pkg";
	$new_section_pkg .= "${new_pkg}::Section";

	{	eval <<"		PERL";

			package $new_pkg;
			use base $pkg;
			sub section_pkg { return '$new_section_pkg'; }

			package $new_section_pkg;
			use base "${pkg}::Section";
	
		PERL
	}

	return $new_pkg->_load( %args );
}

sub _load {
	my $pkg = shift;
	$pkg = ref($pkg) || $pkg;
	my $self = {@_};
	bless $self, $pkg;

	no warnings;
	if ((exists $$self{config_file} and OpenSRF::Utils::Config->current) and (OpenSRF::Utils::Config->current->FILE eq $$self{config_file}) and (!$self->{force})) {
		delete $$self{force};
		return OpenSRF::Utils::Config->current();
	}

	$self->_sub_builder('__id');
	$self->FILE($$self{config_file});
	delete $$self{config_file};
	return undef unless ($self->FILE);

	my %load_args;
	if (exists $$self{base_path}) { # blank != non-existent for this setting
		$load_args{base_path} = $$self{base_path};
	}
	$self->load_config(%load_args);
	$self->load_env();
	$self->mangle_dirs();
	$self->mangle_logs();

	$OpenSRF::Utils::ConfigCache = $self unless $self->nocache;
	delete $$self{nocache};
	delete $$self{force};
	delete $$self{base_path};
	return $self;
}

sub sections {
	my $self = shift;
	my %filters = @_;

	my @parts = (grep { UNIVERSAL::isa($_,'OpenSRF::Utils::Config::Section') } values %$self);
	if (keys %filters) {
		my $must_match = scalar(keys %filters);
		my @ok_parts;
		foreach my $part (@parts) {
			my $part_count = 0;
			for my $fkey (keys %filters) {
				$part_count++ if ($part->$key eq $filters{$key});
			}
			push @ok_parts, $part if ($part_count == $must_match);
		}
		return @ok_parts;
	}
	return @parts;
}

sub current {
	return $OpenSRF::Utils::ConfigCache;
}

sub FILE {
	return shift()->__id(@_);
}

sub load_env {
	my $self = shift;
	my $host = $ENV{'OSRF_HOSTNAME'} || hostfqdn();
	chomp $host;
	$$self{env} = $self->section_pkg->new;
	$$self{env}{hostname} = $host;
}

sub mangle_logs {
	my $self = shift;
	return unless ($self->logs && $self->dirs && $self->dirs->log_dir);
	for my $i ( keys %{$self->logs} ) {
		next if ($self->logs->$i =~ /^\//);
		$self->logs->$i($self->dirs->log_dir."/".$self->logs->$i);
	}
}

sub mangle_dirs {
	my $self = shift;
	return unless ($self->dirs && $self->dirs->base_dir);
	for my $i ( keys %{$self->dirs} ) {
		if ( $i ne 'base_dir' ) {
			next if ($self->dirs->$i =~ /^\//);
			my $dir_tmp = $self->dirs->base_dir."/".$self->dirs->$i;
			$dir_tmp =~ s#//#/#go;
			$dir_tmp =~ s#/$##go;
			$self->dirs->$i($dir_tmp);
		}
	}
}

sub load_config {
	my $self = shift;
	my $parser = XML::LibXML->new();
	my %args = @_;

	# Hash of config values
	my %bootstrap;
	
	# Return an XML::LibXML::Document object
	my $config = $parser->parse_file($self->FILE);

	unless ($config) {
		OpenSRF::Utils::Logger->error("Could not open ".$self->FILE.": $!\n");
		die "Could not open ".$self->FILE.": $!\n";
	}

	# For backwards compatibility, we default to /config/opensrf
	my $base_path;
	if (exists $args{base_path}) { # allow for empty to import entire file
		$base_path = $args{base_path};
	} else {
		$base_path = '/config/opensrf';
	}
	# Return an XML::LibXML::NodeList object matching all child elements
	# of $base_path...
	my $osrf_cfg = $config->findnodes("$base_path/child::*");

	# Iterate through the nodes to pull out key=>value pairs of config settings
	foreach my $node ($osrf_cfg->get_nodelist()) {
		my $child_state = 0;

		# This will be overwritten if it's a scalar setting
		$bootstrap{$node->nodeName()} = [];

		foreach my $child_node ($node->childNodes) {
			# from libxml/tree.h: nodeType 1 = ELEMENT_NODE
			next if $child_node->nodeType() != 1;

			# If the child node is an element, this element may
			# have multiple values; therefore, push it into an array
            my $content = OpenSRF::Utils::Config::extract_child($child_node);
			push(@{$bootstrap{$node->nodeName()}}, $content) if $content;
			$child_state = 1;
		}
		if (!$child_state) {
			$bootstrap{$node->nodeName()} = OpenSRF::Utils::Config::extract_text($node->textContent);
		}
	}

	my $section = $self->section_pkg->new(\%bootstrap);
	# if the Config was loaded with a 'base_path' option, overwrite the
	# hardcoded 'bootstrap' name with something more reasonable
	if (exists $$self{base_path}) { # blank != non-existent for this setting
		# name root node to reflect last member of base_path, or default to root
		my $root = (split('/', $$self{base_path}))[-1] || 'root';
		$section->__id($root);
	}
	my $sub_name = $section->SECTION;
	$self->_sub_builder($sub_name);
	$self->$sub_name($section);

}
sub extract_child {
    my $node = shift;
    use OpenSRF::Utils::SettingsParser;
    return OpenSRF::Utils::SettingsParser::XML2perl($node);
}

sub extract_text {
	my $self = shift;
	$self =~ s/^\s*([.*?])\s*$//m;
	return $self;
}

#------------------------------------------------------------------------------------------------------------------------------------

=head1 SEE ALSO

	OpenSRF::Utils

=head1 LIMITATIONS

Elements containing heterogeneous child elements are treated as though they have the same element name;
for example:
  <routers>
    <router>localhost</router>
    <furniture>chair</furniture>
  </routers>

... will simply generate a key=>value pair of C<< routers => ['localhost', 'chair'] >>.

=head1 BUGS

No known bugs, but report any to open-ils-dev@list.georgialibraries.org or mrylander@gmail.com.

=head1 COPYRIGHT AND LICENSING

Copyright (C) 2000-2007, Mike Rylander
Copyright (C) 2007, Laurentian University, Dan Scott <dscott@laurentian.ca>

The OpenSRF::Utils::Config module is free software. You may distribute under the terms
of the GNU General Public License version 2 or greater.

=cut


1;

package OpenSRF::Utils::JSON;
use JSON::XS;
use vars qw/%_class_map/;

my $parser = JSON::XS->new;
$parser->ascii(1); # output \u escaped strings
$parser->allow_nonref(1);

sub true {
    return $parser->true();
}

sub false {
    return $parser->false();
}

sub register_class_hint {
	my $class = shift;
	my %args = @_;
	$_class_map{hints}{$args{hint}} = \%args;
	$_class_map{classes}{$args{name}} = \%args;
}

sub lookup_class {
	my $self = shift;
	my $hint = shift;
	return $_class_map{hints}{$hint}{name}
}

sub lookup_hint {
	my $self = shift;
	my $class = shift;
	return $_class_map{classes}{$class}{hint}
}

sub _json_hint_to_class {
	my $type = shift;
	my $hint = shift;

	return $_class_map{hints}{$hint}{name} if (exists $_class_map{hints}{$hint});
	
	$type = 'hash' if ($type eq '}');
	$type = 'array' if ($type eq ']');

	OpenSRF::Utils::JSON->register_class_hint(name => $hint, hint => $hint, type => $type);

	return $hint;
}


my $JSON_CLASS_KEY = '__c';
my $JSON_PAYLOAD_KEY = '__p';

sub JSON2perl {
	my( $class, $string ) = @_;
	my $perl = $class->rawJSON2perl($string);
	return $class->JSONObject2Perl($perl);
}

sub perl2JSON {
	my( $class, $obj ) = @_;
	my $json = $class->perl2JSONObject($obj);
	return $class->rawPerl2JSON($json);
}

sub JSONObject2Perl {
	my $class = shift;
	my $obj = shift;
	my $ref = ref($obj);
	if( $ref eq 'HASH' ) {
		if( defined($obj->{$JSON_CLASS_KEY})) {
			my $cls = $obj->{$JSON_CLASS_KEY};
            $cls =~ s/^\s+//o;
            $cls =~ s/\s+$//o;
			if( $obj = $class->JSONObject2Perl($obj->{$JSON_PAYLOAD_KEY}) ) {
				$cls = $class->lookup_class($cls) || $cls;
				return bless(\$obj, $cls) unless ref($obj); 
				return bless($obj, $cls);
			}
			return undef;
		}
        for my $k (keys %$obj) {
            $obj->{$k} = (ref($obj->{$k}) eq 'JSON::XS::Boolean') ? 
                $obj->{$k} : $class->JSONObject2Perl($obj->{$k});
        }
	} elsif( $ref eq 'ARRAY' ) {
		$obj->[$_] = $class->JSONObject2Perl($obj->[$_]) for(0..scalar(@$obj) - 1);
	}
	return $obj;
}

sub perl2JSONObject {
	my $class = shift;
	my $obj = shift;
	my $ref = ref($obj);

	return $obj unless $ref;

    return $obj if $ref eq 'JSON::XS::Boolean';
	my $newobj;

    if(UNIVERSAL::isa($obj, 'HASH')) {
        $newobj = {};
        $newobj->{$_} = $class->perl2JSONObject($obj->{$_}) for (keys %$obj);
    } elsif(UNIVERSAL::isa($obj, 'ARRAY')) {
        $newobj = [];
        $newobj->[$_] = $class->perl2JSONObject($obj->[$_]) for(0..scalar(@$obj) - 1);
    }

    if($ref ne 'HASH' and $ref ne 'ARRAY') {
		$ref = $class->lookup_hint($ref) || $ref;
		$newobj = {$JSON_CLASS_KEY => $ref, $JSON_PAYLOAD_KEY => $newobj};
    }

	return $newobj;	
}


sub rawJSON2perl {
	my $class = shift;
    my $json = shift;
    return undef unless defined $json and $json !~ /^\s*$/o;
    return $parser->decode($json);
}

sub rawPerl2JSON {
	my ($class, $perl) = @_;
    return $parser->encode($perl);
}

1;

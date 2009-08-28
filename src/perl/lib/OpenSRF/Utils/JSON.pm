package OpenSRF::Utils::JSON;
use JSON::XS;

my $parser = JSON::XS->new;
$parser->ascii(1);        # output \u escaped strings for any char with a value over 127
$parser->allow_nonref(1); # allows non-reference values to equate to themselves (see perldoc)

my %_class_map = ();
my $JSON_CLASS_KEY = '__c';
my $JSON_PAYLOAD_KEY = '__p';


=head1 NAME

OpenSRF::Utils::JSON - Bucket-o-Routines for JSON

=head1 SYNOPSIS

C<O::U::JSON> is a functional-style package which exports nothing. All
calls to routines must use the fully-qualified name, and expect an
invocant, as in

    OpenSRF::Utils::JSON->JSON2perl($string);

Most routines are straightforward data<->JSON transformation wrappers
around L<JSON::XS>, but some (like L</register_class_hint>) provide
OpenSRF functionality.

=head1 ROUTINES

=head2 register_class_hint

=cut

sub register_class_hint {
    my ($pkg, %args) = @_;
    $_class_map{hints}{$args{hint}} = \%args;
    $_class_map{classes}{$args{name}} = \%args;
}

=head2 lookup_class

=cut

sub lookup_class {
    my ($pkg, $hint) = @_;
    return $_class_map{hints}{$hint}{name}
}

=head2 lookup_hint

=cut

sub lookup_hint {
    my ($pkg, $class) = @_;
    return $_class_map{classes}{$class}{hint}
}

=head2 JSON2perl

=cut

sub JSON2perl {
    my( $pkg, $string ) = @_;
    my $perl = $class->rawJSON2perl($string);
    return $class->JSONObject2Perl($perl);
}

=head2 perl2JSON

=cut

sub perl2JSON {
    my( $pkg, $obj ) = @_;
    my $json = $pkg->perl2JSONObject($obj);
    return $pkg->rawPerl2JSON($json);
}

=head2 rawJSON2perl

Internal routine used by L</JSON2Perl>. Wrapper around
L<JSON::XS::decode>.

=cut

sub rawJSON2perl {
    my ($class, $json) = @_;
    return undef unless defined $json and $json !~ /^\s*$/o;
    return $parser->decode($json);
}

=head2 rawPerl2JSON

Internal routine used by L</Perl2JSON>. Wrapper around
L<JSON::XS::encode>.

=cut

sub rawPerl2JSON {
    my ($class, $perl) = @_;
    return $parser->encode($perl);
}

=head2 JSONObject2Perl

=cut

sub JSONObject2Perl {
    my ($pkg, $obj) = @_;
    my $ref = ref $obj;
    if( $ref eq 'HASH' ) {
        if( defined($obj->{$JSON_CLASS_KEY})) {
            my $class = $obj->{$JSON_CLASS_KEY};
            $class =~ s/^\s+//o;
            $class =~ s/\s+$//o;
            if( $obj = $pkg->JSONObject2Perl($obj->{$JSON_PAYLOAD_KEY}) ) {
                $class = $pkg->lookup_class($class) || $class;
                return bless(\$obj, $class) unless ref($obj);
                return bless($obj, $class);
            }
            return undef;
        }
        for my $k (keys %$obj) {
            $obj->{$k} = $pkg->JSONObject2Perl($obj->{$k})
              unless ref($obj->{$k}) eq 'JSON::XS::Boolean';
        }
    } elsif( $ref eq 'ARRAY' ) {
        for my $i (0..scalar(@$obj) - 1) {
            $obj->[$i] = $pkg->JSONObject2Perl($obj->[$i])
              unless ref($obj->[$i]) eq 'JSON::XS::Boolean';
        }
    }
    return $obj;
}

=head2 perl2JSONObject

=cut

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

=head2 true

Wrapper for JSON::XS::true. J::X::true and J::X::false, according to
its documentation, "are JSON atoms become JSON::XS::true and
JSON::XS::false, respectively. They are overloaded to act almost
exactly like the numbers 1 and 0"

=cut

sub true { return $parser->true }

=head2 false

See L</true>

=cut

sub false { return $parser->false }

1;

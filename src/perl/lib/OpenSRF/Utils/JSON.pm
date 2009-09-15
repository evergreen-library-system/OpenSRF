package OpenSRF::Utils::JSON;

use warnings;
use strict;
use JSON::XS;

our $parser = JSON::XS->new;
$parser->ascii(1);        # output \u escaped strings for any char with a value over 127
$parser->allow_nonref(1); # allows non-reference values to equate to themselves (see perldoc)

our %_class_map = ();
our $JSON_CLASS_KEY = '__c';   # points to the classname of encoded objects
our $JSON_PAYLOAD_KEY = '__p'; # same, for payload



=head1 NAME

OpenSRF::Utils::JSON - Serialize/Vivify objects

=head1 SYNOPSIS

C<O::U::JSON> is a functional-style package which exports nothing. All
calls to routines must use the fully-qualified name, and expect an
invocant, as in

    OpenSRF::Utils::JSON->JSON2perl($string);

The routines which are called by existing external code all deal with
the serialization/stringification of objects and their revivification.



=head1 ROUTINES

=head2 register_class_hint

This routine is used by objects which wish to serialize themselves
with the L</perl2JSON> routine. It has two required arguments, C<name>
and C<hint>.

    O::U::J->register_class_hint( hint => 'osrfException',
                                  name => 'OpenSRF::DomainObject::oilsException');

Where C<hint> can be any unique string (but canonically is the name
from the IDL which matches the object being operated on), and C<name>
is the language-specific classname which objects will be revivified
as.

=cut

sub register_class_hint {
    # FIXME hint can't be a dupe
    # FIXME fail unless we have hint and name
    my ($pkg, %args) = @_;
    # FIXME why is the same thing shoved into two places? One mapping
    # would suffice if class and hint were always returned together...
    $_class_map{hints}{$args{hint}} = \%args;
    $_class_map{classes}{$args{name}} = \%args;
}


=head2 JSON2perl

Given a JSON-encoded string, returns a vivified Perl object built from
that string.

=cut

sub JSON2perl {
    # FIXME $string is not checked for any criteria, even existance
    my( $pkg, $string ) = @_;
    my $perl = $pkg->rawJSON2perl($string);
    return $pkg->JSONObject2Perl($perl);
}


=head2 perl2JSON

Given a Perl object, returns a JSON stringified representation of that
object.

=cut

sub perl2JSON {
    my( $pkg, $obj ) = @_;
    # FIXME no validation of any sort
    my $json = $pkg->perl2JSONObject($obj);
    return $pkg->rawPerl2JSON($json);
}



=head1 INTERNAL ROUTINES

=head2 rawJSON2perl

Intermediate routine called by L</JSON2Perl>.

=cut

sub rawJSON2perl {
    my ($pkg, $json) = @_;
    # FIXME change regex conditional to '=~ /\S/'
    return undef unless (defined $json and $json !~ /^\s*$/o);
    return $parser->decode($json);
}


=head2 JSONObject2Perl

Final routine in the object re-vivification chain, called by L</rawJSON2perl>.

=cut

sub JSONObject2Perl {
    my ($pkg, $obj) = @_;

    # if $obj is a hash
    if ( ref $obj eq 'HASH' ) {
        # and if it has the "I'm a class!" marker
        if ( defined $obj->{$JSON_CLASS_KEY} ) {
            # vivify the payload
            my $vivobj = $pkg->JSONObject2Perl($obj->{$JSON_PAYLOAD_KEY});
            return undef unless defined $vivobj;

            # and bless it back into an object
            my $class = $obj->{$JSON_CLASS_KEY};
            $class =~ s/^\s+//; # FIXME pretty sure these lines could condense to 's/\s+//g'
            $class =~ s/\s+$//;
            $class = $pkg->lookup_class($class) || $class;
            return bless(\$vivobj, $class) unless ref $vivobj;
            return bless($vivobj, $class);
        }

        # is a hash, but no class marker; simply revivify innards
        for my $k (keys %$obj) {
            $obj->{$k} = $pkg->JSONObject2Perl($obj->{$k})
              unless ref $obj->{$k} eq 'JSON::XS::Boolean';
        }
    } elsif ( ref $obj eq 'ARRAY' ) {
        # not a hash; an array. revivify.
        for my $i (0..scalar(@$obj) - 1) {
            $obj->[$i] = $pkg->JSONObject2Perl($obj->[$i])
              unless ref $obj->[$i] eq 'JSON::XS::Boolean';
        }
    }

    # return vivified non-class hashes, all arrays, and anything that
    # isn't a hash or array ref
    return $obj;
}


=head2 rawPerl2JSON

Intermediate routine used by L</Perl2JSON>.

=cut

sub rawPerl2JSON {
    # FIXME no validation of any sort
    my ($pkg, $perl) = @_;
    return $parser->encode($perl);
}


=head2 perl2JSONObject

=cut

sub perl2JSONObject {
    my ($pkg, $obj) = @_;
    my $ref = ref $obj;

    return $obj unless $ref;

    return $obj if $ref eq 'JSON::XS::Boolean';
    my $newobj;

    if(UNIVERSAL::isa($obj, 'HASH')) {
        $newobj = {};
        $newobj->{$_} = $pkg->perl2JSONObject($obj->{$_}) for (keys %$obj);
    } elsif(UNIVERSAL::isa($obj, 'ARRAY')) {
        $newobj = [];
        $newobj->[$_] = $pkg->perl2JSONObject($obj->[$_]) for(0..scalar(@$obj) - 1);
    }

    if($ref ne 'HASH' and $ref ne 'ARRAY') {
        $ref = $pkg->lookup_hint($ref) || $ref;
        $newobj = {$JSON_CLASS_KEY => $ref, $JSON_PAYLOAD_KEY => $newobj};
    }

    return $newobj;
}


=head2 lookup_class

=cut

sub lookup_class {
    # FIXME when there are tests, see if these two routines can be
    # rewritten as one, or at least made to do lookup in the structure
    # they're named after. best case: flatten _class_map, since hints
    # and classes are identical
    my ($pkg, $hint) = @_;
    return $_class_map{hints}{$hint}{name}
}


=head2 lookup_hint

=cut

sub lookup_hint {
    my ($pkg, $class) = @_;
    return $_class_map{classes}{$class}{hint}
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

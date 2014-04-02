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
    # FIXME hint can't be a dupe?
    # FIXME fail unless we have hint and name?
    # FIXME validate hint against IDL?
    my ($pkg, %args) = @_;
    # FIXME maybe not just store a reference to %args; the lookup
    # functions are really confusing at first glance as a side effect
    # of this
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
object.  Callers should not expect that the JSON string has hash keys
sorted in any particular order.

=cut

sub perl2JSON {
    my( $pkg, $obj ) = @_;
    # FIXME no validation of any sort
    my $json = $pkg->perl2JSONObject($obj);
    return $pkg->rawPerl2JSON($json);
}



=head1 INTERNAL ROUTINES

=head2 rawJSON2perl

Performs actual JSON -> data transformation, before
L</JSONObject2Perl> is called.

=cut

sub rawJSON2perl {
    my ($pkg, $json) = @_;
    return undef unless (defined $json and $json =~ /\S/o);
    return $parser->decode($json);
}


=head2 rawPerl2JSON

Performs actual data -> JSON transformation, after L</perl2JSONObject>
has been called.

=cut

sub rawPerl2JSON {
    # FIXME is there a reason this doesn't return undef with no
    # content as rawJSON2perl does?
    my ($pkg, $perl) = @_;
    return $parser->encode($perl);
}


=head2 JSONObject2Perl

Routine called by L</JSON2perl> after L</rawJSON2perl> is called.

At this stage, the JSON string will have been vivified as data. This
routine's job is to turn it back into an OpenSRF system object of some
sort, if possible.

If it's not possible, the original data (structure), or one very much
like it will be returned.

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
            $class = $pkg->lookup_class($class) if $pkg->lookup_class($class);
            return bless(\$vivobj, $class) unless ref $vivobj;
            return bless($vivobj, $class);
        }

        # is a hash, but no class marker; simply revivify innards
        for my $k (keys %$obj) {
            $obj->{$k} = $pkg->JSONObject2Perl($obj->{$k})
              unless JSON::XS::is_bool $obj->{$k};
        }
    } elsif ( ref $obj eq 'ARRAY' ) {
        # not a hash; an array. revivify.
        for my $i (0..scalar(@$obj) - 1) {
            $obj->[$i] = $pkg->JSONObject2Perl($obj->[$i])
              unless JSON::XS::is_bool $obj->[$i];
              # FIXME? This does nothing except leave any Booleans in
              # place, without recursively calling this sub on
              # them. I'm not sure if that's what's supposed to
              # happen, or if they're supposed to be thrown out of the
              # array
        }
    }

    # return vivified non-class hashes, all arrays, and anything that
    # isn't a hash or array ref
    return $obj;
}


=head2 perl2JSONObject

Routine called by L</perl2JSON> before L</rawPerl2JSON> is called.

For OpenSRF system objects which have had hints about their classes
stowed via L</register_class_hint>, this routine acts as a wrapper,
encapsulating the incoming object in metadata about itself. It is not
unlike the process of encoding IP datagrams.

The only metadata encoded at the moment is the class hint, which is
used to reinflate the data as an object of the appropriate type in the
L</JSONObject2perl> routine.

Other forms of data more-or-less come out as they went in, although
C<CODE> or C<SCALAR> references will return what looks like an OpenSRF
packet, but with a class hint of their reference type and an C<undef>
payload.

=cut

sub perl2JSONObject {
    my ($pkg, $obj) = @_;
    my $ref = ref $obj;

    return $obj if !$ref or JSON::XS::is_bool $obj;

    my $jsonobj;

    if(UNIVERSAL::isa($obj, 'HASH')) {
        $jsonobj = {};
        $jsonobj->{$_} = $pkg->perl2JSONObject($obj->{$_}) for (keys %$obj);
    } elsif(UNIVERSAL::isa($obj, 'ARRAY')) {
        $jsonobj = [];
        $jsonobj->[$_] = $pkg->perl2JSONObject($obj->[$_]) for(0..scalar(@$obj) - 1);
    }

    if($ref ne 'HASH' and $ref ne 'ARRAY') {
        $ref = $_class_map{classes}{$ref}{hint} || $ref;
        $jsonobj = {$JSON_CLASS_KEY => $ref, $JSON_PAYLOAD_KEY => $jsonobj};
    }

    return $jsonobj;
}


=head2 lookup_class

Given a class hint, returns the classname matching it. Returns undef
on failure.

=cut

sub lookup_class {
    # FIXME when there are tests, see if these two routines can be
    # rewritten as one, or at least made to do lookup in the structure
    # they're named after. best case: flatten _class_map, since hints
    # and classes are identical
    my ($pkg, $hint) = @_;
    return undef unless $hint;
    return $_class_map{hints}{$hint}{name}
}


=head2 lookup_hint

Given a classname, returns the class hint matching it. Returns undef
on failure.

=cut

sub lookup_hint {
    my ($pkg, $class) = @_;
    return undef unless $class;
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

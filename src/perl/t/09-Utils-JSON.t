#!perl -T

use Test::More tests => 49;

use OpenSRF::Utils::JSON;


#
# initial state from use
#

# do we have a JSON::XS object?
is (ref $OpenSRF::Utils::JSON::parser,   'JSON::XS');

# make sure the class and payload keys are as expected
is ($OpenSRF::Utils::JSON::JSON_CLASS_KEY,   '__c');
is ($OpenSRF::Utils::JSON::JSON_PAYLOAD_KEY, '__p');

# start with the simplest bits possible
is (OpenSRF::Utils::JSON::true, 1);
is (OpenSRF::Utils::JSON->true, 1);
is (OpenSRF::Utils::JSON->false, 0);


#
# register_class_hint
my $testmap =  { hints   => { osrfException =>
                              { hint => 'osrfException',
                                name => 'OpenSRF::DomainObject::oilsException' }
                            },
                 classes => { OpenSRF::DomainObject::oilsException =>
                              { hint => 'osrfException',
                                name => 'OpenSRF::DomainObject::oilsException' }
                            }
               };
OpenSRF::Utils::JSON->register_class_hint( hint => 'osrfException',
                                           name => 'OpenSRF::DomainObject::oilsException');
is_deeply (\%OpenSRF::Utils::JSON::_class_map, $testmap);


#
# lookup_class
is (OpenSRF::Utils::JSON->lookup_class('osrfException'), 'OpenSRF::DomainObject::oilsException');
is (OpenSRF::Utils::JSON->lookup_class(37), undef, "Argument doesn't exist");
is (OpenSRF::Utils::JSON->lookup_class(''), undef, "Null string lookup");
is (OpenSRF::Utils::JSON->lookup_class(), undef, "Null request");


#
# lookup_hint
is (OpenSRF::Utils::JSON->lookup_hint('OpenSRF::DomainObject::oilsException'), 'osrfException');
is (OpenSRF::Utils::JSON->lookup_hint(37), undef, "Argument doesn't exist");
is (OpenSRF::Utils::JSON->lookup_hint(''), undef, "Null string lookup");
is (OpenSRF::Utils::JSON->lookup_hint(), undef, "Null request");


#
# rawPerl2JSON
my $struct = [ { foo => 'bar' }, 'baz', 'quux', 'x'];
is (OpenSRF::Utils::JSON->rawPerl2JSON($struct),
    '[{"foo":"bar"},"baz","quux","x"]');
is (OpenSRF::Utils::JSON->rawPerl2JSON(''), '""', "Null string as argument");


#
# rawJSON2perl
is_deeply (OpenSRF::Utils::JSON->rawJSON2perl(OpenSRF::Utils::JSON->rawPerl2JSON($struct)),
           [ { foo => 'bar' }, 'baz', 'quux', 'x']);
is (OpenSRF::Utils::JSON->rawJSON2perl(), undef, "Null argument");
is (OpenSRF::Utils::JSON->rawJSON2perl(''), undef, "Null string as argument"); # note inconsistency with above


#
# perl2JSONObject
is (OpenSRF::Utils::JSON->perl2JSONObject(),      undef, "Returns argument unless it's a ref");
is (OpenSRF::Utils::JSON->perl2JSONObject(3),     3,     "Returns argument unless it's a ref");
is (OpenSRF::Utils::JSON->perl2JSONObject('foo'), 'foo', "Returns argument unless it's a ref");

ok (JSON::XS::is_bool(OpenSRF::Utils::JSON->true), 'OpenSRF::Utils::JSON->true is a Boolean according to JSON::XS');
is (OpenSRF::Utils::JSON->perl2JSONObject(OpenSRF::Utils::JSON->true), '1', "Returns argument if it's a Boolean according to JSON");

my $hashref = { foo => 'bar' };
is (UNIVERSAL::isa($hashref,'HASH'), 1);
is_deeply (OpenSRF::Utils::JSON->perl2JSONObject($hashref), { foo => 'bar' }, "Passing in unblessed hashref");

my $arryref = [ 11, 12 ];
is (UNIVERSAL::isa($arryref,'ARRAY'), 1);
is_deeply (OpenSRF::Utils::JSON->perl2JSONObject($arryref), [ 11, 12 ], "Passing in unblessed arrayref");

my $coderef = sub { return 0 };            # this is almost certainly undesired behavior, but the
is (UNIVERSAL::isa($coderef,'CODE'), 1);   # code doesn't stop me from doing it
is_deeply (OpenSRF::Utils::JSON->perl2JSONObject($coderef),
           { __c => 'CODE', __p => undef }, "Passing in coderef");

my $fakeobj = bless { foo => 'bar' }, 'OpenSRF::DomainObject::oilsException';
is (UNIVERSAL::isa($fakeobj,'HASH'), 1);
my $jsonobj = OpenSRF::Utils::JSON->perl2JSONObject($fakeobj);
is_deeply ($jsonobj, { __c => 'osrfException', __p => { foo => 'bar' } },
           "Wrap object into an OpenSRF-shaped packet");


#
# perl2JSON
my $jsonstr = OpenSRF::Utils::JSON->perl2JSON($fakeobj);
is ($jsonstr, '{"__c":"osrfException","__p":{"foo":"bar"}}');


#
# JSONObject2Perl
is (OpenSRF::Utils::JSON->JSONObject2Perl(),      undef, "Returns argument unless it's a ref");
is (OpenSRF::Utils::JSON->JSONObject2Perl(3),     3,     "Returns argument unless it's a ref");
is (OpenSRF::Utils::JSON->JSONObject2Perl('foo'), 'foo', "Returns argument unless it's a ref");
is (OpenSRF::Utils::JSON->JSONObject2Perl($coderef), $coderef, "Returns argument unless it's a ref");

is_deeply (OpenSRF::Utils::JSON->JSONObject2Perl([11, 12]), [11, 12], "Arrayrefs get reconstructed as themselves");
is_deeply (OpenSRF::Utils::JSON->JSONObject2Perl([11, OpenSRF::Utils::JSON->true, 12]), [11, OpenSRF::Utils::JSON->true, 12],
           "Even when they contain JSON::XS  Booleans; those just don't get recursed upon");
           # note: [11, 1, 12] doesn't work here, even though you can do math on J::X Booleans

is_deeply (OpenSRF::Utils::JSON->JSONObject2Perl($hashref), { foo => 'bar' }, "Hashrefs without the class flag also get turned into themselves");
is_deeply (OpenSRF::Utils::JSON->JSONObject2Perl({ foo => OpenSRF::Utils::JSON->true, bar => 'baz' }), 
           { foo => OpenSRF::Utils::JSON->true, bar => 'baz'},
           "Even when they contain JSON::XS  Booleans; those just don't get recursed upon");

my $vivobj = OpenSRF::Utils::JSON->JSONObject2Perl($jsonobj);
is (ref $vivobj, 'OpenSRF::DomainObject::oilsException');
is_deeply ($vivobj, { foo => 'bar' }, "perl2JSONObject-packaged things get blessed to their original contents and class");

my $codeobj = OpenSRF::Utils::JSON->perl2JSONObject($coderef);
is_deeply (OpenSRF::Utils::JSON->JSONObject2Perl($codeobj), undef, "Things with undefined payloads (see above)return undef");

$vivobj = OpenSRF::Utils::JSON->JSONObject2Perl({ __c => 'foo', __p => 'bar' });
is (ref $vivobj, 'foo');
is_deeply ($vivobj, \'bar', "Scalar payload and non-resolvable class hint vivifies to a scalar *ref* and a class of the class flag");


#
# json2Perl
my $perlobj = OpenSRF::Utils::JSON->JSON2perl($jsonstr);
is (ref $perlobj, 'OpenSRF::DomainObject::oilsException');
is_deeply ($perlobj,  { foo => 'bar' }, "Successful revivification from JSON in one step");

#!perl -T

use Test::More tests => 6;

use OpenSRF::Utils::JSON;

# do we have a JSON::XS object?
is (ref $OpenSRF::Utils::JSON::parser,   'JSON::XS');

# make sure the class and payload keys are as expected
is ($OpenSRF::Utils::JSON::JSON_CLASS_KEY,   '__c');
is ($OpenSRF::Utils::JSON::JSON_PAYLOAD_KEY, '__p');

# start with the simplest bits possible
is (OpenSRF::Utils::JSON::true, 1);
is (OpenSRF::Utils::JSON->true, 1);
is (OpenSRF::Utils::JSON->false, 0);


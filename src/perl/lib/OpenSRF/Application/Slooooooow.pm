package OpenSRF::Application::Slooooooow;
use base qw/OpenSRF::Application/;
use OpenSRF::Application;

use OpenSRF::Utils::SettingsClient;
use OpenSRF::EX qw/:try/;
use OpenSRF::Utils qw/:common/;
use OpenSRF::Utils::Logger;

my $log;

sub initialize {
    $log = 'OpenSRF::Utils::Logger';
}

sub child_init {}

sub wait_for_it {
    my $self = shift;
    my $client = shift;
    my $pause = shift;

    $pause =~ s/\D//g if (defined $pause);
    $pause //= 1;

    $log->info("Holding for $pause seconds...");
    sleep($pause);
    $log->info("Done waiting, time to return.");
    return [$pause, @_]
}
__PACKAGE__->register_method(
    api_name        => 'opensrf.slooooooow.wait',
    method          => 'wait_for_it',
    argc            => 1,
    signature       => {
        params => [
            {name => "pause", type => "number", desc => "Seconds to sleep, can be fractional"},
            {name => "extra", type => "string", desc => "Extra optional parameter used to inflate the payload size"}
        ],
        return => {
            desc => "Array of passed parameters",
            type => "array"
        }
    }

);

1;

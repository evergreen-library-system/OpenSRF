package OpenSRF::Application::Validator::Invalid;
use base qw/OpenSRF::Application::Validator::Base/;

use OpenSRF::Application::Validator::Base;

use strict;
use warnings;

sub validate {
    my $self = shift;
    my $input = shift;
    my $settings = shift;
    return { 'valid' => 0, 'normalized' => $input, 'error' => 'Forced Invalid' };
}

1;

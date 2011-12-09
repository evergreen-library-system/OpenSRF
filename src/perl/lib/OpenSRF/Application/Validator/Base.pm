package OpenSRF::Application::Validator::Base;
use strict;
use warnings;

sub validate {
    my $self = shift;
    my $input = shift;
    my $settings = shift;
    return { 'valid' => 1, 'normalized' => $input };
}

1;

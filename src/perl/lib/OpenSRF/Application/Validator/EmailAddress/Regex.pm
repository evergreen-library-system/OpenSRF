package OpenSRF::Application::Validator::EmailAddress::Regex;
use base qw/OpenSRF::Application::Validator::Base/;

use OpenSRF::Application::Validator::Base;

use strict;
use warnings;

sub validate {
    my $self = shift;
    my $input = shift;
    my $settings = shift;

    if(!$input->{emailaddress}) {
        return { 'valid' => 0, 'normalized' => $input, 'error' => 'No Address' };
    } elsif ($input->{emailaddress} !~ /^.+@[^@]+\.[^@]{2,}$/) {
        return { 'valid' => 0, 'normalized' => $input, 'error' => 'Bad Address - Regex Check Failed' };
    }
    $input->{emailaddress} = lc($input->{emailaddress});
    return { 'valid' => 1, 'normalized' => $input };
}

1;

package OpenSRF::Application::Validator::EmailAddress::DNS;
use base qw/OpenSRF::Application::Validator::Base/;

use OpenSRF::Application::Validator::Base;
use Net::DNS;

use strict;
use warnings;

sub validate {
    my $self = shift;
    my $input = shift;
    my $settings = shift;
    
    my $return = { 'valid' => 0, 'normalized' => $input };

    if(!$input->{emailaddress}) {
        return { 'valid' => 0, 'normalized' => $input, 'error' => 'No Address' };
    } elsif ($input->{emailaddress} !~ /@([^@]+)$/) {
        return { 'valid' => 0, 'normalized' => $input, 'error' => 'Bad Address - Regex Check Failed' };
    }
    my $domain = $1;
    my $checkMXA = $settings->{check_mx_a};
    my $checkAAAA = $settings->{check_aaaa};
    my $config_file = $settings->{config_file};
    my @badAddrs;
    if($settings->{ignore_ips}) {
        @badAddrs = split(',', $settings->{ignore_ips});
    }
    my $res;
    $res = Net::DNS::Resolver->new(config_file => $config_file, defnames => 0) if $config_file;
    $res = Net::DNS::Resolver->new(defnames => 0) if !$config_file;
    my @arecords;
    # Look for MX records first
    my $answer = $res->send($domain, 'MX');
    foreach($answer->answer) {
        if($_->type eq 'MX') {
            push(@arecords, $_->exchange);
        }
    }
    if(@arecords) {
        if($checkMXA) {
            OUTER: foreach my $checkdomain (@arecords) {
                $answer = $res->send($checkdomain, 'A');
                foreach my $record ($answer->answer) {
                    last if $record->type eq 'CNAME' || $record->type eq 'DNAME';
                    if($record->type eq 'A') {
                        next if grep { $_ eq $record->address } @badAddrs;
                        $return->{valid} = 1;
                        last OUTER;
                    }
                }
                if($checkAAAA) {
                    $answer = $res->send($checkdomain, 'AAAA');
                    foreach my $record ($answer->answer) {
                        last if $record->type eq 'CNAME' || $record->type eq 'DNAME';
                        if($record->type eq 'AAAA') {
                            next if grep { $_ eq $record->address } @badAddrs;
                            $return->{valid} = 1;
                            last OUTER;
                        }
                    }
                }
            }
            $return->{error} = "MX Records Invalid" if(!$return->{valid});
        } else {
            $return->{valid} = 1;
        }
    } else {
        $answer = $res->send($domain,'A');
        foreach my $record ($answer->answer) {
            last if $record->type eq 'CNAME' || $record->type eq 'DNAME';
            if($record->type eq 'A') {
                next if grep { $_ eq $record->address } @badAddrs;
                $return->{valid} = 1;
                last;
            }
        }
        if(!$return->{valid} && $checkAAAA) {
            $answer = $res->send($domain, 'AAAA');
            foreach my $record ($answer->answer) {
                last if $record->type eq 'CNAME' || $record->type eq 'DNAME';
                if($record->type eq 'AAAA') {
                    next if grep { $_ eq $record->address } @badAddrs; 
                    $return->{valid} = 1;
                    last;
                }
            }
        }
        $return->{error} = "No A Records Found" if(!$return->{valid});
    }
    $return->{normalized}->{emailaddress} = lc($return->{normalized}->{emailaddress}) if($return->{valid});
    return $return;
}

1;

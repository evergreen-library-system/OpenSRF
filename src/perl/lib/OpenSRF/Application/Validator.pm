package OpenSRF::Application::Validator;
use base qw/OpenSRF::Application/;
use OpenSRF::Application;
use OpenSRF::Utils::SettingsClient;
use OpenSRF::Utils::Logger;
use Module::Load;

my $logger = OpenSRF::Utils::Logger;
my %modules;

sub initialize {
    my $sc = OpenSRF::Utils::SettingsClient->new;
    my $validators = $sc->config_value( apps => 'opensrf.validator' => app_settings => 'validators' );
    while(my $module = each %$validators ) {
        __PACKAGE__->register_method(
            api_name => "opensrf.validator.$module.validate",
            method => 'do_validate',
            argc => 1,
            validator_name => $module
        );
        $modules{$module} = $validators->{$module};
    }
}

sub do_validate {
    my $self = shift;
    my $client = shift;
    my $input = shift;
    my $return = { 'valid' => 1, 'normalized' => $input }; # Default return
    my $validators = $modules{$self->{validator_name}}->{modules};
    my @validator_names = sort keys %$validators;
    my $additionals = ();

    my $submodulename, $submodule;
    while($return->{valid} && ($submodulename = shift @validator_names)) {
        $submodule = $validators->{$submodulename};
        my $implementation = $submodule->{implementation};
        $logger->debug("Running request through $submodulename ($implementation)");
        load $implementation;
        my $result = $implementation->validate($return->{normalized}, $submodule);
        if($result) {
            $return = $result;
            $additionals = {%$additionals, %{$return->{additionals}}} if $return->{additionals};
        }
    }
    $return->{additionals} = $additionals;
    return $return;
}

1;

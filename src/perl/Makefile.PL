use inc::Module::Install;

# Define metadata
name           'OpenSRF';
all_from       'lib/OpenSRF.pm';
abstract       '';

# Specific dependencies
requires       'File::Spec'  => '0.80';
test_requires  'Test::More'  => '0.42';
recommends     'Text::CSV_XS'=> '0.50';
no_index       'directory'   => 'demos';
install_script 'myscript';

WriteAll;

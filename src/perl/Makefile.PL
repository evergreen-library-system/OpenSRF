use inc::Module::Install;

# Define metadata
name           'OpenSRF';
all_from       'lib/OpenSRF.pm';
license        'perl';

# Specific dependencies
requires 'Cache::Memcached' => 0;
requires 'Data::Dumper'     => 0;
requires 'DateTime'         => 0;
requires 'DBI'              => 0;
requires 'Digest::MD5'      => 0;
requires 'Errno'            => 0;
requires 'Error'            => 0;
requires 'FreezeThaw'       => 0;
requires 'IO'               => 0;
requires 'JSON::XS'         => 0;
requires 'Net::Domain'      => 0;
requires 'Net::Server'      => 0;
requires 'Time::HiRes'      => 0;
requires 'Time::Local'      => 0;
requires 'UNIVERSAL::require' => 0;
requires 'Unix::Syslog'       => 0;
requires 'XML::LibXML'        => 0;
requires 'DateTime::Format::ISO8601' => 0;

WriteAll;

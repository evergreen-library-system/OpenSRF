use Module::Build;

my $build = Module::Build->new( module_name => 'OpenSRF',
                                license => 'perl',
                                requires => { 'Cache::Memcached' => 0,
                                              'Data::Dumper'     => 0,
                                              'DateTime'         => 0,
                                              'DBI'              => 0,
                                              'Digest::MD5'      => 0,
                                              'Errno'            => 0,
                                              'Error'            => 0,
                                              'IO'               => 0,
                                              'JSON::XS'         => 0,
                                              'Net::Domain'      => 0,
                                              'Net::Server'      => 0,
                                              'Time::HiRes'      => 0,
                                              'Time::Local'      => 0,
                                              'UNIVERSAL::require' => 0,
                                              'Unix::Syslog'       => 0,
                                              'XML::LibXML'        => 0,
                                              'DateTime::Format::ISO8601' => 0,
                                            },
                              );

$build->create_build_script;

# $build->dispatch('build');
# $build->dispatch('test');
# $build->dispatch('install');

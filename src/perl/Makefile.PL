use strict;
use warnings;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME                => 'OpenSRF',
    AUTHOR              => 'Equinox <foo@esilibrary.com>',
    VERSION_FROM        => 'lib/OpenSRF.pm',
    ABSTRACT_FROM       => 'lib/OpenSRF.pm',
    PL_FILES            => {},
    PREREQ_PM => {
        'Test::More' => 0,
    },
    dist                => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    clean               => { FILES => 'OpenSRF-*' },
);

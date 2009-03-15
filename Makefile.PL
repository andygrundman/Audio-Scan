use 5.008008;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME              => 'Audio::Scan',
    VERSION_FROM      => 'lib/Audio/Scan.pm',
    PREREQ_PM         => {},
    ($] >= 5.005 ?
      (ABSTRACT_FROM  => 'lib/Audio/Scan.pm', #
       AUTHOR         => 'Andy Grundman <andy@slimdevices.com>') : ()),
    LIBS              => ['-liconv -lid3tag'],
    DEFINE            => '',
    INC               => '-I.',
);

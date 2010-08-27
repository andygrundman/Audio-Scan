use strict;

use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

my (@INC, @LIBPATH, @LIBS);
my $MYEXTLIB;

if ( $^O !~ /Win32/ ) {
    push @LIBS, '-lz';
}

my $DEFINES = '-O2';
$DEFINES .= ' -Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security' unless $^O =~ /Win32|sun|solaris/i;

# we want to put the local include dirs in front since the flac include dir possibly added above
# might also contain incompatible versions of the local header files, which then would be chosen instead
# of the correct local ones (e.g. include/id3.h)
unshift @INC, '-I. -I.. -Isrc -Iinclude';

if ( $^O =~ /Win32/ ) {
    *MY::postamble = sub {};
    $MYEXTLIB .= 'win32/zlib.lib ';
}

my $inc_files = join(' ', glob 'include/*.h');
my $src_files = join(' ', glob 'src/*.c');

WriteMakefile(
    NAME              => 'Audio::Scan',
    VERSION_FROM      => 'lib/Audio/Scan.pm',
    PREREQ_PM         => {},
    ABSTRACT_FROM     => 'lib/Audio/Scan.pm',
    AUTHOR            => 'Andy Grundman <andy@slimdevices.com>',
    INC               => join(' ', @INC),
    LIBS              => [ join(' ', @LIBPATH, @LIBS) ],
    DEFINE            => $DEFINES,
    MYEXTLIB          => $MYEXTLIB,
    depend            => { 'Scan.c' => "$inc_files $src_files" },
);

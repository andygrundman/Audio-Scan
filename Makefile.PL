use strict;

use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

require 5.8.8;

my (@INC, @LIBPATH, @LIBS);
my ($FLAC, $MYEXTLIB);

if ( $^O !~ /Win32/ ) {
    push @LIBS, '-lz';
}

my ($help, $flac_inc, $flac_lib);
my $result = GetOptions(
    "help|h"                 => \$help,
    "with-flac-includes=s"   => \$flac_inc,
    "with-flac-libs=s"       => \$flac_lib,
);

if ( $help || !$result ) {
    print STDERR <<END;
Usage: perl Makefile.PL [options]

Configure Audio::Scan module.

Options:
     --with-flac-includes       Path to directory containing FLAC/all.h
     --with-flac-libs           Path to directory containing libFLAC

END

    exit 0;
}

my @check = qw(/usr/include /usr/local/include /opt/local/include);

# Look for libFLAC
for my $incdir ( $flac_inc, @check ) {
    if ( $incdir && -e catfile( $incdir, 'FLAC/all.h' ) ) {
        $FLAC = 1;
        $flac_inc = $incdir;

        push @INC, '-I' . $flac_inc;

        if ( $flac_lib ) {
            push @LIBPATH, '-L' . $flac_lib;
        }

        push @LIBS, '-lFLAC';
        last;
    }
}

print "\n";
print "Audio::Scan will be built with:\n\n";
print "   MP3 support:     yes\n";
print "  FLAC support:     " . ( $FLAC ? "yes ($flac_inc)" : "no, install libFLAC" ) . "\n";
print "   OGG support:     yes\n";
print "\n";

my $DEFINES = '-Wall';
$DEFINES .= ' -DHAVE_FLAC' if $FLAC;

# Support for building libid3tag
sub MY::postamble {
    my $postamble =<<'END';

libid3tag/Makefile:
	cd libid3tag && CC="$(CC)" CFLAGS="$(OPTIMIZE) $(CCFLAGS) $(CCCDLFLAGS)" ./configure --disable-dependency-tracking $(EXTRA_CONF)

clean::
	-cd libid3tag && make distclean

force:

$(MYEXTLIB): libid3tag/Makefile force
	cd libid3tag && $(MAKE) all

END
}

if ( $^O =~ /Win32/ ) {
    *MY::postamble = sub {};
    $MYEXTLIB = 'libid3tag/msvc++/Release/libid3tag.lib';
}
else {
    $MYEXTLIB = 'libid3tag/.libs/libid3tag$(LIB_EXT)';
}

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
    depend            => {
        'Scan.c' => 'buffer.h buffer.c tagutils-common.c tagutils-common.h tagutils-mp3.h tagutils-mp3.c tagutils-flac.h tagutils-flac.c tagutils-ogg.h tagutils-ogg.c',
    },
);

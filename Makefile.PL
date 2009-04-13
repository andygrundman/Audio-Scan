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

# Check in environment-supplied locations before standard ones - keeps cross compilers happier
unshift @check, split /:/, $ENV{INCLUDE};

if ( $^O =~ /Win32/ ) {
    push @check, '../flac-1.2.1/include';
}

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
print "   ASF support:     yes\n";
print "\n";

my $DEFINES = '-Wall';
$DEFINES .= ' -DHAVE_FLAC' if $FLAC;

push @INC, ' -I. -I.. -Isrc -Iinclude';

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
    *MY::postamble = sub {
        my $postamble = <<'END';

fixflac:
    cp ../flac-1.2.1/obj/release/lib/libFLAC_dynamic.dll blib/arch/auto/Audio/Scan

END
    };
    
    $MYEXTLIB = 'libid3tag/msvc++/Release/libid3tag.lib ';
    $MYEXTLIB .= '../zlib-1.2.3/projects/visualc6/Win32_LIB_Release/zlib.lib ';
    $MYEXTLIB .= '../flac-1.2.1/obj/release/lib/libFLAC_dynamic.lib ';
}
else {
    if ( $ENV{NO_LIBID3TAG} ) {
        # Link to external libid3tag
        *MY::postamble = sub {};
        push @LIBPATH, '-L' . $ENV{ID3TAG_LIBS} if $ENV{ID3TAG_LIBS};
        push @LIBS, '-lid3tag';
    }
    else {
        $MYEXTLIB = 'libid3tag/.libs/libid3tag$(LIB_EXT)';
    }
}

my $inc_files = join(' include/', qw(
    include/buffer.h common.h mp3.h flac.h ogg.h asf.h
));

my $src_files = join(' src/', qw(
    src/buffer.c common.c mp3.c flac.c ogg.c asf.c
));

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

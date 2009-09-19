use strict;

use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

require 5.8.1;

my (@INC, @LIBPATH, @LIBS);
my ($FLAC, $MYEXTLIB);

if ( $^O !~ /Win32/ ) {
    push @LIBS, '-lz';
}

my ($help, $flac_inc, $flac_lib, $flac_static);
my $result = GetOptions(
    "help|h"                 => \$help,
    "with-flac-includes=s"   => \$flac_inc,
    "with-flac-libs=s"       => \$flac_lib,
    "with-flac-static"       => \$flac_static,
);

if ( $help || !$result ) {
    print STDERR <<END;
Usage: perl Makefile.PL [options]

Configure Audio::Scan module.

Options:
     --with-flac-includes       Path to directory containing FLAC/all.h
     --with-flac-libs           Path to directory containing libFLAC
     --with-flac-static         Statically link in libFLAC (requires libFLAC.a)

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
    if ( $incdir && -e catfile( $incdir, 'FLAC/export.h' ) ) {
        # Check for a new enough version
        my $current;
        open my $fh, '<', catfile( $incdir, 'FLAC/export.h' );
        while ( <$fh> ) {
            if ( /FLAC_API_VERSION_CURRENT/ ) {
                $current = 1;
                last;
            }
        }
        close $fh;

        if ( $current ) {
            $FLAC = 1;
            $flac_inc = $incdir;

            push @INC, '-I' . $flac_inc;

            if ( $flac_lib ) {
                push @LIBPATH, '-L' . $flac_lib;
            }
        
            if ( $flac_static ) {
                if ( $flac_lib ) {
                    $MYEXTLIB .= $flac_lib . '/' . 'libFLAC$(LIB_EXT) ';
                    # Allow FLAC without Ogg support
                    if ( glob $flac_lib . '/' . 'libogg.*' ) {
                        $MYEXTLIB .= $flac_lib . '/' . 'libogg$(LIB_EXT) ';
                    }
                }
                else {
                    $MYEXTLIB .= $flac_inc . '/../lib/libFLAC$(LIB_EXT) ';
                    # Allow FLAC without Ogg support
                    if ( glob $flac_inc . '/../lib/libogg.*' ) {
                        $MYEXTLIB .= $flac_inc . '/../lib/libogg$(LIB_EXT) ';
                    }
                }
            }
            else {
                push @LIBS, '-lFLAC';
            }
        
            last;
        }
    }
}

print "\n";
print "Audio::Scan will be built with:\n\n";
print "   MP3 support:     yes\n";
print "   MP4 support:     yes\n";
print "  FLAC support:     " . ( $FLAC ? "yes ($flac_inc)" : "no, install libFLAC 1.1.3+" ) . "\n";
print "   OGG support:     yes\n";
print "   ASF support:     yes\n";
print "   MPC support:     yes\n";
print "   MAC support:     yes\n";
print "   WAV support:     yes\n";
print "   AIFF support:    yes\n";
print "\n";

my $DEFINES = '-Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security' unless $^O =~ /Win32|sun|solaris/i;
$DEFINES .= ' -DHAVE_FLAC' if $FLAC;

# we want to put the local include dirs in front since the flac include dir possibly added above
# might also contain incompatible versions of the local header files, which then would be chosen instead
# of the correct local ones (e.g. include/id3.h)
unshift @INC, '-I. -I.. -Isrc -Iinclude';

# Support for building libid3tag
sub MY::postamble {
    my $postamble =<<'END';

libid3tag/Makefile:
	cd libid3tag && CC="$(CC)" CFLAGS="$(OPTIMIZE) $(CCFLAGS) $(CCCDLFLAGS)" LDFLAGS="$(LDFLAGS)" ./configure --disable-dependency-tracking $(EXTRA_CONF)

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
    
    $MYEXTLIB .= 'libid3tag/msvc++/Release/libid3tag.lib ';
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
        $MYEXTLIB .= 'libid3tag/.libs/libid3tag$(LIB_EXT) ';
    }
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

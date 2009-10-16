use strict;

use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

require 5.8.8;

my (@INC, @LIBPATH, @LIBS);
my $MYEXTLIB;

if ( $^O !~ /Win32/ ) {
    push @LIBS, '-lz';
}

my $DEFINES = '-Wall' unless $^O =~ /sun|solaris/i;
$DEFINES .= ' -Wno-unused-value -Wno-format-security' unless $^O =~ /Win32|sun|solaris/i;

# we want to put the local include dirs in front since the flac include dir possibly added above
# might also contain incompatible versions of the local header files, which then would be chosen instead
# of the correct local ones (e.g. include/id3.h)
unshift @INC, '-I. -I.. -Isrc -Iinclude';

# Support for building libid3tag
sub MY::postamble {
    my $postamble =<<'END';

libid3tag/Makefile:
	cd libid3tag && CC="$(CC)" CFLAGS="$(OPTIMIZE) $(CCFLAGS) $(CCCDLFLAGS)" LDFLAGS="$(LDFLAGS)" /bin/sh configure --disable-dependency-tracking $(EXTRA_CONF)

clean::
	-cd libid3tag && make distclean

force:

$(MYEXTLIB): libid3tag/Makefile force
	cd libid3tag && $(MAKE) all

END
}

if ( $^O =~ /Win32/ ) {
    *MY::postamble = sub {};
    $MYEXTLIB .= 'libid3tag/msvc++/Release/libid3tag.lib ';
    $MYEXTLIB .= '../zlib-1.2.3/projects/visualc6/Win32_LIB_Release/zlib.lib ';
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

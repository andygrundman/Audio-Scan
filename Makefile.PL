use strict;

use ExtUtils::MakeMaker qw(WriteMakefile);
use File::Spec::Functions;
use Getopt::Long;

require 5.8.8;

my (@INC, @LIBPATH, @LIBS);
my ($MP3);

my ($help, $id3tag_inc, $id3tag_lib);
my $result = GetOptions(
    "help|h"                 => \$help,
    "with-id3tag-includes=s" => \$id3tag_inc,
    "with-id3tag-libs=s"     => \$id3tag_lib,
);

if ( $help || !$result ) {
    print STDERR <<END;
Usage: perl Makefile.PL [options]

Configure Audio::Scan module.

Options:
     --with-id3tag-includes     Path to directory containing id3tag.h
     --with-id3tag-libs         Path to directory containing libid3tag

END

    exit 0;
}

my @check = qw(/usr/include /usr/local/include /opt/local/include);

# Look for libid3tag
for my $incdir ( $id3tag_inc, @check ) {
    if ( $incdir && -e catfile( $incdir, 'id3tag.h' ) ) {
        $MP3 = 1;
        $id3tag_inc = $incdir;

        push @INC, '-I' . $id3tag_inc;

        if ( $id3tag_lib ) {
            push @LIBPATH, '-L' . $id3tag_lib;
        }

        push @LIBS, '-lid3tag';
        last;
    }
}

print "\n";
print "Audio::Scan will be built with:\n\n";
print "   MP3 support:     " . ( $MP3 ? "yes ($id3tag_inc)" : "no, install libid3tag" ) . "\n";

my $DEFINES = '-Wall';
$DEFINES .= ' -DHAVE_MP3' if $MP3;

if ( $DEFINES ) {
    print "\n";
}
else {
    print "\n";
    print "Please install at least one of the above libraries to use this module.\n";
    print "Run perl Makefile.PL -h for command-line options.\n\n";

    exit 0;
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
);

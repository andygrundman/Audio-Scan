use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 19;

use Audio::Scan;
use Data::Dump qw(dump);

# MD5 check
{
    my $s = Audio::Scan->scan( _f('md5.flac') );

    my $info = $s->{info};

    is($info->{MD5CHECKSUM}, '00428198e1ae27ad16754f75ff068752', 'MD5 Checksum ok');
}

# Application block
{
    my $s = Audio::Scan->scan( _f('appId.flac') );

    my $tags = $s->{tags};

    my $cue = $tags->{cuesheet};
    ok($cue, 'Cue sheet exists');

    my $app = $tags->{'application'}{1835361648};
    ok($app, "Application block exists");

    ok($app =~ /musicbrainz/, "Found musicbrainz block");
}

# Cross section of tests.
{
    my $s = Audio::Scan->scan( _f('test.flac') );

    my $tags = $s->{tags};
    my $info = $s->{info};

    is($info->{SAMPLERATE}, 44100, "Sample rate ok");
    is($info->{MD5CHECKSUM}, '592fb7897a3589c6acf957fd3f8dc854', 'MD5 checksum ok');
    is($info->{TOTALSAMPLES}, 153200460, 'Total samples ok');

    is($tags->{AUTHOR}, 'Praga Khan', 'AUTHOR ok');

    ok($info->{trackLengthFrames} =~ /70.00\d+/, "Track Length Frames ok");
    is($info->{trackLengthMinutes}, 57, "Track Length Minutes ok");
    ok($info->{bitRate} =~ /1.236\d+/, 'Bitrate ok');
    ok($info->{trackTotalLengthSeconds} =~ /3473.93\d+/, 'Total seconds ok');

    my $cue = $tags->{cuesheet};

    ok($cue, 'Got cuesheet ok');

    is(scalar @{$cue}, 37, 'Cuesheet length ok');

    ok($cue->[35] =~ /REM FLAC__lead-in 88200/, 'Cuesheet lead-in ok');
    ok($cue->[36] =~ /REM FLAC__lead-out 170 153200460/, 'Cuesheet lead-out ok');
}

# FLAC file with ID3 tag
{

    my $s = Audio::Scan->scan( _f('id3tagged.flac') );

    my $tags = $s->{tags};

    ok($tags->{title} =~ /Allegro Maestoso/, "Found title after ID3 tag ok.");
}

{
    my $s = Audio::Scan->scan( _f('picture.flac') );

    my $tags        = $s->{tags};
    my $vendor      = ''; # $s->{info}->{vendor_string};
    my $has_picture = 1;

    if ($vendor and $vendor =~ /libFLAC\s+(\d+\.\d+\.\d+)/) {
        $has_picture = 0 if ($1 lt '1.1.3');
    }

    SKIP: {
        skip "XS - No PICTURE support", 3 unless $has_picture;

        my $picture = $tags->{picture};

        ok($picture, "Found picture ok");
        is($picture->{3}{'mimeType'}, 'image/jpeg', 'Found Cover JPEG ok');
    }
}

sub _f {
    return catfile( $FindBin::Bin, 'flac', shift );
}

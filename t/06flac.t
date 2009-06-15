use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More;

use Audio::Scan;

SKIP:
{
    if ( !Audio::Scan->has_flac() ) {
        plan skip_all => 'FLAC support not available';
    }
    else {
        plan tests => 19;
    }
}

# MD5 check
{
    my $s = Audio::Scan->scan( _f('md5.flac') );

    my $info = $s->{info};

    is($info->{md5}, '00428198e1ae27ad16754f75ff068752', 'MD5 Checksum ok');
}

# Application block
{
    my $s = Audio::Scan->scan( _f('appId.flac') );

    my $tags = $s->{tags};

    my $cue = $tags->{CUESHEET_BLOCK};
    ok($cue, 'Cue sheet exists');

    my $app = $tags->{APPLICATION}{1835361648};
    ok($app, "Application block exists");

    ok($app =~ /musicbrainz/, "Found musicbrainz block");
}

# Cross section of tests.
{
    my $s = Audio::Scan->scan( _f('test.flac') );

    my $tags = $s->{tags};
    my $info = $s->{info};

    is($info->{samplerate}, 44100, "Sample rate ok");
    is($info->{md5}, '592fb7897a3589c6acf957fd3f8dc854', 'MD5 checksum ok');
    is($info->{total_samples}, 153200460, 'Total samples ok');

    is($tags->{AUTHOR}, 'Praga Khan', 'AUTHOR ok');

    ok($info->{frames} =~ /70.00\d+/, "Track Length Frames ok");
    ok($info->{bitrate} =~ /1.236\d+/, 'Bitrate ok');

    my $cue = $tags->{CUESHEET_BLOCK};

    ok($cue, 'Got cuesheet ok');

    is(scalar @{$cue}, 37, 'Cuesheet length ok');

    ok($cue->[35] =~ /REM FLAC__lead-in 88200/, 'Cuesheet lead-in ok');
    ok($cue->[36] =~ /REM FLAC__lead-out 170 153200460/, 'Cuesheet lead-out ok');
}

# FLAC file with ID3 tag
{

    my $s = Audio::Scan->scan( _f('id3tagged.flac') );

    my $tags = $s->{tags};

    ok($tags->{TITLE} =~ /Allegro Maestoso/, "Found title after ID3 tag ok.");
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

        my $picture = $tags->{PICTURE};

        ok($picture, "Found picture ok");
        is($picture->{3}{mime_type}, 'image/jpeg', 'Found Cover JPEG ok');
    }
}

# Find frame offset
{
    my $offset = Audio::Scan->find_frame( _f('audio-data.flac'), 4602 );
    is( $offset, 9768, 'Find frame VBR ok' );
}

{
    open my $fh, '<', _f('audio-data.flac');
    my $offset = Audio::Scan->find_frame_fh( flac => $fh, 1000 );
    close $fh;

    is( $offset, 4601, 'Find frame via filehandle ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'flac', shift );
}

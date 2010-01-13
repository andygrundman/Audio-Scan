use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 44;

use Audio::Scan;

my $HAS_ENCODE;
eval {
    require Encode;
    $HAS_ENCODE = 1;
};

# Basics
{
    my $s = Audio::Scan->scan( _f('test.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};
    
    SKIP:
    {
        skip 'Encode is not available', 1 unless $HAS_ENCODE;
        my $utf8 = Encode::decode_utf8('シチヅヲ');
        is($tags->{PERFORMER}, $utf8, 'PERFORMER (UTF8) Tag ok');
    }

    is($tags->{ARTIST}, 'Test Artist', 'ASCII Tag ok');
    is($tags->{YEAR}, 2009, 'Year Tag ok');
    ok($tags->{VENDOR} =~ /Xiph/, 'Vendor ok');

    is($info->{bitrate_average}, 9887, 'Bitrate ok');
    is($info->{channels}, 2, 'Channels ok');
    is($info->{file_size}, 4553, 'File size ok' );
    is($info->{stereo}, 1, 'Stereo ok');
    is($info->{samplerate}, 44100, 'Sample Rate ok');
    is($info->{song_length_ms}, 3684, 'Song length ok');
    is($info->{audio_offset}, 4204, 'Audio offset ok');
}

# Multiple tags.
{
    my $s = Audio::Scan->scan( _f('multiple.ogg') );

    my $tags = $s->{tags};

    is($tags->{ARTIST}[0], 'Multi 1', 'Multiple Artist 1 ok');
    is($tags->{ARTIST}[1], 'Multi 2', 'Multiple Artist 1 ok');
    is($tags->{ARTIST}[2], 'Multi 3', 'Multiple Artist 1 ok');
}

# Equals char in tag.
{
    my $s = Audio::Scan->scan( _f('equals-char.ogg') );

    my $tags = $s->{tags};

    is($tags->{TITLE}, 'Me - You = Loneliness', 'Equals char in tag ok');
}

# Large page size.
{
    my $s = Audio::Scan->scan( _f('large-pagesize.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};

    is($info->{audio_offset}, 110616, 'Large page size audio offset ok');
    is($tags->{TITLE}, 'Deadzy', 'Large page title tag ok');
    is($tags->{ARTIST}, 'Medeski Scofield Martin & Wood', 'Large page artist tag ok');
    is($tags->{ALBUM}, 'Out Louder (bonus disc)', 'Large page album tag ok');
    is(length($tags->{COVERART}), 104704, 'Cover art ok');
}

# Test ignoring artwork
{
    local $ENV{AUDIO_SCAN_NO_ARTWORK} = 1;
    
    my $s = Audio::Scan->scan( _f('large-pagesize.ogg') );
    
    my $tags = $s->{tags};
    
    is( $tags->{COVERART}, 104704, 'Cover art with AUDIO_SCAN_NO_ARTWORK ok');
}

# Old encoder files.
{
    my $s1 = Audio::Scan->scan( _f('old1.ogg') );
    is($s1->{tags}->{ALBUM}, 'AutoTests', 'Old encoded album tag ok');
    is($s1->{info}->{samplerate}, 8000, 'Old encoded rate ok');

    my $s2 = Audio::Scan->scan( _f('old2.ogg') );
    is($s2->{tags}->{ALBUM}, 'AutoTests', 'Old encoded album tag ok');
    is($s2->{info}->{samplerate}, 12000, 'Old encoded rate ok');
}

# SC bugs
{
    my $s = Audio::Scan->scan( _f('bug1155-1.ogg') );

    my $info = $s->{info};

    is($info->{bitrate_nominal}, 206723, 'Bug1155 nominal bitrate ok');
    is($info->{bitrate_average}, 1092, 'Bug1155 avg bitrate ok');
    is($info->{song_length_ms}, 187146, 'Bug1155 duration ok');
}

{
    my $s = Audio::Scan->scan( _f('bug1155-2.ogg') );

    my $info = $s->{info};

    is($info->{bitrate_average}, 7414, 'Bug1155-2 bitrate ok');
    is($info->{song_length_ms}, 5864, 'Bug1155-2 duration ok');
}

{
    my $s = Audio::Scan->scan( _f('bug803.ogg') );

    my $info = $s->{info};
    
    is($info->{bitrate_average}, 785, 'Bug803 bitrate ok');
    is($info->{song_length_ms}, 219104, 'Bug803 song length ok');
}

{
    my $s = Audio::Scan->scan( _f('bug905.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is($info->{bitrate_average}, 681, 'Bug905 bitrate ok');
    is($info->{song_length_ms}, 223484, 'Bug905 song length ok');
    is($tags->{DATE}, '08-05-1998', 'Bug905 date ok');
}

# Scan via a filehandle
{
    open my $fh, '<', _f('test.ogg');
    
    my $s = Audio::Scan->scan_fh( ogg => $fh );
    
    my $info = $s->{info};
    my $tags = $s->{tags};

    is($tags->{ARTIST}, 'Test Artist', 'ASCII Tag ok via filehandle');
    is($tags->{YEAR}, 2009, 'Year Tag ok via filehandle');

    is($info->{bitrate_average}, 9887, 'Bitrate ok via filehandle');
    
    close $fh;
}

# Find frame offset
{
    my $offset = Audio::Scan->find_frame( _f('bug1155-1.ogg'), 17005 );
    
    is( $offset, 21351, 'Find frame ok' );
}

{
    open my $fh, '<', _f('bug1155-1.ogg');
    
    my $offset = Audio::Scan->find_frame_fh( ogg => $fh, 16600 );
    
    is( $offset, 17004, 'Find frame via filehandle ok' );
    
    close $fh;
}

# Bug 12615, aoTuV-encoded file uncovered bug in offset calculation
{
    my $s = Audio::Scan->scan( _f('bug12615-aotuv.ogg') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 3970, 'Bug 12615 aoTuV offset ok' );
    
    like( $tags->{VENDOR}, qr/aoTuV/, 'Bug 12615 aoTuV tags ok' );
}

# Test file with page segments > 128
{
    my $s = Audio::Scan->scan( _f('large-page-segments.ogg') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 41740, 'Large page segments audio offset ok' );
    is( $tags->{ARTIST}, 'Led Zeppelin', 'Large page segments comments ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'ogg', shift );
}

use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 33;

use Audio::Scan;
use Encode;

# Basics
{
    my $s = Audio::Scan->scan( _f('test.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};
    my $utf8 = Encode::decode_utf8('シチヅヲ');

    is($tags->{ARTIST}, 'Test Artist', 'ASCII Tag ok');
    is($tags->{YEAR}, 2009, 'Year Tag ok');
    is($tags->{PERFORMER}, $utf8, 'PERFORMER (UTF8) Tag ok');
    ok($tags->{VENDOR} =~ /Xiph/, 'Vendor ok');

    is($info->{bitrate_average}, 12141, 'Bitrate ok');
    is($info->{channels}, 2, 'Channels ok');
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

    is($info->{bitrate_average}, 206723, 'Bug1155 bitrate ok');
}

{
    my $s = Audio::Scan->scan( _f('bug1155-2.ogg') );

    my $info = $s->{info};

    is($info->{bitrate_average}, 8696, 'Bug1155 bitrate ok');
}

{
    my $s = Audio::Scan->scan( _f('bug803.ogg') );

    my $info = $s->{info};
    
    is($info->{song_length_ms}, 219693, 'Bug803 song length ok');
}

{
    my $s = Audio::Scan->scan( _f('bug905.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is($info->{song_length_ms}, 225986, 'Bug905 song length ok' );
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

    is($info->{bitrate_average}, 12141, 'Bitrate ok via filehandle');
    
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

sub _f {
    return catfile( $FindBin::Bin, 'ogg', shift );
}

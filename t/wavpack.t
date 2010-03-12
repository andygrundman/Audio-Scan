use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 37;

use Audio::Scan;

# Silence file with APEv2 tag
{
    my $s = Audio::Scan->scan( _f('silence-44-s.wv') );
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{ape_version}, 'APEv2', 'APE version ok' );
    is( $info->{audio_offset}, 0, 'audio_offset ok' );
    is( $info->{bitrate}, 76323, 'bitrate ok' );
    is( $info->{bits_per_sample}, 16, 'bits_per_sample ok' );
    is( $info->{channels}, 2, 'channels ok' );
    is( $info->{encoder_version}, 0x403, 'version ok' );
    is( $info->{file_size}, 35147, 'file_size ok' );
    is( $info->{lossless}, 1, 'lossless ok' );
    is( $info->{samplerate}, 44100, 'samplerate ok' );
    is( $info->{song_length_ms}, 3684, 'song_length_ms ok' );
    is( $info->{total_samples}, 162496, 'total_samples ok' );

    is( $tags->{DATE}, 2004, 'DATE ok' );
    is( $tags->{GENRE}, 'Silence', 'GENRE ok' );
    is( $tags->{TITLE}, 'Silence', 'TITLE ok' );    
}

# Self-extracting file (why?!)
{
    my $s = Audio::Scan->scan_info( _f('win-executable.wv') );
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 30720, 'EXE audio_offset ok' );
    is( $info->{song_length_ms}, 29507, 'EXE song_length_ms ok' );
}

# Hybrid (lossy) file
{
    my $s = Audio::Scan->scan_info( _f('hybrid-24kbps.wv') );
    my $info = $s->{info};
    
    is( $info->{channels}, 1, 'hybrid-24 channels ok' );
    is( $info->{hybrid}, 1, 'hybrid-24 hybrid flag ok' );
    is( $info->{samplerate}, 8000, 'hybrid-24 samplerate ok' );
    is( $info->{song_length_ms}, 25000, 'hybrid-24 song_length_ms ok' );
}

# 24-bit file
{
    my $s = Audio::Scan->scan_info( _f('24-bit.wv') );
    my $info = $s->{info};
    
    is( $info->{bits_per_sample}, 24, '24-bit bits_per_sample ok' );
    is( $info->{channels}, 2, '24-bit channels ok' );
    is( $info->{samplerate}, 44100, '24-bit samplerate ok' );
}

# File with initial block containing 0 block_samples (bug 8601)
{
    my $s = Audio::Scan->scan_info( _f('zero-first-block.wv') );
    my $info = $s->{info};
    
    is( $info->{bits_per_sample}, 16, 'Zero first block bits_per_sample ok' );
    is( $info->{channels}, 2, 'Zero first block channels ok' );
    is( $info->{samplerate}, 44100, 'Zero first block samplerate ok' );
    is( $info->{song_length_ms}, 36506, 'Zero first block song_length_ms ok' );
    is( $info->{total_samples}, 1609944, 'Zero first block total_samples ok' );
}

# v3 file
{
    my $s = Audio::Scan->scan_info( _f('v3.wv') );
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 0, 'v3 audio_offset ok' );
    is( $info->{bitrate}, 4, 'v3 bitrate ok' );
    is( $info->{bits_per_sample}, 16, 'v3 bits_per_sample ok' );
    is( $info->{channels}, 2, 'v3 channels ok' );
    is( $info->{encoder_version}, 3, 'v3 encoder_version ok' );
    is( $info->{file_size}, 176, 'v3 file_size ok' );
    is( $info->{samplerate}, 44100, 'v3 samplerate ok' );
    is( $info->{song_length_ms}, 329280, 'v3 song_length_ms ok' );
    is( $info->{total_samples}, 14521248, 'v3 total_samples ok' );
}

sub _f {    
    return catfile( $FindBin::Bin, 'wavpack', shift );
}

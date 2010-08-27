use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 31;

use Audio::Scan;

# Mono ADTS file
{
    my $s = Audio::Scan->scan( _f('mono.aac') );
    
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 0, 'Audio offset ok' );
    is( $info->{audio_size}, 2053, 'Audio size ok' );
    is( $info->{bitrate}, 35000, 'Bitrate ok' );
    is( $info->{channels}, 1, 'Channels ok' );
    is( $info->{file_size}, 2053, 'File size ok' );
    is( $info->{profile}, 'LC', 'Profile ok' );
    is( $info->{samplerate}, 44100, 'Samplerate ok' );
    is( $info->{song_length_ms}, 464, 'Duration ok' );
}

# Stereo ADTS file
{
    my $s = Audio::Scan->scan( _f('stereo.aac') );
    
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 0, 'Stereo ADTS audio offset ok' );
    is( $info->{bitrate}, 58000, 'Stereo ADTS bitrate ok' );
    is( $info->{channels}, 2, 'Stereo ADTS channels ok' );
    is( $info->{profile}, 'LC', 'Stereo ADTS profile ok' );
    is( $info->{samplerate}, 44100, 'Stereo ADTS samplerate ok' );
    is( $info->{song_length_ms}, 1393, 'Stereo ADTS duration ok' );
}

# ADTS with ID3v2 tags
{
    my $s = Audio::Scan->scan( _f('id3v2.aac'), { md5_size => 4096 } );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 2182, 'ID3v2 audio offset ok' );
    is( $info->{audio_size}, 2602, 'ID3v2 audio_size ok' );
    is( $info->{audio_md5}, 'f84210edefebcd92792fd1b3d21860d5', 'ID3v2 audio_md5 ok' );
    is( $info->{bitrate}, 128000, 'ID3v2 bitrate ok' );
    is( $info->{channels}, 2, 'ID3v2 channels ok' );
    is( $info->{profile}, 'LC', 'ID3v2 profile ok' );
    is( $info->{samplerate}, 44100, 'ID3v2 samplerate ok' );
    is( $info->{song_length_ms}, 162, 'ID3v2 duration ok' );
    is( $info->{id3_version}, 'ID3v2.3.0', 'ID3v2 version ok' );
    
    is( $tags->{TPE1}, 'Calibration Level', 'ID3v2 TPE1 ok' );
    is( $tags->{TENC}, 'ORBAN', 'ID3v2 TENC ok' );
    is( $tags->{TIT2}, '1kHz -20dBfs', 'ID3v2 TIT2 ok' );    
}

# ADTS with leading junk (from a radio stream)
{
    my $s = Audio::Scan->scan( _f('leading-junk.aac') );
    
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 638, 'Leading junk offset ok' );
    is( $info->{bitrate}, 64000, 'Leading junk bitrate ok' );
    is( $info->{channels}, 2, 'Leading junk channels ok' );
    is( $info->{profile}, 'LC', 'Leading junk profile ok' );
    is( $info->{samplerate}, 44100, 'Leading junk samplerate ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'aac', shift );
}

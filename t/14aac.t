use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 12;

use Audio::Scan;

# Mono ADTS file
{
    my $s = Audio::Scan->scan( _f('mono.aac') );
    
    my $info  = $s->{info};
    
    is( $info->{audio_offset}, 0, 'Audio offset ok' );
    is( $info->{bitrate}, 35000, 'Bitrate ok' );
    is( $info->{channels}, 1, 'Channels ok' );
    is( $info->{profile}, 'LC', 'Profile ok' );
    is( $info->{samplerate}, 44100, 'Samplerate ok' );
    is( $info->{song_length_ms}, 464, 'Duration ok' );
}

# Stereo ADTS file
{
    my $s = Audio::Scan->scan( _f('stereo.aac') );
    
    my $info  = $s->{info};
    
    is( $info->{audio_offset}, 0, 'Audio offset ok' );
    is( $info->{bitrate}, 58000, 'Bitrate ok' );
    is( $info->{channels}, 2, 'Channels ok' );
    is( $info->{profile}, 'LC', 'Profile ok' );
    is( $info->{samplerate}, 44100, 'Samplerate ok' );
    is( $info->{song_length_ms}, 1393, 'Duration ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'aac', shift );
}

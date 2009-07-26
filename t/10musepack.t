use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 14;

use Audio::Scan;
use Encode;

# Musepack file with APEv2 tags
{
    my $s = Audio::Scan->scan( _f('apev2.mpc') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 24, 'Audio offset ok' );
    is( $info->{bitrate}, 692, 'Bitrate ok' );
    is( $info->{file_size}, 51782, 'File size ok' );
    is( $info->{profile}, 'Extreme (q=6)', 'Profile ok' );
    is( $info->{samplerate}, 44100, 'Sample rate ok' );
    is( $info->{song_length_ms}, 598138, 'Song length ok' );
    is( $info->{channels}, '2', 'Channels version ok' );
    is( $info->{gapless}, 1, 'Gapless ok' );
    is( $info->{track_gain}, '-2.89 dB', 'Track gain ok' );
    is( $info->{album_gain}, '-5.63 dB', 'Album gain ok' );

    is( $tags->{ALBUM}, 'Special Cases', 'Album ok' );
    is( $tags->{ARTIST}, 'Massive Attack', 'Artist ok' );
    is( $tags->{TITLE}, 'Special Cases [Akufen remix]', 'Title ok' );
    is( $tags->{TRACK}, 2, 'Track ok' );
}

# XXX: SV8 test file

sub _f {
    return catfile( $FindBin::Bin, 'musepack', shift );
}

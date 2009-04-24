use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 7;

use Audio::Scan;
use Encode;

# Musepack file with ID3 tags
{
    my $s = Audio::Scan->scan( _f('apev2.mpc') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{samplerate}, 44100, 'Sample rate ok' );
    is( $info->{song_length_ms}, 598000, 'Song length ok' );
    is( $info->{channels}, '2', 'Channels version ok' );

    is( $tags->{ALBUM}, 'Special Cases', 'Album ok' );
    is( $tags->{ARTIST}, 'Massive Attack', 'Artist ok' );
    is( $tags->{TITLE}, 'Special Cases [Akufen remix]', 'Title ok' );
    is( $tags->{TRACK}, 2, 'Track ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'musepack', shift );
}

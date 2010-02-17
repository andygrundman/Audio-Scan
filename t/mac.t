use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 17;

use Audio::Scan;

# Monkey's Audio files with APEv2 tags
{
    my $s = Audio::Scan->scan( _f('apev2.ape') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{ape_version}, 'APEv2', 'APE version ok' );
    is( $info->{bitrate}, 7741, 'Bitrate ok' );
    is( $info->{samplerate}, 44100, 'Sample rate ok' );
    is( $info->{song_length_ms}, 100800, 'Song length ok' );
    is( $info->{channels}, 2, 'Channels version ok' );
    is( $info->{file_size}, 97547, 'File size ok' );
    is( $info->{version}, 3.99, 'Encoder ok' );
    is( $info->{compression}, "Fast (poor)", 'Compression ok' );

    is( $tags->{ALBUM}, 'Surfer Girl', 'Album ok' );
    is( $tags->{ARTIST}, 'Beach Boys', 'Artist ok' );
    is( $tags->{TITLE}, 'Little Deuce Coupe', 'Title ok' );
    is( $tags->{TRACK}, 6, 'Track ok' );
    is( $tags->{YEAR}, 1990, 'Year ok' );
    is( $tags->{GENRE}, "Rock", 'Genre ok' );
}

# APEv1 tags
{
    my $s = Audio::Scan->scan( _f('apev1.ape') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{ape_version}, 'APEv1', 'APEv1 version ok' );
    
    is( $tags->{GENRE}, "\xFF", 'APEv1 genre ok' );
    is( $tags->{YEAR}, "2004", 'APEv1 year ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'mac', shift );
}

use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 22;

use Audio::Scan;

# AIFF file with ID3 tags (tagged by iTunes)
{
    my $s = Audio::Scan->scan( _f('aiff-id3.aif') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 46, 'Audio offset ok' );
    is( $info->{bitrate}, 1411200, 'Bitrate ok' );
    is( $info->{bits_per_sample}, 16, 'Bits/sample ok' );
    is( $info->{block_align}, 4, 'Block align ok' );
    is( $info->{channels}, 2, 'Channels ok' );
    is( $info->{file_size}, 4170, 'File size ok' );
    is( $info->{samplerate}, 44100, 'Sample rate ok' );
    is( $info->{song_length_ms}, 10, 'Song length ok' );
    is( $info->{id3_version}, 'ID3v2.2.0', 'ID3 version ok' );
    
    is( $tags->{TALB}, '...And So It Goes', 'TALB ok' );
    is( $tags->{TCON}, 'Electronica/Dance', 'TCON ok' );
    is( $tags->{TDRC}, 2008, 'TDRC ok' );
    is( $tags->{TIT2}, 'Dark Roads', 'TIT2 ok' );
    is( $tags->{TPE1}, 'Kaya Project', 'TPE1 ok' );
}

# 32-bit AIFF with PEAK info
{
    my $s = Audio::Scan->scan( _f('aiff32.aiff') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 2822400, '32-bit AIFF bitrate ok' );
    is( $info->{bits_per_sample}, 32, '32-bit AIFF bits/sample ok' );
    is( $info->{block_align}, 8, '32-bit AIFF block align ok' );
    is( ref $info->{peak}, 'ARRAY', '32-bit AIFF PEAK ok' );
    is( $info->{peak}->[0]->{position}, 284, '32-bit AIFF Peak 1 ok' );
    is( $info->{peak}->[1]->{position}, 47, '32-bit AIFF Peak 2 ok' );
    like( $info->{peak}->[0]->{value}, qr/^0.477/, '32-bit AIFF Peak 1 value ok' );
    like( $info->{peak}->[1]->{value}, qr/^0.476/, '32-bit AIFF Peak 2 value ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'aiff', shift );
}
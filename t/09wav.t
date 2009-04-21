use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 21;

use Audio::Scan;
use Encode;

# WAV file with ID3 tags
{
    my $s = Audio::Scan->scan( _f('id3.wav') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 44, 'Audio offset ok' );
    is( $info->{bitrate}, 1411200, 'Bitrate ok' );
    is( $info->{bits_per_sample}, 16, 'Bits/sample ok' );
    is( $info->{block_align}, 4, 'Block align ok' );
    is( $info->{channels}, 2, 'Channels ok' );
    is( $info->{file_size}, 4240, 'File size ok' );
    is( $info->{format}, 1, 'Format ok' );
    is( $info->{samplerate}, 44100, 'Sample rate ok' );
    is( $info->{song_length_ms}, 10, 'Song length ok' );
    
    # XXX: ID3 tags
}

# 32-bit WAV with PEAK info
{
    my $s = Audio::Scan->scan( _f('wav32.wav') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 2822400, '32-bit WAV bitrate ok' );
    is( $info->{bits_per_sample}, 32, '32-bit WAV bits/sample ok' );
    is( $info->{block_align}, 8, '32-bit WAV block align ok' );
    is( ref $info->{peak}, 'ARRAY', '32-bit WAV PEAK ok' );
    is( $info->{peak}->[0]->{position}, 284, '32-bit WAV Peak 1 ok' );
    is( $info->{peak}->[1]->{position}, 47, '32-bit WAV Peak 2 ok' );
    like( $info->{peak}->[0]->{value}, qr/^0.477/, '32-bit WAV Peak 1 value ok' );
    like( $info->{peak}->[1]->{value}, qr/^0.476/, '32-bit WAV Peak 2 value ok' );
}

# MP3 in WAV
{
    my $s = Audio::Scan->scan( _f('8kmp38.wav') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 8000, 'MP3 WAV bitrate ok' );
    is( $info->{format}, 85, 'MP3 WAV format ok' );
    is( $info->{samplerate}, 8000, 'MP3 WAV samplerate ok' );
    is( $info->{song_length_ms}, 13811, 'MP3 WAV length ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'wav', shift );
}
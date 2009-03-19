use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 20;

use Audio::Scan;

## Test file info on non-tagged files

# MPEG1, Layer 3, 32k / 32kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 32, 'MPEG1, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 32000, 'MPEG1, Layer 3 samplerate ok' );
}

# MPEG2, Layer 3, 8k / 22.05kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp2l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 8, 'MPEG2, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 22050, 'MPEG2, Layer 3 samplerate ok' );
}

# MPEG2.5, Layer 3, 8k / 8kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp2.5l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 8, 'MPEG2.5, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 8000, 'MPEG2.5, Layer 3 samplerate ok' );
}

# MPEG1, Layer 3, ~40k / 32kHz VBR
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3-vbr.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 40, 'MPEG1, Layer 3 VBR bitrate ok' );
    is( $info->{samplerate}, 32000, 'MPEG1, Layer 3 VBR samplerate ok' );
    
    # Xing header
    is( $info->{xing_bytes}, $info->{audio_size}, 'Xing bytes field ok' );
    is( $info->{xing_frames}, 30, 'Xing frames field ok' );
    is( $info->{xing_quality}, 57, 'Xing quality field ok' );

	# LAME header
	is( $info->{lame_encoder_delay}, 576, 'LAME encoder delay ok' );
	is( $info->{lame_encoder_padding}, 1191, 'LAME encoder padding ok' );
	is( $info->{lame_vbr_method}, 'Average Bitrate', 'LAME VBR method ok' );
	is( $info->{vbr}, 1, 'LAME VBR flag ok' );
	is( $info->{lame_preset}, 'ABR 40', 'LAME preset ok' );
	is( $info->{lame_replay_gain_radio}, '-4.6 dB', 'LAME ReplayGain ok' );
    
    # XXX: LAME tag tests
}

# MPEG2, Layer 3, 320k / 44.1kHz CBR with LAME Info tag
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3-cbr320.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 320, 'Info tag bitrate ok' );
    is( $info->{samplerate}, 44100, 'Info tag samplerate ok' );
    
    is( $info->{lame_encoder_version}, 'LAME3.97 ', 'Info tag LAME version ok' );
}

sub _f {    
    return catfile( $FindBin::Bin, 'mp3', shift );
}
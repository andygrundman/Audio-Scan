use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 105;

use Audio::Scan;

## Test file info on non-tagged files

# MPEG1, Layer 2, 192k / 44.1kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l2.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{layer}, 2, 'MPEG1, Layer 2 ok' );
    is( $info->{bitrate}, 192, 'MPEG1, Layer 2 bitrate ok' );
    is( $info->{samplerate}, 44100, 'MPEG1, Layer 2 samplerate ok' );
}

# MPEG2, Layer 2, 96k / 16khz mono
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l2-mono.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{layer}, 2, 'MPEG2, Layer 2 ok' );
    is( $info->{bitrate}, 96, 'MPEG2, Layer 2 bitrate ok' );
    is( $info->{samplerate}, 16000, 'MPEG2, Layer 2 samplerate ok' );
    is( $info->{stereo}, 0, 'MPEG2, Layer 2 mono ok' );
}

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
}

# MPEG2, Layer 3, 320k / 44.1kHz CBR with LAME Info tag
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3-cbr320.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 320, 'CBR file bitrate ok' );
    is( $info->{samplerate}, 44100, 'CBR file samplerate ok' );
    is( $info->{vbr}, undef, 'CBR file does not have VBR flag' );
    is( $info->{lame_encoder_version}, 'LAME3.97 ', 'CBR file LAME Info tag version ok' );
}

# Non-Xing/LAME VBR file to test average bitrate calculation
{
	my $s = Audio::Scan->scan( _f('no-tags-no-xing-vbr.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 229, 'Non-Xing VBR average bitrate calc ok' );
}

# File with no audio frames, test is rejected properly
{
    # Hide stderr
    no warnings;
    open OLD_STDERR, '>&', STDERR;
    close STDERR;
    
    my $s = Audio::Scan->scan_info( _f('v2.3-no-audio-frames.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, undef, 'File with no audio frames ok' );
    
    # Restore stderr
    open STDERR, '>&', OLD_STDERR;
}

# MPEG1 Xing mono file to test xing_offset works properly
{
    my $s = Audio::Scan->scan_info( _f('no-tags-mp1l3-mono.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{stereo}, 0, 'MPEG1 Xing mono file ok' );
    is( $info->{xing_frames}, 42, 'MPEG1 Xing mono frames ok' );    
}

# MPEG2 Xing mono file to test xing_offset
{
    my $s = Audio::Scan->scan_info( _f('no-tags-mp2l3-mono.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{stereo}, 0, 'MPEG2 Xing mono file ok' );
    is( $info->{xing_frames}, 30, 'MPEG2 Xing mono frames ok' );    
}

# MPEG2 Xing stereo file to test xing_offset
{
    my $s = Audio::Scan->scan_info( _f('no-tags-mp2l3-vbr.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{stereo}, 1, 'MPEG2 Xing stereo file ok' );
    is( $info->{xing_frames}, 30, 'MPEG2 Xing stereo frames ok' );
    is( $info->{vbr}, 1, 'MPEG2 Xing vbr ok' );
}

# VBRI mono file
{
    my $s = Audio::Scan->scan_info( _f('no-tags-vbri-mono.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{stereo}, 0, 'VBRI mono file ok' );
    
    # XXX: VBRI mono files do not seem to put the VBRI tag at the correct place
}

# VBRI stereo file
{
    my $s = Audio::Scan->scan_info( _f('no-tags-vbri-stereo.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{vbri_delay}, 2353, 'VBRI delay ok' );
    is( $info->{bitrate}, 61, 'VBRI bitrate ok' );
    is( $info->{song_length_ms}, 1071, 'VBRI duration ok' );
}

### ID3 tag tests

# ID3v1
{
    my $s = Audio::Scan->scan( _f('v1.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v1', 'ID3v1 version ok' );
    is( $tags->{TPE1}, 'Artist Name', 'ID3v1 artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v1 title ok' );
    is( $tags->{TALB}, 'Album Name', 'ID3v1 album ok' );
    is( $tags->{TDRC}, 2009, 'ID3v1 year ok' );
    is( $tags->{TCON}, 'Ambient', 'ID3v1 genre ok' );
    is( $tags->{COMM}->[3], 'This is a comment', 'ID3v1 comment ok' );
}

# ID3v1.1 (adds track number)
{
    my $s = Audio::Scan->scan( _f('v1.1.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v1.1', 'ID3v1.1 version ok' );
    is( $tags->{TPE1}, 'Artist Name', 'ID3v1.1 artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v1.1 title ok' );
    is( $tags->{TALB}, 'Album Name', 'ID3v1.1 album ok' );
    is( $tags->{TDRC}, 2009, 'ID3v1.1 year ok' );
    is( $tags->{TCON}, 'Ambient', 'ID3v1.1 genre ok' );
    is( $tags->{COMM}->[3], 'This is a comment', 'ID3v1.1 comment ok' );
    is( $tags->{TRCK}, 16, 'ID3v1.1 track number ok' );
}

# ID3v1 with ISO-8859-1 encoding
{
    my $s = Audio::Scan->scan_tags( _f('v1-iso-8859-1.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, 'pâté', 'ID3v1 ISO-8859-1 artist ok' );
    
    # Make sure it's been converted to UTF-8
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v1 ISO-8859-1 is valid UTF-8' );
    is( unpack( 'H*', $tags->{TPE1} ), '70c3a274c3a9', 'ID3v1 ISO-8859-1 converted to UTF-8 ok' );
}

# ID3v2.2 (libid3tag converts them to v2.4-equivalent tags)
{
    my $s = Audio::Scan->scan( _f('v2.2.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.2.0', 'ID3v2.2 version ok' );
    is( $tags->{TPE1}, 'Pudge', 'ID3v2.2 artist ok' );
    is( $tags->{TIT2}, 'Test v2.2.0', 'ID3v2.2 title ok' );
    is( $tags->{TDRC}, 1998, 'ID3v2.2 year ok' );
    is( $tags->{TCON}, 'Sound Clip', 'ID3v2.2 genre ok' );
    is( $tags->{COMM}->[1], 'eng', 'ID3v2.2 comment language ok' );
    is( $tags->{COMM}->[3], 'All Rights Reserved', 'ID3v2.2 comment ok' );
    is( $tags->{TRCK}, 2, 'ID3v2.2 track number ok' );
}

# ID3v2.2 with multiple comment tags
{
	my $s = Audio::Scan->scan_tags( _f('v2.2-multiple-comm.mp3') );
    
    my $tags = $s->{tags};

	is( scalar @{ $tags->{COMM} }, 4, 'ID3v2.2 4 comment tags ok' );
	is( $tags->{COMM}->[1]->[2], 'iTunNORM', 'ID3v2.2 iTunNORM ok' );
	is( $tags->{COMM}->[2]->[2], 'iTunes_CDDB_1', 'ID3v2.2 iTunes_CDDB_1 ok' );
	is( $tags->{COMM}->[3]->[2], 'iTunes_CDDB_TrackNumber', 'ID3v2.2 iTunes_CDDB_TrackNumber ok' );
}

# ID3v2.3
{
    my $s = Audio::Scan->scan( _f('v2.3.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.3.0', 'ID3v2.3 version ok' );
    is( $tags->{TPE1}, 'Artist Name', 'ID3v2.3 artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.3 title ok' );
    is( $tags->{TALB}, 'Album Name', 'ID3v2.3 album ok' );
    is( $tags->{TCON}, 'Ambient', 'ID3v2.3 genre ok' );
    is( $tags->{TRCK}, '02/10', 'ID3v2.3 track number ok' );
    is( $tags->{'Tagging time'}, '2009-03-16T17:58:23', 'ID3v2.3 TXXX ok' ); # TXXX tag
}

# ID3v2.3 ISO-8859-1
{
    my $s = Audio::Scan->scan( _f('v2.3-iso-8859-1.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.3.0', 'ID3v2.3 version ok' );
    is( $tags->{TPE1}, 'Ester Koèièková a Lubomír Nohavica', 'ID3v2.3 ISO-8859-1 artist ok' );
    is( $tags->{TALB}, 'Ester Koèièková a Lubomír Nohavica s klavírem', 'ID3v2.3 ISO-8859-1 album ok' );
    is( $tags->{TIT2}, 'Tøem sestrám', 'ID3v2.3 ISO-8859-1 title ok' );
    
    # Make sure it's been converted to UTF-8
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 ISO-8859-1 is valid UTF-8' );
    is( unpack( 'H*', $tags->{TPE1} ), '4573746572204b6fc3a869c3a86b6f76c3a12061204c75626f6dc3ad72204e6f686176696361', 'ID3v2.3 ISO-8859-1 converted to UTF-8 ok' );
}

# ID3v2.3 UTF-16BE
{
    my $s = Audio::Scan->scan_tags( _f('v2.3-utf16be.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, 'pâté', 'ID3v1 UTF-16BE artist ok' );
    
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 UTF-16BE is valid UTF-8' );
    is( unpack( 'H*', $tags->{TPE1} ), '70c3a274c3a9', 'ID3v2.3 UTF-16BE converted to UTF-8 ok' );
}

# ID3v2.3 UTF-16LE
{
    my $s = Audio::Scan->scan_tags( _f('v2.3-utf16le.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, 'pâté', 'ID3v1 UTF-16LE artist ok' );
    
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 UTF-16LE is valid UTF-8' );
    is( unpack( 'H*', $tags->{TPE1} ), '70c3a274c3a9', 'ID3v2.3 UTF-16LE converted to UTF-8 ok' );
}

# ID3v2.4
{
    my $s = Audio::Scan->scan( _f('v2.4.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.4.0', 'ID3v2.4 version ok' );
    is( $tags->{TPE1}, 'Artist Name', 'ID3v2.4 artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 title ok' );
    is( $tags->{TALB}, 'Album Name', 'ID3v2.4 album ok' );
    is( $tags->{TCON}, 'Ambient', 'ID3v2.4 genre ok' );
    is( $tags->{TRCK}, '02/10', 'ID3v2.4 track number ok' );
    is( $tags->{PCNT}, 256, 'ID3v2.4 playcount field ok' );
    is( $tags->{POPM}->[0]->[0], 'foo@foo.com', 'ID3v2.4 POPM #1 ok' );
    is( $tags->{POPM}->[1]->[2], 7, 'ID3v2.4 POPM #2 ok' );
    is( $tags->{TBPM}, 120, 'ID3v2.4 BPM field ok' );
    is( $tags->{UFID}->[0], 'foo@foo.com', 'ID3v2.4 UFID owner id ok' );
    is( $tags->{UFID}->[1], 'da39a3ee5e6b4b0d3255bfef95601890afd80709', 'ID3v2.4 UFID ok' );
    is( $tags->{'User Frame'}, 'User Data', 'ID3v2.4 TXXX ok' );
    is( $tags->{WCOM}, 'http://www.google.com', 'ID3v2.4 WCOM ok' );
    is( $tags->{'User URL'}, 'http://www.google.com', 'ID3v2.4 WXXX ok' );
    
    # XXX: 2 WOAR frames
} 

sub _f {    
    return catfile( $FindBin::Bin, 'mp3', shift );
}
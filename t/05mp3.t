use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 240;

use Audio::Scan;
use Encode;

my $pate = Encode::decode_utf8("pâté");

## Test file info on non-tagged files

# MPEG1, Layer 2, 192k / 44.1kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l2.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{layer}, 2, 'MPEG1, Layer 2 ok' );
    is( $info->{bitrate}, 192000, 'MPEG1, Layer 2 bitrate ok' );
    is( $info->{samplerate}, 44100, 'MPEG1, Layer 2 samplerate ok' );
}

# MPEG2, Layer 2, 96k / 16khz mono
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l2-mono.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{layer}, 2, 'MPEG2, Layer 2 ok' );
    is( $info->{bitrate}, 96000, 'MPEG2, Layer 2 bitrate ok' );
    is( $info->{samplerate}, 16000, 'MPEG2, Layer 2 samplerate ok' );
    is( $info->{stereo}, 0, 'MPEG2, Layer 2 mono ok' );
}

# MPEG1, Layer 3, 32k / 32kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 32000, 'MPEG1, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 32000, 'MPEG1, Layer 3 samplerate ok' );
}

# MPEG2, Layer 3, 8k / 22.05kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp2l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 8000, 'MPEG2, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 22050, 'MPEG2, Layer 3 samplerate ok' );
}

# MPEG2.5, Layer 3, 8k / 8kHz
{
    my $s = Audio::Scan->scan( _f('no-tags-mp2.5l3.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 8000, 'MPEG2.5, Layer 3 bitrate ok' );
    is( $info->{samplerate}, 8000, 'MPEG2.5, Layer 3 samplerate ok' );
}

# MPEG1, Layer 3, ~40k / 32kHz VBR
{
    my $s = Audio::Scan->scan( _f('no-tags-mp1l3-vbr.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 40000, 'MPEG1, Layer 3 VBR bitrate ok' );
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
    
    is( $info->{bitrate}, 320000, 'CBR file bitrate ok' );
    is( $info->{samplerate}, 44100, 'CBR file samplerate ok' );
    is( $info->{vbr}, undef, 'CBR file does not have VBR flag' );
    is( $info->{lame_encoder_version}, 'LAME3.97 ', 'CBR file LAME Info tag version ok' );
}

# Non-Xing/LAME VBR file to test average bitrate calculation
{
	my $s = Audio::Scan->scan( _f('no-tags-no-xing-vbr.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{bitrate}, 229000, 'Non-Xing VBR average bitrate calc ok' );
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
    is( $info->{bitrate}, 61000, 'VBRI bitrate ok' );
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
    
    is( $tags->{TPE1}, $pate, 'ID3v1 ISO-8859-1 artist ok' );
    
    # Make sure it's been converted to UTF-8
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v1 ISO-8859-1 is valid UTF-8' );
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

# ID3v2.2 from iTunes 8.1, full of non-standard frames
{
    my $s = Audio::Scan->scan( _f('v2.2-itunes81.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $tags->{TENC}, 'iTunes 8.1', 'ID3v2.2 from iTunes 8.1 ok' );
    is( $tags->{USLT}->[3], 'This is the lyrics field from iTunes.', 'iTunes 8.1 USLT ok' );
    is( $tags->{YTCP}, 1, 'iTunes 8.1 TCP ok' );
    is( $tags->{YTS2}, 'Album Artist Sort', 'iTunes 8.1 TS2 ok' );
    is( $tags->{YTSA}, 'Album Sort', 'iTunes 8.1 TSA ok' );
    is( $tags->{YTSC}, 'Composer Sort', 'iTunes 8.1 TSC ok' );
    is( $tags->{YTSP}, 'Artist Name Sort', 'iTunes 8.1 TSP ok' );
    is( $tags->{YTST}, 'Track Title Sort', 'iTunes 8.1 TST ok' );
    is( ref $tags->{YRVA}, 'ARRAY', 'iTunes 8.1 RVA ok' );
    is( $tags->{YRVA}->[0], '-2.119539 dB', 'iTunes 8.1 RVA right ok' );
    is( $tags->{YRVA}->[1], '0.000000', 'iTunes 8.1 RVA right peak ok' );
    is( $tags->{YRVA}->[2], '-2.119539 dB', 'iTunes 8.1 RVA left ok' );
    is( $tags->{YRVA}->[3], '0.000000', 'iTunes 8.1 RVA left peak ok' );
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
    is( $tags->{'TAGGING TIME'}, '2009-03-16T17:58:23', 'ID3v2.3 TXXX ok' ); # TXXX tag
    
    # Make sure TDRC is present and TYER has been removed
    is( $tags->{TDRC}, 2009, 'ID3v2.3 date ok' );
    is( $tags->{TYER}, undef, 'ID3v2.3 TYER removed ok' );
}

# ID3v2.3 ISO-8859-1
{
    my $s = Audio::Scan->scan( _f('v2.3-iso-8859-1.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    my $a = Encode::decode_utf8('Ester Koèièková a Lubomír Nohavica');
    my $b = Encode::decode_utf8('Ester Koèièková a Lubomír Nohavica s klavírem');
    my $c = Encode::decode_utf8('Tøem sestrám');
    
    is( $info->{id3_version}, 'ID3v2.3.0', 'ID3v2.3 version ok' );
    is( $tags->{TPE1}, $a, 'ID3v2.3 ISO-8859-1 artist ok' );
    is( $tags->{TALB}, $b, 'ID3v2.3 ISO-8859-1 album ok' );
    is( $tags->{TIT2}, $c, 'ID3v2.3 ISO-8859-1 title ok' );
    
    # Make sure it's been converted to UTF-8
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 ISO-8859-1 is valid UTF-8' );
}

# ID3v2.3 UTF-16BE
{
    my $s = Audio::Scan->scan_tags( _f('v2.3-utf16be.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, $pate, 'ID3v2.3 UTF-16BE artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.3 UTF-16BE title ok' );
    
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 UTF-16BE is valid UTF-8' );
}

# ID3v2.3 UTF-16LE
{
    my $s = Audio::Scan->scan_tags( _f('v2.3-utf16le.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, $pate, 'ID3v2.3 UTF-16LE artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.3 UTF-16LE title ok' );
    
    is( utf8::valid( $tags->{TPE1} ), 1, 'ID3v2.3 UTF-16LE is valid UTF-8' );
}

# ID3v2.3 mp3HD, make sure we ignore XHD3 frame properly
{
    my $s = Audio::Scan->scan( _f('v2.3-mp3HD.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 57956, 'mp3HD offset ok' );
    is( $tags->{TIT2}, 'mp3HD is evil', 'mp3HD tags ok' );
    is( $tags->{XHD3}, undef, 'mp3HD XHD3 frame ignored' );
}

# ID3v2.3 with empty WXXX tag
{
    my $s = Audio::Scan->scan( _f('v2.3-empty-wxxx.mp3') );
    
    my $tags = $s->{tags};
    
    is( !exists( $tags->{''} ), 1, 'ID3v2.3 empty WXXX ok' );
}

# ID3v2.3 with empty TCON tag
{
    my $s = Audio::Scan->scan( _f('v2.3-empty-tcon.mp3') );
    
    my $tags = $s->{tags};
    
    is( !exists( $tags->{TCON} ), 1, 'ID3v2.3 empty TCON ok' );
    is( $tags->{TRCK}, '03/09', 'ID3v2.3 empty TCON track ok' );
}

# ID3v2.3 from iTunes with non-standard tags with spaces
{
    my $s = Audio::Scan->scan( _f('v2.3-itunes81.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.3.0', 'ID3v2.3 from iTunes ok' );
    is( $tags->{'TST '}, 'Track Title Sort', 'ID3v2.3 invalid iTunes frame ok' );
    is( ref $tags->{RVAD}, 'ARRAY', 'iTunes 8.1 RVAD ok' );
    is( $tags->{RVAD}->[0], '-2.119539 dB', 'iTunes 8.1 RVAD right ok' );
    is( $tags->{RVAD}->[1], '0.000000', 'iTunes 8.1 RVAD right peak ok' );
    is( $tags->{RVAD}->[2], '-2.119539 dB', 'iTunes 8.1 RVAD left ok' );
    is( $tags->{RVAD}->[3], '0.000000', 'iTunes 8.1 RVAD left peak ok' );
}

# ID3v2.3 corrupted text, from http://bugs.gentoo.org/show_bug.cgi?id=210564
{
    my $s = Audio::Scan->scan( _f('gentoo-bug-210564.mp3') );
    
    my $tags = $s->{tags};
    
    my $title = Encode::decode_utf8("花火");
    
    is( $tags->{TALB}, 'aikosingles', 'ID3v2.3 corrupted album ok' );
    is( $tags->{TIT2}, $title, 'ID3v2.3 corrupted title ok' );
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
    is( $tags->{RVA2}->[0], 'normalize', 'ID3v2.4 RVA2 ok' );
    is( $tags->{RVA2}->[1], 1, 'ID3v2.4 RVA2 channel ok' );
    is( $tags->{RVA2}->[2], '4.972656 dB', 'ID3v2.4 RVA2 adjustment ok' );
    is( $tags->{RVA2}->[3], '0.000000 dB', 'ID3v2.4 RVA2 peak ok' );
    is( $tags->{TBPM}, 120, 'ID3v2.4 BPM field ok' );
    is( $tags->{UFID}->[0], 'foo@foo.com', 'ID3v2.4 UFID owner id ok' );
    is( $tags->{UFID}->[1], 'da39a3ee5e6b4b0d3255bfef95601890afd80709', 'ID3v2.4 UFID ok' );
    is( $tags->{'USER FRAME'}, 'User Data', 'ID3v2.4 TXXX ok' );
    is( $tags->{WCOM}, 'http://www.google.com', 'ID3v2.4 WCOM ok' );
    is( $tags->{'USER URL'}, 'http://www.google.com', 'ID3v2.4 WXXX ok' );
    
    # XXX: 2 WOAR frames
}

# ID3v2.4 with negative RVA2
{
    my $s = Audio::Scan->scan_tags( _f('v2.4-rva2-neg.mp3') );
    
    my $tags = $s->{tags};
    is( $tags->{RVA2}->[2], '-2.123047 dB', 'ID3v2.4 negative RVA2 adjustment ok' );
}

# Multiple RVA2 tags with peak, from mp3gain
{
    my $s = Audio::Scan->scan( _f('v2.4-rva2-mp3gain.mp3') );
    
    my $tags = $s->{tags};
    is( ref $tags->{RVA2}, 'ARRAY', 'mp3gain RVA2 ok' );
    is( $tags->{RVA2}->[0]->[0], 'track', 'mp3gain track RVA2 ok' );
    is( $tags->{RVA2}->[0]->[2], '-7.478516 dB', 'mp3gain track gain ok' );
    is( $tags->{RVA2}->[0]->[3], '1.172028 dB', 'mp3gain track peak ok' );
    is( $tags->{RVA2}->[1]->[0], 'album', 'mp3gain album RVA2 ok' );
    is( $tags->{RVA2}->[1]->[2], '-7.109375 dB', 'mp3gain album gain ok' );
    is( $tags->{RVA2}->[1]->[3], '1.258026 dB', 'mp3gain album peak ok' );
}

# ID3v2.4 ISO-8859-1
{
    my $s = Audio::Scan->scan_tags( _f('v2.4-iso-8859-1.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, $pate, 'ID3v2.4 ISO-8859-1 artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 ISO-8859-1 title ok' );
}

# ID3v2.4 UTF-16BE
{
    my $s = Audio::Scan->scan_tags( _f('v2.4-utf16be.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, $pate, 'ID3v2.4 UTF-16BE artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 UTF-16BE title ok' );
    is( $tags->{TCON}, 'Ambient', 'ID3v2.4 genre in (NN) format ok' );
}

# ID3v2.4 UTF-16LE
{
    my $s = Audio::Scan->scan_tags( _f('v2.4-utf16le.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TPE1}, $pate, 'ID3v2.4 UTF-16LE artist ok' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 UTF-16LE title ok' );
}

# ID3v2.4 UTF-8
{
    my $s = Audio::Scan->scan_tags( _f('v2.4-utf8.mp3') );
    
    my $tags = $s->{tags};
    
    my $a = Encode::decode_utf8('ЪЭЯ');
    my $b = Encode::decode_utf8('ΈΤ');
    my $c = Encode::decode_utf8('γζ');
    
    is( $tags->{TPE1}, $a, 'ID3v2.4 UTF-8 title ok' );
    is( $tags->{$b}, $c, 'ID3v2.4 UTF-8 TXXX key/value ok' );
}

# ID3v2.4 from iTunes with non-standard tags with spaces
{
    my $s = Audio::Scan->scan( _f('v2.4-itunes81.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{id3_version}, 'ID3v2.4.0', 'ID3v2.4 from iTunes ok' );
    is( $tags->{'TST '}, 'Track Title Sort', 'ID3v2.4 invalid iTunes frame ok' );
    is( $tags->{TCON}, 'Metal', 'ID3v2.4 TCON with (9) ok' );
}

# ID3v2.4 with JPEG APIC
{
    my $s = Audio::Scan->scan( _f('v2.4-apic-jpg.mp3') );
    
    my $tags = $s->{tags};
    
    is( ref $tags->{APIC}, 'ARRAY', 'ID3v2.4 APIC JPEG frame is array' );
    is( $tags->{APIC}->[0], 0, 'ID3v2.4 APIC JPEG frame text encoding ok' );
    is( $tags->{APIC}->[1], 'image/jpeg', 'ID3v2.4 APIC JPEG mime type ok' );
    is( $tags->{APIC}->[2], 3, 'ID3v2.4 APIC JPEG picture type ok' );
    is( $tags->{APIC}->[3], 'This is the front cover description', 'ID3v2.4 APIC JPEG description ok' );
    is( length( $tags->{APIC}->[4] ), 2103, 'ID3v2.4 APIC JPEG picture length ok' );
    is( unpack( 'H*', substr( $tags->{APIC}->[4], 0, 4 ) ), 'ffd8ffe0', 'ID3v2.4 APIC JPEG picture data ok ');
}

# ID3v2.4 with PNG APIC
{
    my $s = Audio::Scan->scan( _f('v2.4-apic-png.mp3') );
    
    my $tags = $s->{tags};
    
    is( ref $tags->{APIC}, 'ARRAY', 'ID3v2.4 APIC PNG frame is array' );
    is( $tags->{APIC}->[0], 0, 'ID3v2.4 APIC PNG frame text encoding ok' );
    is( $tags->{APIC}->[1], 'image/png', 'ID3v2.4 APIC PNG mime type ok' );
    is( $tags->{APIC}->[2], 3, 'ID3v2.4 APIC PNG picture type ok' );
    is( $tags->{APIC}->[3], 'This is the front cover description', 'ID3v2.4 APIC PNG description ok' );
    is( length( $tags->{APIC}->[4] ), 58618, 'ID3v2.4 APIC PNG picture length ok' );
    is( unpack( 'H*', substr( $tags->{APIC}->[4], 0, 4 ) ), '89504e47', 'ID3v2.4 APIC PNG picture data ok ');
}

# ID3v2.4 with multiple APIC
{
    my $s = Audio::Scan->scan( _f('v2.4-apic-multiple.mp3') );
    
    my $tags = $s->{tags};
    
    my $png = $tags->{APIC}->[0];
    my $jpg = $tags->{APIC}->[1];
    
    is( ref $png, 'ARRAY', 'ID3v2.4 APIC PNG frame is array' );
    is( $png->[0], 0, 'ID3v2.4 APIC PNG frame text encoding ok' );
    is( $png->[1], 'image/png', 'ID3v2.4 APIC PNG mime type ok' );
    is( $png->[2], 3, 'ID3v2.4 APIC PNG picture type ok' );
    is( $png->[3], 'This is the front cover description', 'ID3v2.4 APIC PNG description ok' );
    is( length( $png->[4] ), 58618, 'ID3v2.4 APIC PNG picture length ok' );
    is( unpack( 'H*', substr( $png->[4], 0, 4 ) ), '89504e47', 'ID3v2.4 APIC PNG picture data ok ');
    
    is( ref $jpg, 'ARRAY', 'ID3v2.4 APIC JPEG frame is array' );
    is( $jpg->[0], 0, 'ID3v2.4 APIC JPEG frame text encoding ok' );
    is( $jpg->[1], 'image/jpeg', 'ID3v2.4 APIC JPEG mime type ok' );
    is( $jpg->[2], 4, 'ID3v2.4 APIC JPEG picture type ok' );
    is( $jpg->[3], 'This is the back cover description', 'ID3v2.4 APIC JPEG description ok' );
    is( length( $jpg->[4] ), 2103, 'ID3v2.4 APIC JPEG picture length ok' );
    is( unpack( 'H*', substr( $jpg->[4], 0, 4 ) ), 'ffd8ffe0', 'ID3v2.4 APIC JPEG picture data ok ');
}

# ID3v2.4 with GEOB
{
    my $s = Audio::Scan->scan( _f('v2.4-geob.mp3') );
    
    my $tags = $s->{tags};
    
    is( ref $tags->{GEOB}, 'ARRAY', 'ID3v2.4 GEOB is array' );
    is( $tags->{GEOB}->[0], 0, 'ID3v2.4 GEOB text encoding ok' );
    is( $tags->{GEOB}->[1], 'text/plain', 'ID3v2.4 GEOB mime type ok' );
    is( $tags->{GEOB}->[2], 'eyeD3.txt', 'ID3v2.4 GEOB filename ok' );
    is( $tags->{GEOB}->[3], 'eyeD3 --help output', 'ID3v2.4 GEOB content description ok' );
    is( length( $tags->{GEOB}->[4] ), 6207, 'ID3v2.4 GEOB length ok' );
    is( substr( $tags->{GEOB}->[4], 0, 6 ), "\nUsage", 'ID3v2.4 GEOB content ok' );
}

# ID3v2.4 with multiple GEOB
{
    my $s = Audio::Scan->scan( _f('v2.4-geob-multiple.mp3') );
    
    my $tags = $s->{tags};
    
    my $a = $tags->{GEOB}->[0];
    my $b = $tags->{GEOB}->[1];
    
    is( ref $a, 'ARRAY', 'ID3v2.4 GEOB multiple A is array' );
    is( $a->[0], 0, 'ID3v2.4 GEOB multiple A text encoding ok' );
    is( $a->[1], 'text/plain', 'ID3v2.4 GEOB multiple A mime type ok' );
    is( $a->[2], 'eyeD3.txt', 'ID3v2.4 GEOB multiple A filename ok' );
    is( $a->[3], 'eyeD3 --help output', 'ID3v2.4 GEOB multiple A content description ok' );
    is( length( $a->[4] ), 6207, 'ID3v2.4 GEOB multiple A length ok' );
    is( substr( $a->[4], 0, 6 ), "\nUsage", 'ID3v2.4 GEOB multiple A content ok' );
    
    is( ref $b, 'ARRAY', 'ID3v2.4 GEOB multiple B is array' );
    is( $b->[0], 0, 'ID3v2.4 GEOB multiple B text encoding ok' );
    is( $b->[1], 'text/plain', 'ID3v2.4 GEOB multiple B mime type ok' );
    is( $b->[2], 'genres.txt', 'ID3v2.4 GEOB multiple B filename ok' );
    is( $b->[3], 'eyeD3 --list-genres output', 'ID3v2.4 GEOB multiple B content description ok' );
    is( length( $b->[4] ), 4087, 'ID3v2.4 GEOB multiple B length ok' );
    is( substr( $b->[4], 0, 10 ), '  0: Blues', 'ID3v2.4 GEOB multiple B content ok' );
}

# ID3v2.4 with TIPL frame that has multiple strings
{
    my $s = Audio::Scan->scan( _f('v2.4-tipl.mp3') );
    
    my $tags = $s->{tags};
    
    is( ref $tags->{TIPL}, 'ARRAY', 'ID3v2.4 TIPL array ok' );
    is( $tags->{TIPL}->[0], 'producer', 'ID3v2.4 TIPL string 1 ok' );
    is( $tags->{TIPL}->[1], 'Steve Albini', 'ID3v2.4 TIPL string 2 ok' );
    is( $tags->{TIPL}->[2], 'engineer', 'ID3v2.4 TIPL string 3 ok' );
    is( $tags->{TIPL}->[3], 'Steve Albini', 'ID3v2.4 TIPL string 4 ok' );
}

# ID3v2.4 + APEv2 tags, some tags are multiple
{
    my $s = Audio::Scan->scan( _f('v2.4-ape.mp3') );
    
    my $tags = $s->{tags};
    
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 with APEv2 tag ok' );
    is( $tags->{APE_TAGS_SUCK}, 1, 'APEv2 tag ok' );
    is( ref $tags->{POPULARIMETER}, 'ARRAY', 'APEv2 POPULARIMETER tag ok' );
    is( $tags->{POPULARIMETER}->[0], 'foo@foo.com|150|1234567890', 'APEv2 POPULARIMETER tag 1 ok' );
    is( $tags->{POPULARIMETER}->[1], 'foo2@foo.com|30|7', 'APEv2 POPULARIMETER tag 2 ok' );
}

# iTunes-tagged file with invalid length frames
{
	my $s = Audio::Scan->scan_tags( _f('v2.4-itunes-broken-syncsafe.mp3') );
	
	my $tags = $s->{tags};
	
	is( scalar( keys %{$tags} ), 10, 'iTunes broken syncsafe read all tags ok' );
	is( scalar( @{ $tags->{COMM} } ), 4, 'iTunes broken syncsafe read all COMM frames ok' );
	is( length( $tags->{APIC}->[4] ), 29614, 'iTunes broken syncsafe read APIC ok' );
}

# v2.2 PIC frame
{
    my $s = Audio::Scan->scan_tags( _f('v2.2-pic.mp3') );
    
    my $tags = $s->{tags};
    
    is( scalar( @{ $tags->{APIC} } ), 5, 'v2.2 PIC fields ok' );
    is( $tags->{APIC}->[1], 'PNG', 'v2.2 PIC image format field ok' );
    is( $tags->{APIC}->[2], 0, 'v2.2 PIC picture type ok' );
    is( $tags->{APIC}->[3], '', 'v2.2 PIC description ok' );
    is( length( $tags->{APIC}->[4] ), 61007, 'v2.2 PIC data length ok' );
    is( unpack( 'H*', substr( $tags->{APIC}->[4], 0, 4 ) ), '89504e47', 'v2.2 PIC PNG picture data ok ');
} 

# Scan via a filehandle
{
    open my $fh, '<', _f('v2.4.mp3');
    
    my $s = Audio::Scan->scan_fh( mp3 => $fh );
    
    my $info = $s->{info};
    my $tags = $s->{tags};

    is( $info->{id3_version}, 'ID3v2.4.0', 'ID3v2.4 version ok via filehandle' );
    is( $tags->{TPE1}, 'Artist Name', 'ID3v2.4 artist ok via filehandle' );
    is( $tags->{TIT2}, 'Track Title', 'ID3v2.4 title ok via filehandle' );
    
    close $fh;
}

# Find frame offset
{
    my $offset = Audio::Scan->find_frame( _f('no-tags-no-xing-vbr.mp3'), 17005 );
    
    is( $offset, 17151, 'Find frame ok' );
    
    # Find first frame past Xing tag
    $offset = Audio::Scan->find_frame( _f('no-tags-mp1l3-vbr.mp3'), 1 );
    
    is( $offset, 576, 'Find frame past Xing tag ok' );
}

{
    open my $fh, '<', _f('no-tags-no-xing-vbr.mp3');
    
    my $offset = Audio::Scan->find_frame_fh( mp3 => $fh, 42000 );
    
    is( $offset, 42641, 'Find frame via filehandle ok' );
    
    close $fh;
}

# Bug 12409, file with just enough junk data before first audio frame
# to require a second buffer read
{
    my $s = Audio::Scan->scan_info( _f('v2.3-null-bytes.mp3') );
    
    my $info = $s->{info};
    
    is( $info->{audio_offset}, 4896, 'Bug 12409 audio offset ok' );
    is( $info->{bitrate}, 64000, 'Bug 12409 bitrate ok' ); # XXX technically should be 128k, but Xing data is wrong
    is( $info->{lame_encoder_version}, 'LAME3.96r', 'Bug 12409 encoder version ok' );
    is( $info->{song_length_ms}, 244464, 'Bug 12409 song length ok' );
}

# Bug 9942, APE tag with no ID3v1 tag and multiple tags
{
    my $s = Audio::Scan->scan( _f('ape-no-v1.mp3') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{ape_version}, 'APEv2', 'APE no ID3v1 ok' );
    
    is( $tags->{ALBUM}, '13 Blues for Thirteen Moons', 'APE no ID3v1 ALBUM ok' );
    is( ref $tags->{ARTIST}, 'ARRAY', 'APE no ID3v1 ARTIST ok' );
    is( $tags->{ARTIST}->[0], 'artist1', 'APE no ID3v1 artist1 ok' );
    is( $tags->{ARTIST}->[1], 'artist2', 'APE no ID3v1 artist2 ok' );
}

sub _f {    
    return catfile( $FindBin::Bin, 'mp3', shift );
}

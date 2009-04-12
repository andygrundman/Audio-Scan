use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 127;

use Audio::Scan;
use Encode;

# Basic tests of all fields
{
    my $s = Audio::Scan->scan( _f('wma92-32k.wma') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{audio_offset}, 5111, 'Audio offset ok' );
    is( $info->{broadcast}, 0, 'Broadcast not set ok' );
    is( ref $info->{codec_list}, 'ARRAY', 'Codec list ok' );
    is( $info->{codec_list}->[0]->{description}, ' 32 kbps, 22 kHz, stereo 2-pass CBR', 'Codec description ok' );
    is( $info->{codec_list}->[0]->{name}, 'Windows Media Audio 9.2', 'Codec name ok' );
    is( $info->{codec_list}->[0]->{type}, 'Audio', 'Codec type ok' );
    is( $info->{creation_date}, 1239379533, 'Creation date ok' );
    is( $info->{data_packets}, 5, 'Data packets ok' );
    is( $info->{file_id}, '4c2d71e7-f116-4e47-ae0f-e27a4632f9e3', 'File ID ok' );
    is( $info->{file_size}, 12751, 'File size ok' );
    is( ref $info->{language_list}, 'ARRAY', 'Language list ok' );
    is( $info->{language_list}->[0], 'en-us', 'Language list item ok' );
    is( $info->{max_bitrate}, 32645, 'Max bitrate ok' );
    is( $info->{max_packet_size}, 1518, 'Max packet size ok' );
    is( $info->{min_packet_size}, 1518, 'Min packet size ok' );
    is( $info->{play_duration_ms}, 2602, 'Play duration ok' );
    is( $info->{preroll}, 1579, 'Preroll ok' );
    is( $info->{seekable}, 1, 'Seekable ok' );
    is( $info->{send_duration_ms}, 1857, 'Send duration ok' );
    is( $info->{song_length_ms}, 1023, 'Song length ok' );
    
    is( ref $info->{streams}, 'ARRAY', 'Streams ok' );
    
    my $stream = $info->{streams}->[0];
    
    is( $stream->{DeviceConformanceTemplate}, 'L2', 'DeviceConformanceTemplate ok' );
    is( $stream->{IsVBR}, 0, 'IsVBR ok' );
    is( $stream->{alt_bitrate}, 32024, 'Alt bitrate ok' );
    is( $stream->{alt_buffer_fullness}, 0, 'Alt buffer fullness ok' );
    is( $stream->{alt_buffer_size}, 1579, 'Alt buffer size ok' );
    is( $stream->{avg_bitrate}, 32645, 'Average bitrate ok' );
    is( $stream->{avg_bytes_per_sec}, 4003, 'Average bytes/sec ok' );
    is( $stream->{bitrate}, 32024, 'Bitrate ok' );
    is( $stream->{bits_per_sample}, 16, 'Bits per sample ok' );
    is( $stream->{block_alignment}, 1487, 'Block alignment ok' );
    is( $stream->{buffer_fullness}, 0, 'Buffer fullness ok' );
    is( $stream->{buffer_size}, 1579, 'Buffer size ok' );
    is( $stream->{channels}, 2, 'Channels ok' );
    is( $stream->{codec_id}, 0x161, 'Codec ID ok' );
    is( $stream->{encode_options}, 23, 'Encode options ok' );
    is( $stream->{encrypted}, 0, 'Encrypted ok' );
    is( $stream->{error_correction_type}, 'ASF_Audio_Spread', 'Error correction type ok' );
    is( $stream->{flag_seekable}, 1, 'Seekable ok' );
    is( $stream->{language_index}, 0, 'Language index ok' );
    is( $stream->{max_object_size}, 1487, 'Max object size ok' );
    is( $stream->{samplerate}, 22050, 'Sample rate ok' );
    is( $stream->{samples_per_block}, 17408, 'Samples per block ok' );
    is( $stream->{stream_number}, 1, 'Stream number ok' );
    is( $stream->{stream_type}, 'ASF_Audio_Media', 'Stream type ok' );
    is( $stream->{super_block_align}, 0, 'Super block align ok' );
    is( $stream->{time_offset}, 0, 'Time offset ok' );
    
    is( $tags->{Author}, 'Author String', 'Author tag ok' );
    is( $tags->{Copyright}, 'Copyright String', 'Copyright tag ok' );
    is( $tags->{Description}, 'Description String', 'Description tag ok' );
    is( $tags->{IsVBR}, 0, 'IsVBR tags ok' );
    is( $tags->{Rating}, 'Rating String', 'Rating tag ok' );
    is( $tags->{Title}, 'Voice Test', 'Title tag ok' );
    is( $tags->{WMFSDKNeeded}, '0.0.0.0000', 'WMFSDKNeeded tag ok' );
    is( $tags->{WMFSDKVersion}, '11.0.5721.5251', 'WMFSDKVersion tag ok' );
}

# Multiple bitrate file
{
    my $s = Audio::Scan->scan( _f('wma92-mbr.wma') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( ref $info->{index_blocks}, 'ARRAY', 'Index blocks ok' );
    is( $info->{index_blocks}->[0], 0, 'Index block 1 ok' );
    is( $info->{index_blocks}->[1], 0, 'Index block 2 ok' );
    is( $info->{index_entry_interval}, 1000, 'Index entry interval ok' );
    is( ref $info->{index_offsets}, 'ARRAY', 'Index offsets ok' );
    is( $info->{index_offsets}->[0]->[4], 11811, 'Index offset stream 1 ok' );
    is( $info->{index_offsets}->[1]->[3], 7811, 'Index offset stream 2 ok' );
    is( ref $info->{index_specifiers}, 'ARRAY', 'Index specifiers ok' );
    is( $info->{index_specifiers}->[0], 1, 'Index specifier stream 1 ok' );
    is( $info->{index_specifiers}->[1], 2, 'Index specifier stream 2 ok' );
    is( ref $info->{mutex_list}, 'ARRAY', 'Mutex list ok' );
    is( $info->{mutex_list}->[0]->{ASF_Mutex_Bitrate}->[0], 1, 'Mutex stream 1 ok' );
    is( $info->{mutex_list}->[0]->{ASF_Mutex_Bitrate}->[1], 2, 'Mutex stream 2 ok' );
    
    is( $info->{streams}->[0]->{stream_number}, 1, 'Stream 1 ok' );
    is( $info->{streams}->[1]->{stream_number}, 2, 'Stream 2 ok' );
    
    is( $tags->{'User Key'}, 'User Value', 'User key ok' );
}

# VBR file
{
    my $s = Audio::Scan->scan( _f('wma92-vbr.wma') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{streams}->[0]->{IsVBR}, 1, 'IsVBR ok' );
    is( $info->{streams}->[0]->{avg_bitrate}, 53719, 'Average bitrate ok' );
    
    my $pate = Encode::decode_utf8("pâté");
    my $ber  = Encode::decode_utf8('ЪЭЯ');
    my $yc   = Encode::decode_utf8('γζ');
    
    is( $tags->{'Latin1 Key'}, $pate, 'Latin1 tag ok' );
    is( $tags->{'Russian Key'}, $ber, 'Unicode tag ok' );
    is( $tags->{$ber}, $yc, 'Unicode key/value ok' );
    
    is( ref $tags->{'WM/Picture'}, 'HASH', 'WM/Picture ok' );
    is( $tags->{'WM/Picture'}->{image_type}, 3, 'WM/Picture type ok' );
    is( $tags->{'WM/Picture'}->{mime_type}, 'image/jpeg', 'WM/Picture MIME type ok' );
    is( length($tags->{'WM/Picture'}->{image}), 2103, 'WM/Picture length ok' );
}

# WMA Pro 10 file
{
    my $s = Audio::Scan->scan( _f('wma92-48k-pro.wma') );
    
    my $info = $s->{info};
    
    is( $info->{codec_list}->[0]->{name}, 'Windows Media Audio 10 Professional', 'WMA 10 Pro ok' );
    is( $info->{streams}->[0]->{codec_id}, 0x0162, 'WMA 10 Pro codec ID ok' );
}

# WMA Lossless file
{
    my $s = Audio::Scan->scan( _f('wma92-lossless.wma') );
    
    my $info = $s->{info};
    
    is( $info->{codec_list}->[0]->{name}, 'Windows Media Audio 9.2 Lossless', 'WMA Lossless ok' );
    is( $info->{streams}->[0]->{codec_id}, 0x0163, 'WMA Lossless codec ID ok' );
    is( $info->{streams}->[0]->{avg_bitrate}, 607494, 'WMA Lossless average bitrate ok' );
}

# WMA Voice file with duplicate tags
{
    my $s = Audio::Scan->scan( _f('wma92-voice.wma') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{streams}->[0]->{codec_id}, 0x000a, 'WMA Voice codec ID ok' );
    
    # Note these are out of order because they are written to different objects by MP3tag
    is( ref $tags->{'WM/Composer'}, 'ARRAY', 'Multiple composer tags ok' );
    is( $tags->{'WM/Composer'}->[0], 'Composer 2', 'Composer 2 ok' );
    is( $tags->{'WM/Composer'}->[1], 'Composer 3', 'Composer 3 ok' );
    is( $tags->{'WM/Composer'}->[2], 'Composer 1', 'Composer 1 ok' );
}

# WMV file, no audio
{
    my $s = Audio::Scan->scan( _f('wmv92.wmv') );
    
    my $info = $s->{info};
    
    my $stream = $info->{streams}->[0];
    
    is( $info->{codec_list}->[0]->{name}, 'Windows Media Video 9 Screen', 'WMV ok' );
    is( $stream->{stream_type}, 'ASF_Video_Media', 'WMV stream type ok' );
    is( $stream->{bpp}, 24, 'WMV bpp ok' );
    is( $stream->{compression_id}, 'MSS2', 'WMV compression ID ok' );
    is( $stream->{height}, 57, 'WMV height ok' );
    is( $stream->{width}, 501, 'WMV width ok' );
}

# Video/Audio file
{
	my $s = Audio::Scan->scan( _f('wmv92-with-audio.wmv') );
    
    my $info = $s->{info};
    
    is( $info->{codec_list}->[0]->{name}, 'Windows Media Audio 9.2', 'WMV audio track ok' );
    is( $info->{codec_list}->[1]->{name}, 'Windows Media Video 9', 'WMV video track ok' );
    
    is( $info->{streams}->[0]->{stream_type}, 'ASF_Audio_Media', 'WMV audio stream ok' );
    is( $info->{streams}->[1]->{stream_type}, 'ASF_Video_Media', 'WMV video stream ok' );
}

# Live audio stream header
{
    my $s = Audio::Scan->scan( _f('wma-live.wma') );
    
    my $info = $s->{info};
    
    is( $info->{broadcast}, 1, 'Live stream ok' );
    is( $info->{seekable}, 0, 'Live stream not seekable ok' );
    ok( !exists $info->{file_size}, 'Live stream has no file size ok' );
    
    is( $info->{streams}->[1]->{stream_type}, 'ASF_Command_Media', 'Live stream metadata stream ok' );
}

# File with DRM, script commands, and 2 images
{
    my $s = Audio::Scan->scan( _f('drm.wma') );
    
    my $info = $s->{info};
    my $tags = $s->{tags};
    
    is( $info->{streams}->[0]->{encrypted}, 1, 'DRM encrypted flag set ok' );
    is( $info->{drm_key}, 'pMYQ3zAwEE+/lAEL5hP0Ug==', 'DRM key ok' );
    is( $info->{drm_license_url}, 'http://switchboard.real.com/rhapsody/?cd=wmupgrade', 'DRM license URL ok' );
    is( $info->{drm_protection_type}, 'DRM', 'DRM protection type ok' );
    
    is( ref $info->{script_types}, 'ARRAY', 'Script types ok' );
    is( $info->{script_types}->[0], 'URL', 'Script type URL ok' );
    is( $info->{script_types}->[1], 'FILENAME', 'Script type FILENAME ok' );
    is( ref $info->{script_commands}, 'ARRAY', 'Script commands ok' );
    is( $info->{script_commands}->[0]->{command}, 'http://www.microsoft.com/isapi/redir.dll?Prd=WMT4&Sbp=DRM&Plcid=0x0409&Pver=4.0&WMTFeature=DRM', 'Script command 1 ok' );
    is( $info->{script_commands}->[0]->{time}, 1579, 'Script time 1 ok' );
    is( $info->{script_commands}->[0]->{type}, 0, 'Script type 1 ok' );
    is( $info->{script_commands}->[1]->{command}, '', 'Script command 2 ok' );
    is( $info->{script_commands}->[1]->{time}, 1579, 'Script time 2 ok' );
    is( $info->{script_commands}->[1]->{type}, 1, 'Script type 2 ok' );
    
    is( ref $tags->{'WM/Picture'}, 'ARRAY', 'WM/Picture array ok' );
    is( $tags->{'WM/Picture'}->[0]->{description}, 'Large Cover Art', 'WM/Picture 1 description ok' );
    is( length( $tags->{'WM/Picture'}->[0]->{image} ), 4644, 'WM/Picture 1 image ok' );
    is( $tags->{'WM/Picture'}->[1]->{description}, 'Cover Art', 'WM/Picture 2 description ok' );
    is( length( $tags->{'WM/Picture'}->[1]->{image} ), 2110, 'WM/Picture 2 image ok ');
}

# File with JFIF image type and MP3 codec
{
    my $s = Audio::Scan->scan( _f('jfif.wma') );
    
    my $info = $s->{info};
    
    is( $info->{streams}->[0]->{stream_type}, 'ASF_JFIF_Media', 'JFIF stream ok' );
    is( $info->{streams}->[1]->{codec_id}, 85, 'MP3 codec ID ok' );
    is( $info->{streams}->[0]->{width}, 320, 'JFIF width ok' );
    is( $info->{streams}->[0]->{height}, 240, 'JFIF height ok' );
}
    

sub _f {
    return catfile( $FindBin::Bin, 'asf', shift );
}
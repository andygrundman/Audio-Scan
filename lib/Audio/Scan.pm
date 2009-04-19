package Audio::Scan;

use 5.008008;
use strict;

our $VERSION = '0.09';

require XSLoader;
XSLoader::load('Audio::Scan', $VERSION);

use constant FILTER_INFO_ONLY => 1;
use constant FILTER_TAGS_ONLY => 2;

sub scan_info {
    my ( $class, $file ) = @_;
    
    $class->scan( $file, FILTER_INFO_ONLY );
}

sub scan_tags {
    my ( $class, $file ) = @_;
    
    $class->scan( $file, FILTER_TAGS_ONLY );
}

1;
__END__

=head1 NAME

Audio::Scan - Fast C parser for MP3, Ogg Vorbis, FLAC, ASF

=head1 SYNOPSIS

    use Audio::Scan;

    my $data = Audio::Scan->scan('/path/to/file.mp3');

    # Just file info
    my $info = Audio::Scan->scan_info('/path/to/file.mp3');

    # Just tags
    my $tags = Audio::Scan->scan_tags('/path/to/file.mp3');
    
    # Scan a filehandle
    open my $fh, '<', 'my.mp3';
    my $data = Audio::Scan->scan_fh( mp3 => $fh );
    close $fh;

=head1 DESCRIPTION

Audio::Scan is a C-based scanner for audio file metadata and tag information. It currently
supports MP3 via an included version of libid3tag, Ogg Vorbis, FLAC (if libFLAC is
installed), and ASF.  A future release will add support for AAC, WAV, and possibly others.

See below for specific details about each file format.

=head1 METHODS

=head2 scan( $path )

Scans $path for both metadata and tag information.  The type of scan performed is
determined by the file's extension.  Supported extensions are:

    MP3:  mp3, mp2
    Ogg:  ogg, oga
    FLAC: flc, flac, fla
    ASF:  wma, wmv, asf

This method returns a hashref containing two other hashrefs: info and tags.  The
contents of the info and tag hashes vary depending on file format, see below for details.

=head2 scan_info( $path )

If you only need file metadata and don't care about tags, you can use this method.

=head2 scan_tags( $path )

If you only need the tags and don't care about the metadata, use this method.

=head2 scan_fh( $type => $fh )

Scans a filehandle. $type is the type of file to scan as, i.e. "mp3" or "ogg".
Note that FLAC does not support reading from a filehandle.

=head2 find_frame( $path, $offset )

Returns the byte offset to the first audio frame starting from $offset.

The offset value is different depending on the file type:

=over 4

=item MP3, Ogg

Offset is the byte offset to start searching from.  The byte offset to the first
audio packet/frame past this point will be returned.

=item ASF

Offset is a timestamp in milliseconds.  The byte offset to the ASF data packet
containing this timestamp will be returned.

=item FLAC

Not yet supported by find_frame.

=back

=head2 find_frame_fh( $type => $fh, $offset )

Same as L<find_frame>, but with a filehandle.

=head2 has_flac()

Returns 1 if FLAC support was compiled in, or 0 if not.

=head1 MP3

=head2 INFO

The following metadata about a file may be returned:

    id3_version (i.e. "ID3v2.4.0")
    song_length_ms (duration in milliseconds)
    layer (i.e. 3)
    stereo
    samples_per_frame
    padding
    audio_size (size of all audio frames)
    audio_offset (byte offset to first audio frame)
    bitrate (in bps, determined using Xing/LAME/VBRI if possible, or average in the worst case)
    samplerate (in kHz)
    vbr (1 if file is VBR)

    If a Xing header is found:
    xing_frames
    xing_bytes
    xing_quality

    If a VBRI header is found:
    vbri_delay
    vbri_frames
    vbri_bytes
    vbri_quality

    If a LAME header is found:
    lame_encoder_version
    lame_tag_revision
    lame_vbr_method
    lame_lowpass
    lame_replay_gain_radio
    lame_replay_gain_audiophile
    lame_encoder_delay
    lame_encoder_padding
    lame_noise_shaping
    lame_stereo_mode
    lame_unwise_settings
    lame_source_freq
    lame_surround
    lame_preset

=head2 TAGS

Raw tags are returned as found by libid3tag.  This means older tags such as ID3v1 and ID3v2.2
are converted to ID3v2.4 tag names.  Multiple instances of a tag in a file will be returned
as arrays.  Complex tags such as APIC and COMM are returned as arrays.  All tag fields are
converted to upper-case.  Sample tag data:

    tags => {
          ALBUMARTISTSORT => "Solar Fields",
          APIC => [ 0, "image/jpeg", 3, "", <binary data snipped> ],
          CATALOGNUMBER => "INRE 017",
          COMM => [0, "eng", "", "Amazon.com Song ID: 202981429"],
          "MUSICBRAINZ ALBUM ARTIST ID" => "a2af1f31-c9eb-4fff-990c-c4f547a11b75",
          "MUSICBRAINZ ALBUM ID" => "282143c9-6191-474d-a31a-1117b8c88cc0",
          "MUSICBRAINZ ALBUM RELEASE COUNTRY" => "FR",
          "MUSICBRAINZ ALBUM STATUS" => "official",
          "MUSICBRAINZ ALBUM TYPE" => "album",
          "MUSICBRAINZ ARTIST ID" => "a2af1f31-c9eb-4fff-990c-c4f547a11b75",
          "REPLAYGAIN_ALBUM_GAIN" => "-2.96 dB",
          "REPLAYGAIN_ALBUM_PEAK" => "1.045736",
          "REPLAYGAIN_TRACK_GAIN" => "+3.60 dB",
          "REPLAYGAIN_TRACK_PEAK" => "0.892606",
          TALB => "Leaving Home",
          TCOM => "Magnus Birgersson",
          TCON => "Ambient",
          TCOP => "2005 ULTIMAE RECORDS",
          TDRC => "2004-10",
          TIT2 => "Home",
          TPE1 => "Solar Fields",
          TPE2 => "Solar Fields",
          TPOS => "1/1",
          TPUB => "Ultimae Records",
          TRCK => "1/11",
          TSOP => "Solar Fields",
          UFID => [
                "http://musicbrainz.org",
                "1084278a-2254-4613-a03c-9fed7a8937ca",
          ],
    },

=head1 OGG VORBIS

=head2 INFO

The following metadata about a file is returned:

    version
    channels
    stereo
    samplerate (in kHz)
    bitrate_average (in bps)
    bitrate_upper
    bitrate_nominal
    bitrate_lower
    blocksize_0
    blocksize_1
    audio_offset (byte offset to audio)
    song_length_ms (duration in milliseconds)

=head2 TAGS

Raw Vorbis comments are returned.  All comment keys are capitalized.

=head1 FLAC

=head2 INFO

The following metadata about a file is returned:

    channels
    samplerate (in kHz)
    bitrate (in bps)
    file_size
    audio_offset (byte offset to audio)
    song_length_ms (duration in milliseconds)
    bits_per_sample
    frames
    minimum_blocksize
    maximum_blocksize
    minimum_framesize
    maximum_framesize
    md5
    total_samples

=head2 TAGS

Raw FLAC comments are returned.  All comment keys are capitalized.  Some data returned is special:

APPLICATION

    Each application block is returned in the APPLICATION tag keyed by application ID.

CUESHEET

    The CUESHEET tag is an array containing each line of the cue sheet.

PICTURE

    Embedded pictures are returned in an ALLPICTURES array.  Each picture has the following metadata:
    
        mime_type
        description
        width
        height
        depth
        color_index
        image_data
        picture_type

=head1 ASF (Windows Media Audio/Video)

=head2 INFO

The following metadata about a file may be returned.  Reading the ASF spec is encouraged if you
want to find out more about any of these values.

    audio_offset (byte offset to first data packet)
    broadcast (boolean, whether the file is a live broadcast or not)
    codec_list (array of information about codecs used in the file)
    creation_date (UNIX timestamp when file was created)
    data_packets
    drm_key
    drm_license_url
    drm_protection_type
    drm_data
    file_id (unique file ID)
    file_size
    index_blocks
    index_entry_interval (in milliseconds)
    index_offsets (byte offsets for each second of audio, per stream. Useful for seeking)
    index_specifiers (indicates which stream a given index_offset points to)
    language_list (array of languages referenced by the file's metadata)
    lossless (boolean)
    max_bitrate
    max_packet_size
    min_packet_size
    mutex_list (mutually exclusive stream information)
    play_duration_ms
    preroll
    script_commands
    script_types
    seekable (boolean, whether the file is seekable or not)
    send_duration_ms
    song_length_ms (the actual length of the audio, in milliseconds)

STREAMS

The streams array contains metadata related to an individul stream within the file.
The following metadata may be returned:
    
    DeviceConformanceTemplate
    IsVBR
    alt_bitrate
    alt_buffer_fullness
    alt_buffer_size
    avg_bitrate (most accurate bitrate for this stream)
    avg_bytes_per_sec (audio only)
    bitrate
    bits_per_sample (audio only)
    block_alignment (audio only)
    bpp (video only)
    buffer_fullness
    buffer_size
    channels (audio only)
    codec_id (audio only)
    compression_id (video only)
    encode_options
    encrypted (boolean)
    error_correction_type
    flag_seekable (boolean)
    height (video only)
    index_type
    language_index (offset into language_list array)
    max_object_size
    samplerate (in kHz) (audio only)
    samples_per_block
    stream_number
    stream_type
    super_block_align
    time_offset
    width (video only)

=head2 TAGS

Raw tags are returned.  Tags that occur more than once are returned as arrays.
In contrast to the other formats, tag keys are NOT capitalized. There is one special key:

WM/Picture

Pictures are returned as a hash with the following keys:

    image_type (numeric type, same as ID3v2 APIC)
    mime_type
    description
    image    

=head1 THANKS

Some of the file format parsing code was derived from the mt-daapd project,
and adapted by Netgear.  It has been heavily rewritten to fix bugs and add
more features.

The source to the original Netgear C scanner for SqueezeCenter is located
at L<http://svn.slimdevices.com/repos/slim/7.3/trunk/platforms/readynas/contrib/scanner>

=head1 SEE ALSO

ASF Spec L<http://www.microsoft.com/windows/windowsmedia/forpros/format/asfspec.aspx>

=head1 AUTHORS

Andy Grundman, E<lt>andy@slimdevices.comE<gt>

Dan Sully, E<lt>daniel@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009 Logitech, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

=cut

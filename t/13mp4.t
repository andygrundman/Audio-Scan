use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 67;

use Audio::Scan;

# AAC file from iTunes 8.1.1
{
    my $s = Audio::Scan->scan( _f('itunes811.m4a') );
    
    my $info  = $s->{info};
    my $tags  = $s->{tags};
    my $track = $info->{tracks}->[0];
    
    is( $info->{audio_offset}, 6169, 'Audio offset ok' );
    is( $info->{compatible_brands}->[0], 'M4A ', 'Compatible brand 1 ok' );
    is( $info->{compatible_brands}->[1], 'mp42', 'Compatible brand 2 ok' );
    is( $info->{compatible_brands}->[2], 'isom', 'Compatible brand 3 ok' );
    is( $info->{leading_mdat}, undef, 'Leading MDAT flag is blank' );
    is( $info->{major_brand}, 'M4A ', 'Major brand ok' );
    is( $info->{minor_version}, 0, 'Minor version ok' );
    is( $info->{song_length_ms}, 69, 'Song length ok' );
    is( $info->{timescale}, 44100, 'Timescale ok' );
    
    is( $track->{audio_type}, 64, 'Audio type ok' );
    is( $track->{avg_bitrate}, 96000, 'Avg bitrate ok' );
    is( $track->{bits_per_sample}, 16, 'Bits per sample ok' );
    is( $track->{channels}, 2, 'Channels ok' );
    is( $track->{duration}, 69, 'Duration ok' );
    is( $track->{encoding}, 'mp4a', 'Encoding ok' );
    is( $track->{handler_name}, '', 'Handler name ok' );
    is( $track->{handler_type}, 'soun', 'Handler type ok' );
    is( $track->{id}, 1, 'Track ID ok' );
    is( $track->{max_bitrate}, 0, 'Max bitrate ok' );
    is( $track->{samplerate}, 44100, 'Samplerate ok' );
    
    is( $tags->{AART}, 'Album Artist', 'AART ok' );
    is( $tags->{ALB}, 'Album', 'ALB ok' );
    is( $tags->{ART}, 'Artist', 'ART ok' );
    is( $tags->{CMT}, 'Comments', 'CMT ok' );
    is( length($tags->{COVR}), 2103, 'COVR ok' );
    is( $tags->{CPIL}, 1, 'CPIL ok' );
    is( $tags->{DAY}, 2009, 'DAY ok' );
    is( $tags->{DESC}, 'Video Description', 'DESC ok' );
    is( $tags->{DISK}, '1/2', 'DISK ok' );
    is( $tags->{GNRE}, 'Jazz', 'GNRE ok' );
    is( $tags->{GRP}, 'Grouping', 'GRP ok' );
    is( $tags->{ITUNNORM}, ' 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000', 'ITUNNORM ok' );
    is( $tags->{ITUNSMPB}, ' 00000000 00000840 000001E4 00000000000001DC 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000', 'ITUNSMPB ok' );
    is( $tags->{LYR}, 'Lyrics', 'LYR ok' );
    is( $tags->{NAM}, 'Name', 'NAM ok' );
    is( $tags->{PGAP}, 1, 'PGAP ok' );
    is( $tags->{SOAA}, 'Sort Album Artist', 'SOAA ok' );
    is( $tags->{SOAL}, 'Sort Album', 'SOAL ok' );
    is( $tags->{SOAR}, 'Sort Artist', 'SOAR ok' );
    is( $tags->{SOCO}, 'Sort Composer', 'SOCO ok' );
    is( $tags->{SONM}, 'Sort Name', 'SONM ok' );
    is( $tags->{SOSN}, 'Sort Show', 'SOSN ok' );
    is( $tags->{TMPO}, 120, 'TMPO ok' );
    is( $tags->{TOO}, 'iTunes 8.1.1, QuickTime 7.6', 'TOO ok' );
    is( $tags->{TRKN}, '1/10', 'TRKN ok' );
    is( $tags->{TVEN}, 'Episode ID', 'TVEN ok' );
    is( $tags->{TVES}, 12, 'TVES ok' );
    is( $tags->{TVSH}, 'Show', 'TVSH ok' );
    is( $tags->{TVSN}, 12, 'TVSN ok' );
    is( $tags->{WRT}, 'Composer', 'WRT ok' );
}

# ALAC file from iTunes 8.1.1
{
    my $s = Audio::Scan->scan( _f('alac.m4a') );
    
    my $info  = $s->{info};
    my $tags  = $s->{tags};
    my $track = $info->{tracks}->[0];
    
    is( $info->{audio_offset}, 3850, 'ALAC audio offset ok' );
    is( $info->{song_length_ms}, 10, 'ALAC song length ok' );
    is( $info->{timescale}, 44100, 'ALAC timescale ok' );
    
    is( $track->{avg_bitrate}, 122700, 'ALAC avg bitrate ok' );
    is( $track->{duration}, 10, 'ALAC duration ok' );
    is( $track->{encoding}, 'alac', 'ALAC encoding ok' );
    
    is( $tags->{CPIL}, 0, 'ALAC CPIL ok' );
    is( $tags->{DISK}, '1/2', 'ALAC DISK ok' );
    is( $tags->{TOO}, 'iTunes 8.1.1', 'ALAC TOO ok' );
}

# File with mdat before the rest of the boxes
{
    my $s = Audio::Scan->scan( _f('leading-mdat.m4a') );
    
    my $info  = $s->{info};
    my $tags  = $s->{tags};
    my $track = $info->{tracks}->[0];
    
    is( $info->{audio_offset}, 20, 'Leading MDAT offset ok' );
    is( $info->{leading_mdat}, 1, 'Leading MDAT flag ok' );
    is( $info->{song_length_ms}, 69845, 'Leading MDAT length ok' );
    
    is( $track->{avg_bitrate}, 128000, 'Leading MDAT bitrate ok' );
    is( $track->{samplerate}, 44100, 'Leading MDAT samplerate ok' );
    
    is( $tags->{DAY}, '-001', 'Leading MDAT DAY ok' );
    is( $tags->{TOO}, 'avc2.0.11.1110', 'Leading MDAT TOO ok' );
}

# Find frame
{
    my $offset = Audio::Scan->find_frame( _f('itunes811.m4a'), 30 );
    
    is( $offset, 6177, 'Find frame ok' );
}

sub _f {
    return catfile( $FindBin::Bin, 'mp4', shift );
}
use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 17;

use Audio::Scan;
use Encode;

# Basics
{
    my $s = Audio::Scan->scan( _f('test.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};
    my $utf8 = Encode::decode_utf8('シチヅヲ');

    is($tags->{ARTIST}, 'Test Artist', 'ASCII Tag ok');
    is($tags->{YEAR}, 2009, 'Year Tag ok');
    is($tags->{PERFORMER}, $utf8, 'PERFORMER (UTF8) Tag ok');

    is($info->{CHANNELS}, 2, 'Channels ok');
    is($info->{RATE}, 44100, 'Sample Rate ok');
    ok($info->{VENDOR} =~ /Xiph/, 'Vendor ok');
}

# Multiple tags.
{
    my $s = Audio::Scan->scan( _f('multiple.ogg') );

    my $tags = $s->{tags};

    is($tags->{ARTIST}[0], 'Multi 1', 'Multiple Artist 1 ok');
    is($tags->{ARTIST}[1], 'Multi 2', 'Multiple Artist 1 ok');
    is($tags->{ARTIST}[2], 'Multi 3', 'Multiple Artist 1 ok');
}

# Equals char in tag.
{
    my $s = Audio::Scan->scan( _f('equals-char.ogg') );

    my $tags = $s->{tags};

    is($tags->{TITLE}, 'Me - You = Loneliness', 'Equals char in tag ok');
}

# Large page size.
{
    my $s = Audio::Scan->scan( _f('large-pagesize.ogg') );

    my $tags = $s->{tags};

    is($tags->{TITLE}, 'Deadzy', 'Large page title tag ok');
    is($tags->{ARTIST}, 'Medeski Scofield Martin & Wood', 'Large page artist tag ok');
    is($tags->{ALBUM}, 'Out Louder (bonus disc)', 'Large page album tag ok');
}

# Old encoder files.
{
    my $s1 = Audio::Scan->scan( _f('old1.ogg') );
    is($s1->{tags}->{ALBUM}, 'AutoTests', 'Old encoded album tag ok');
    is($s1->{info}->{RATE}, 8000, 'Old encoded rate ok');

    my $s2 = Audio::Scan->scan( _f('old2.ogg') );
    is($s2->{tags}->{ALBUM}, 'AutoTests', 'Old encoded album tag ok');
    is($s2->{info}->{RATE}, 12000, 'Old encoded rate ok');
}

sub _f {
    return catfile( $FindBin::Bin, 'ogg', shift );
}

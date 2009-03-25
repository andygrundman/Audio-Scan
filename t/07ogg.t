use strict;

use File::Spec::Functions;
use FindBin ();
use Test::More tests => 6;

use Audio::Scan;

# Basics
{
    my $s = Audio::Scan->scan( _f('test.ogg') );

    my $info = $s->{info};
    my $tags = $s->{tags};

    is($tags->{ARTIST}, 'Test Artist', 'ASCII Tag ok');
    is($tags->{YEAR}, 2009, 'Year Tag ok');
    is($tags->{PERFORMER}, 'シチヅヲ', 'PERFORMER (UTF8) Tag ok');

    is($info->{CHANNELS}, 2, 'Channels ok');
    is($info->{SAMPLERATE}, 44100, 'Sample Rate ok');
    ok($info->{VENDOR} =~ /Xiph/, 'Vendor ok');
}

sub _f {
    return catfile( $FindBin::Bin, 'ogg', shift );
}

#!/usr/bin/perl

use lib qw(blib/lib blib/arch);
use Audio::Scan;
use Time::HiRes qw(sleep);

my $file = shift;

for ( 1..5000 ) {
    my $s = Audio::Scan->scan($file);
    
    # Test find_frame doesn't leak
    Audio::Scan->find_frame( $file, 1000 );
    
    sleep 0.01;
}

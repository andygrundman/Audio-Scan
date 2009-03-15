package Audio::Scan;

use 5.008008;
use strict;

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('Audio::Scan', $VERSION);

sub scan {
    my ( $class, $path ) = @_;
    
    return $class->_scan( $path );
}

1;
__END__

=head1 NAME

Audio::Scan - Fast parsing of audio file information and tags

=head1 SYNOPSIS

  use Audio::Scan;
  
  my $data = Audio::Scan->scan('/path/to/file.mp3');

=head1 DESCRIPTION

=head1 THANKS

Some of the file format parsing code was derived from the mt-daapd project,
and adapted by Netgear.

=head1 AUTHOR

Andy Grundman, E<lt>andy@slimdevices.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009 by Andy Grundman

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

=cut

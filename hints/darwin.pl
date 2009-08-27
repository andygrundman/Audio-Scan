#!/usr/bin/perl

use Config;

if ( $Config{myarchname} =~ /i386/ ) {
    if ( $Config{version} =~ /^5\.10/ ) {
        # 5.10, build as 10.4+ with Snow Leopard 64-bit support
        $arch = "-arch i386 -arch x86_64 -arch ppc -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.4";
    }
    else {
        # 5.8.x, build for 10.3+ 32-bit universal
        $arch = "-arch i386 -arch ppc -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.3";
    }
    
    print "Adding $arch";

    $self->{CCFLAGS} = "$arch $Config{ccflags}";
    $self->{LDFLAGS} = "$arch -L/usr/lib $Config{ldflags}";
    $self->{LDDLFLAGS} = "$arch $Config{lddlflags}";
}

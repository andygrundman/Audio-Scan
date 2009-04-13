#!/usr/bin/perl

if ( $Config{myarchname} =~ /i386/ ) {
	$arch = "-arch i386 -arch ppc -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.3";
	print "Adding $archn";

	$self->{CCFLAGS} = "$arch $Config{ccflags}";
	$self->{LDFLAGS} = "$arch -L/usr/lib $Config{ldflags}";
	$self->{LDDLFLAGS} = "$arch $Config{lddlflags}";
}

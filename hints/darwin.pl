#!/usr/bin/perl

if ( $Config{myarchname} =~ /i386/ ) {
	$arch = "-arch i386 -arch ppc";
	print "Adding $archn";

	$self->{CCFLAGS} = "$arch $Config{ccflags}";
	$self->{LDFLAGS} = "$arch $Config{ldflags}";
	$self->{LDDLFLAGS} = "$arch $Config{lddlflags}";
}

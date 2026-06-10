#!/usr/bin/perl
# Copyright 2012 pooler@litecoinpool.org
#
# nomacro.pl - convert GAS .macro blocks to C preprocessor #define macros.
#
# Clang on macOS cannot assemble pooler's x86_64 .S files that rely on GAS
# macros (vpshufd immediates lose their $ prefix, etc.). Run this once per
# file before assembly on Apple x86_64 builds.
#
# Usage:
#   nomacro.pl input.S output.S

use strict;
use warnings;

my ($in, $out) = @ARGV;
die "usage: nomacro.pl input.S output.S\n" unless defined $in && defined $out;

open my $FIN, '<', $in or die "nomacro.pl: cannot read $in: $!\n";
open my $FOUT, '>', $out or die "nomacro.pl: cannot write $out: $!\n";

my $inmacro = 0;
my %macros = ();

while (<$FIN>) {
	if (m/^\.macro\s+([_0-9A-Z]+)(?:\s*)(.*)$/i) {
		print $FOUT "#define $1($2) \\\n";
		$macros{$1} = 1;
		$inmacro = 1;
		next;
	}
	if (m/^\.endm/) {
		print $FOUT "\n";
		$inmacro = 0;
		next;
	}
	for my $m (keys %macros) {
		s/^([ \t]*)\Q$m\E(?:[ \t]+([^#\n]*))?([;\n])/$1$m($2)$3/;
	}
	if ($inmacro) {
		if (m/^\s*#if/) {
			$_ = <$FIN> while (!m/^\s*#endif/);
			next;
		}
		next if (m/^\s*$/);
		s/\\//g;
		s/$/; \\/;
	}
	print $FOUT $_;
}

close $FOUT;
close $FIN;

#!/usr/bin/env perl

use strict;
use warnings;

sub extract_dependencies {
    my $conanfile = 'conanfile.txt';
    open my $fh, '<', $conanfile or die "Unable to open $conanfile: $!\n";
    my $in_requires = 0;
    my @deps;

    while (my $line = <$fh>) {
        if ($line =~ /^\[requires\]/) {
            $in_requires = 1;
            next;
        }

        $in_requires = 0 if $line =~ /^\[/;
        next unless $in_requires;

        $line =~ s/#.*$//;
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;
        next unless length $line;
        $line =~ s/\r//g;

        push @deps, $line;
    }

    close $fh;
    return @deps;
}

sub version_compare {
    my ($a, $b) = @_;
    my @a_parts = split /(\d+)/, $a;
    my @b_parts = split /(\d+)/, $b;

    while (@a_parts && @b_parts) {
        my $a_part = shift @a_parts;
        my $b_part = shift @b_parts;

        my $a_is_num = ($a_part // '') =~ /^\d+$/;
        my $b_is_num = ($b_part // '') =~ /^\d+$/;

        if ($a_is_num && $b_is_num) {
            my $cmp = $a_part <=> $b_part;
            return $cmp if $cmp;
        } else {
            $a_part //= '';
            $b_part //= '';
            my $cmp = $a_part cmp $b_part;
            return $cmp if $cmp;
        }
    }

    return @a_parts <=> @b_parts;
}

sub latest_version_from {
    my ($dep_name, $output) = @_;
    my @versions;

    for my $line (split /\n/, $output) {
        if ($line =~ /^\s*\Q$dep_name\E\/([0-9A-Za-z_.+\-]+)/) {
            push @versions, $1;
        }
    }

    return unless @versions;
    @versions = sort { version_compare($a, $b) } @versions;
    return $versions[-1];
}

sub check_dependency {
    my ($entry) = @_;
    my ($dep_name, $rest) = split m{/}, $entry, 2;

    unless (defined $rest && length $dep_name && length $rest) {
        print "⚠ Unable to parse dependency entry: $entry\n";
        return;
    }

    my ($current_version) = split /@/, $rest, 2;

    unless (defined $current_version && length $current_version) {
        print "⚠ Unable to determine current version for $entry\n";
        return;
    }

    my $output = `conan search $dep_name -r conancenter 2>/dev/null`;

    if ($? != 0) {
        printf "⚠ %s search failed (current %s)\n", $dep_name, $current_version;
        return;
    }

    my $latest_version = latest_version_from($dep_name, $output);

    unless (defined $latest_version) {
        printf "⚠ %s unable to determine latest version (current %s)\n", $dep_name, $current_version;
        return;
    }

    if ($current_version eq $latest_version) {
        printf "✓ %s is up to date (version %s)\n", $dep_name, $current_version;
    } else {
        printf "⚠ %s update available (%s → %s)\n", $dep_name, $current_version, $latest_version;
    }
}

print "Checking for Conan dependency updates...\n";
my @dependencies = extract_dependencies();

unless (@dependencies) {
    print "No dependencies found in conanfile.txt\n";
    exit 0;
}

for my $dep (@dependencies) {
    check_dependency($dep);
}

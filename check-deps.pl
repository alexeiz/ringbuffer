#!/usr/bin/env perl

use strict;
use warnings;
use version;
use feature 'say';

# configuration
my $CONAN_REMOTE = 'conancenter';
my $CONANFILE = 'conanfile.txt';

sub extract_dependencies {
    open my $fh, '<', $CONANFILE or die "Unable to open $CONANFILE: $!\n";

    my $in_requires = 0;
    my @deps;

    while (my $line = <$fh>) {
        if ($line =~ /^\[requires\]/) {
            $in_requires = 1;
            next;
        }

        $in_requires = 0 if $line =~ /^\[/;
        next unless $in_requires;

        chomp $line;
        $line =~ s/#.*$//;        # remove comments
        $line =~ s/^\s+|\s+$//g;  # trim whitespace
        next unless $line;

        push @deps, $line;
    }

    close $fh;
    return @deps;
}

sub version_compare {
    my ($a, $b) = @_;
    return eval { version->parse($a) <=> version->parse($b) } || ($a cmp $b);
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
        say "⚠ Unable to parse dependency entry: $entry";
        return;
    }

    my ($current_version) = split /@/, $rest, 2;

    unless (defined $current_version && length $current_version) {
        say "⚠ Unable to determine current version for $entry";
        return;
    }

    my $output = `conan search $dep_name -r $CONAN_REMOTE 2>&1`;

    if ($? != 0) {
        printf "⚠ %s search failed (current %s)\n", $dep_name, $current_version;
        say "\n--- Conan Output ---\n$output\n--- End Conan Output ---";
        return;
    }

    my $latest_version = latest_version_from($dep_name, $output);

    unless (defined $latest_version) {
        printf "⚠ %s unable to determine latest version (current %s)\n", $dep_name, $current_version;
        return;
    }

    if (version_compare($current_version, $latest_version) == 0) {
        printf "✓ %s is up to date (version %s)\n", $dep_name, $current_version;
    } else {
        printf "⚠ %s update available (%s → %s)\n", $dep_name, $current_version, $latest_version;
    }
}

say "Checking for Conan dependency updates...";
my @dependencies = extract_dependencies();

unless (@dependencies) {
    say "No dependencies found in $CONANFILE";
    exit 0;
}

for my $dep (@dependencies) {
    check_dependency($dep);
}

#!/usr/bin/env perl

use strict;
use warnings;
use version;
use feature 'say';
use Pod::Usage;

# Configuration
use constant {
    CONAN_REMOTE => 'conancenter',
    CONANFILE    => 'conanfile.txt',
};

sub extract_dependencies {
    open my $fh, '<', CONANFILE or die "Unable to open @{[CONANFILE]}: $!\n";

    my $in_requires = 0;
    my @deps;

    while (my $line = <$fh>) {
        chomp $line;

        # check for section headers
        if ($line =~ /^\[requires\]/) {
            $in_requires = 1;
            next;
        }

        $in_requires = 0 if $line =~ /^\[/;
        next unless $in_requires;

        $line =~ s/#.*$//;        # remove comments
        $line =~ s/^\s+|\s+$//g;  # trim whitespace
        next unless $line;        # skip empty lines

        push @deps, $line;
    }

    close $fh;
    return @deps;
}

sub latest_version_from {
    my ($dep_name, $output) = @_;
    my @versions;

    for my $line (split /\n/, $output) {
        if ($line =~ /^\s*\Q$dep_name\E\/([0-9A-Za-z_.+\-]+)/) {
            push @versions, version->parse($1);
        }
    }

    return unless @versions;
    @versions = sort { $a <=> $b } @versions;
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
    $current_version = version->parse($current_version);

    unless (defined $current_version && length $current_version) {
        say "⚠ Unable to determine current version for $entry";
        return;
    }

    my $output = `conan search $dep_name -r @{[CONAN_REMOTE]} 2>&1`;

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

    if ($current_version == $latest_version) {
        printf "✓ %s is up to date (version %s)\n", $dep_name, $current_version;
    } else {
        printf "⚠ %s update available (%s → %s)\n", $dep_name, $current_version, $latest_version;
    }
}

sub main {
    pod2usage(-verbose => 2, -exitval => 0) if @ARGV && $ARGV[0] eq '--help';

    say "Checking for Conan dependency updates...";

    my @dependencies = extract_dependencies();

    unless (@dependencies) {
        say "No dependencies found in @{[CONANFILE]}";
        exit 0;
    }

    for my $dep (@dependencies) {
        check_dependency($dep);
    }
}

main();

__END__

=head1 NAME

check-deps.pl - Check for updated Conan dependencies

=head1 SYNOPSIS

perl check-deps.pl [--help]

=head1 DESCRIPTION

This script checks the conanfile.txt for dependencies and compares their current
versions against the latest versions available in the Conan remote repository.
It reports which dependencies are up-to-date and which have updates available.

=head1 OPTIONS

=over 4

=item B<--help>

Display this help message and exit.

=back

=cut

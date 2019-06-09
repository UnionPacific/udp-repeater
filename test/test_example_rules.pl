#!/usr/bin/perl -w
use IO::Socket;
use strict;

# Tests for UDP Packet Repeater
# Based on the configuration included in conf/example_rules.json
#
# Written by Thomas Coe
# Union Pacific Railroad

{
    my $TEST_STRING1 = "1234ABCDEF";
    my $TEST_STRING2 = "ZYXW987654";

    my ($result1, $flags1, $result2, $flags2, $result3, $flags3);

    # Create bound sockets
    my $sock_2000 = IO::Socket::INET->new(
        Proto       => 'udp',
        LocalPort   => 2000,
        PeerPort    => 8001,
        PeerAddr    => 'localhost'
    ) or die "Could not create socket 2000: $!\n";
    my $sock_2001 = IO::Socket::INET->new(
        Proto       => 'udp',
        LocalPort   => 2001,
        PeerPort    => 8002,
        PeerAddr    => 'localhost'
    ) or die "Could not create socket 2001: $!\n";
    my $sock_9000 = IO::Socket::INET->new(
        Proto       => 'udp',
        LocalPort   => 9000
    ) or die "Could not create socket 9000: $!\n";
    my $sock_9001 = IO::Socket::INET->new(
        Proto       => 'udp',
        LocalPort   => 9001
    ) or die "Could not create socket 9001: $!\n";

    # Set 5 second timeout on sockets that are receiving
    $sock_9000->setsockopt(SOL_SOCKET, SO_RCVTIMEO, pack('L!L!', +5, 0)) or die "setsockopt: $!\n";
    $sock_9001->setsockopt(SOL_SOCKET, SO_RCVTIMEO, pack('L!L!', +5, 0)) or die "setsockopt: $!\n";

    ### TEST 1 ###
    # Send test string
    $sock_2000->send($TEST_STRING1) or die "Send error: $!\n";
    # Receive packet on port 9000 (length +1 to catch additional 'bad' characters sent)
    $sock_9000->recv($result1, length($TEST_STRING1) + 1, $flags1);
    if (length($result1) == 0) {
        print "TEST 1 FAILURE: Timeout while receiving on port 9000\n";
    } elsif (!($result1 eq $TEST_STRING1)) {
        print "TEST 1 FAILURE: Received \"$result1\" on port 9000 (expected \"$TEST_STRING1\")\n";
    } else {
        print "TEST 1 SUCCESS: Received \"$result1\"\n";
    }

    ### TESTS 2 and 3 ###
    # Send test string
    $sock_2001->send($TEST_STRING2) or die "Send error: $!\n";
    # Receive packet on port 9000 (length +1 to catch additional 'bad' characters sent)
    $sock_9000->recv($result2, length($TEST_STRING2) + 1, $flags2);
    if (length($result2) == 0) {
        print "TEST 2 FAILURE: Timeout while receiving on port 9000\n";
    } elsif (!($result2 eq $TEST_STRING2)) {
        print "TEST 2 FAILURE: Received \"$result2\" on port 9000 (expected \"$TEST_STRING2\")\n";
    } else {
        print "TEST 2 SUCCESS: Received \"$result2\"\n";
    }
    # Receive packet on port 9001 (length +1 to catch additional 'bad' characters sent)
    $sock_9001->recv($result3, length($TEST_STRING2) + 1, $flags3);
    if (length($result3) == 0) {
        print "TEST 3 FAILURE: Timeout while receiving on port 9001\n";
    } elsif (!($result3 eq $TEST_STRING2)) {
        print "TEST 3 FAILURE: Received \"$result3\" on port 9001 (expected \"$TEST_STRING2\")\n";
    } else {
        print "TEST 3 SUCCESS: Received \"$result3\"\n";
    }

}

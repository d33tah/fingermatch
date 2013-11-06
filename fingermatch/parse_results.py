#!/usr/bin/python

"""
Reads the uncompressed run-research.sh output from
the standard input, extract and count the perfect matches and print the
results to the standard output.

For a speedup, run this under pypy.
"""

import sys
import argparse

# for ip_to_u32
import socket
import struct

from fputils import print_stderr, percent_type

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--names', action='store_true',
                    help='group by names instead of line numbers')
parser.add_argument('--percentage', default=100, type=percent_type,
                    metavar='N',
                    help='minimum percentage needed to count the result')
args = parser.parse_args()

if sys.stdin.isatty():
  sys.exit("ERROR: %s: the script expects feeder.py output as its "
           "standard input." % sys.argv[0])

if args.names:
  # Create a dictionary where line number is a key and the fingerprint as the
  # value
  fingerprints = dict(enumerate(open('nmap-os-db').readlines()))


def ip_to_u32(ip):
    """
    Translate an IP address to little endian unsigned 32-bit integer. This way
    I could save some memory, storing the integer in the dictionary instead of
    the string.
    """
    return struct.unpack("<I", socket.inet_aton(ip))[0]

results = {}
ip_counts = {}
duplicates = 0
for line in sys.stdin:
  columns = line.rstrip("\r\n").split()
  if len(columns) < 3:
    matches_column = ''
    matches = []
  else:
    matches_column = columns[2]
    if matches_column == '?':
      matches = []
    else:
      matches = matches_column.split(',')

  ip = ip_to_u32(columns[0])
  checked_hash = hash(matches_column)
  if ip in ip_counts and ip_counts[ip] == checked_hash:
    duplicates += 1
    continue
  elif matches_column not in ['?', '']:
    ip_counts[ip] = checked_hash

  for match in matches:
    line_number, percentage = map(int, match.rstrip(']').split('['))
    if percentage < args.percentage:
      continue
    score = 1
    if args.names:
      fingerprint_name = fingerprints[line_number - 1]
      if fingerprint_name.find("Fingerprint") == -1:
        sys.exit(line)
      fingerprint_name = fingerprint_name[len("Fingerprint "):]
      fingerprint_name = fingerprint_name.rstrip("\r\n")
      key = fingerprint_name
    else:
      key = line_number
    if not key in results:
      results[key] = score
    else:
      results[key] += score

results = reversed(sorted(results.iteritems(), key=lambda k: k[1]))
print_stderr("%s duplicates found" % duplicates)

for fingerprint_line, num_devices in results:
  print("%s\t%s" % (num_devices, fingerprint_line))

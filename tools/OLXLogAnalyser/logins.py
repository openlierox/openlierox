#!/usr/bin/env python3

import sys, os, re

opts = [opt for opt in sys.argv[1:] if not os.path.exists(opt)]
files = [f for f in sys.argv[1:] if f not in opts]

def Stream(files):
	for fn in files:
		for l in open(fn, "r"):
			yield l.strip("\n")

reJoin = re.compile(r"^H: Worm joined: (?P<name>.*) \(id (?P<id>[0-9]+), from (?P<ip>[0-9.]+):[0-9]+\((?P<version>.*)\)\)$")

def Joins(stream):
	for l in stream:
		m = reJoin.match(l)
		if m: yield m.groupdict()

ips = dict()
for j in Joins(Stream(files)):
	ip = j["ip"]
	name = j["name"]
	if ip not in ips: ips[ip] = dict()  # names
	ips[ip][name] = j

print("Total IPs:", len(ips))

for i in range(2, 100):
	ipsfilter = [d for d in ips.values() if len(d) > i]
	print("IPs used by more than", i, "nicks:", len(ipsfilter))
	if len(ipsfilter) == 0: break

N = 4
print("IPs used by more than", N, "nicks:")
ipsfilter = [d for d in ips.items() if len(d[1]) > N]
for d in ipsfilter:
	#print(" ", d[0], ":")
	print("-------------")
	for name in d[1]:
		print("    ", name)

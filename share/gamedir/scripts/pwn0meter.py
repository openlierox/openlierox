#!/usr/bin/env python3 -u

import sys, time, os, random, traceback, re
from functools import cmp_to_key
from html import escape as html_escape

f = open("pwn0meter.txt","r")
w = open("pwn0meter.html","w")
#w = sys.stdout
w.write("<html><HEAD>\n")
w.write("<META HTTP-EQUIV=\"content-type\" CONTENT=\"text/html; charset=utf-8\">\n")
w.write("<TITLE>Pwn0meter</TITLE>\n")
w.write("</HEAD>\n<BODY>\n<H2>Pwn0meter</H2>\n")
w.write("<p>updated on %s</p>\n" % time.asctime())

# make random chat quotes
# doesn't matter if it fails, so surround by try/catch
try:
	# really hacky way to get latest logfile (assume that ls sorts by name)
	lastlogfile = "logs/" + os.popen("ls logs").read().splitlines()[-1]
	chatlogmark = "n: CHAT: "
	chatlines = os.popen("tail -n 10000 \"" + lastlogfile + "\" | grep \"" + chatlogmark + "\"").read().splitlines()

	chatstr = "<h3>Random chat quotes</h3><p>"
	rndstart = random.randint(0, len(chatlines) - 5)
	for i in range(rndstart, rndstart + 5):
		chatstr += html_escape(chatlines[i].replace(chatlogmark, "")) + "<br>"
	chatstr += "</p>"
	w.write(chatstr)

except Exception:
	print("Unexpected error:", traceback.format_exc())
	pass

killers = {}
deaders = {}
clan_killers = {}
clan_deaders = {}
clans = {}

re_clans = (
	re.compile(r"^\[\[\[(?P<clan>.+)\]\]\].+$"),
	re.compile(r"^\[(?P<clan>.+)\].+$"),
	re.compile(r"^.+\[(?P<clan>.+)\]$"),
	re.compile(r"^\((?P<clan>.+)\).+$"),
	re.compile(r"^.+\((?P<clan>.+)\)$"),
	re.compile(r"^-=(?P<clan>.+)=-.+$"),
	re.compile(r"^-(?P<clan>.+)-.+$"),
	re.compile(r"^\<(?P<clan>.+)\>.+$"),
	re.compile(r"^\{(?P<clan>.+)\}.+$"),
	re.compile(r"^.+\{(?P<clan>.+)\}$"),
	re.compile(r"^\|(?P<clan>.+)\|.+$"),
	re.compile(r"^.+\[(?P<clan>.+)\]$"),
	re.compile(r"^\|(?P<clan>.+)\|.+$"),
	re.compile(r"^.+\[(?P<clan>.+)\]$"),
	)

def clan_of(name):
	for m in [r.match(name) for r in re_clans]:
		if m:
			return m.group("clan")
	return "Clanfree"

for l in f.readlines():
	l = l.strip()
	if l == "":
		continue
	try:
		( time_, deader, killer ) = l.split("\t")
	except Exception:
		continue
	if killer.find("[CPU]") >= 0:
		continue
	if killer.find("The Third") >= 0:
		continue
	if killer.find("OpenLieroXor") >= 0:
		continue

	if not killer in killers:
		killers[killer] = {}
	if not deader in deaders:
		deaders[deader] = {}
	killers[killer][deader] = killers[killer].get(deader,0) + 1
	deaders[deader][killer] = deaders[deader].get(killer,0) + 1

	clankiller = clan_of(killer)
	clandeader = clan_of(deader)
	#if clankiller == clandeader: continue # ignore that

	if not clankiller in clan_killers:
		clan_killers[clankiller] = {}
	if not clandeader in clan_deaders:
		clan_deaders[clandeader] = {}
	clan_killers[clankiller][clandeader] = clan_killers[clankiller].get(clandeader,0) + 1
	clan_deaders[clandeader][clankiller] = clan_deaders[clandeader].get(clankiller,0) + 1

	if not clankiller in clans:
		clans[clankiller] = set()
	clans[clankiller].add(killer)

f.close()

#print(killers)


def printRanks(killers, deaders):
	def sortFunc(s1, s2):
		kills1 = sum(killers[s1].values()) - killers[s1].get(s1,0)
		kills2 = sum(killers[s2].values()) - killers[s2].get(s2,0)
		if kills1 < kills2: return 1
		if kills1 > kills2: return -1
		try:
			deaths1 = sum(deaders[s1].values())
		except Exception:
			deaths1 = 0
		try:
			deaths2 = sum(deaders[s2].values())
		except Exception:
			deaths2 = 0
		if deaths1 < deaths2: return -1
		if deaths1 > deaths2: return 1
		return 0

	sortedKillers = sorted(killers.keys(), key=cmp_to_key(sortFunc))

	i = 1
	for k in sortedKillers:
		kills = sum(killers[k].values())
		try:
			deaths = sum(deaders[k].values())
		except Exception:
			deaths = 0
		suicides = killers[k].get(k,0)
		kills -= suicides
		deaths -= suicides
		w.write("%i. <B>%s</B>: %i kills %i deaths %i suicides, killed:" %
			( i, html_escape(k), kills, deaths, suicides ))
		# Ugly killer sorting
		killedMax = {}
		for f_ in killers[k]:
			if not killers[k][f_] in killedMax:
				killedMax[killers[k][f_]] = []
			if killedMax[killers[k][f_]].count(f_) == 0:
				killedMax[killers[k][f_]].append(f_)
		killedMax1 = sorted(killedMax.keys(), reverse=True)
		count = 0
		for f_ in killedMax1:
			for f1 in killedMax[f_]:
				if f1 == k: # Don't write suicides
					continue
				count += 1
				if count >= 5:
					break
				if count != 1:
					w.write(",")
				w.write(" %s - %i" % ( html_escape(f1), f_ ) )
		w.write("<BR>\n")
		i += 1

w.write("<a href=\"#players\">go directly to player ranks</a>")
w.write("<h2>Clans</h2>\n")
printRanks(clan_killers, clan_deaders)

w.write("<h2>Players</h2>")
w.write("<a name=\"players\"></a>\n")
printRanks(killers, deaders)

w.write("<h2>Clan members</h2>\n")
for c in clans.keys():
	if c == "Clanfree": continue # ignore
	w.write("<b>%s</b>: " % html_escape(c))
	w.write("%s<br>\n" % html_escape(", ".join(list(clans[c]))))

w.write("</BODY>\n</html>\n")
w.close()

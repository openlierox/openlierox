from functools import cmp_to_key

import dedicated_control_io as io


def ParseRank(useRatios=True):
	#io.messageLog("ParseRank: Opening pwn0meter.txt", io.LOG_INFO)
	try:
		f = open(io.getWriteFullFileName("pwn0meter.txt"), "r")
	except IOError:
		return {}
	l = f.readline()
	killers = {}
	deaders = {}
	while l != "":
		l = l.strip()
		#io.messageLog("ParseRank: line: " + l, io.LOG_INFO)
		if not (l.count("\t") == 2):  # Don't show empty names or empty lines !
			l = f.readline()
			continue
		(date, deader, killer) = l.split("\t")

		killerOk = killer != "OpenLieroXor"
		if killer not in killers and killerOk:
			killers[killer] = []
		if deader not in deaders:
			deaders[deader] = []
		if killerOk:
			killers[killer].append(deader)
		deaders[deader].append(killer)
		l = f.readline()
	f.close()

	def sortFunc(s1, s2):
		suicides1 = killers[s1].count(s1)
		suicides2 = killers[s2].count(s2)
		kills1 = float(len(killers[s1]) - suicides1)  # kills - suicides
		kills2 = float(len(killers[s2]) - suicides2)
		try:
			deaths1 = len(deaders[s1])
		except KeyError:
			deaths1 = 0
		try:
			deaths2 = len(deaders[s2])
		except KeyError:
			deaths2 = 0
		if useRatios:  # You can change this to have the ratio you want... Here 2/3 is number of kill, and 1/3 is kills/deaths
			kills1 = kills1 / (deaths1 + 1 + suicides1) * kills1 * kills1
			kills2 = kills2 / (deaths2 + 1 + suicides2) * kills2 * kills2
		if kills1 < kills2: return 1
		if kills1 > kills2: return -1
		if deaths1 < deaths2: return -1
		if deaths1 > deaths2: return 1
		return 0

	sortedKillers = sorted(killers.keys(), key=cmp_to_key(sortFunc))
	rank = 0
	total = {}
	for k in sortedKillers:
		rank += 1
		kills = len(killers[k])
		try:
			deaths = len(deaders[k])
		except KeyError:
			deaths = 0
		suicides = killers[k].count(k)
		kills -= suicides
		deaths -= suicides
		total[k] = [kills, deaths, suicides, rank]
	#io.messageLog("ParseRank: rank " + str(total), io.LOG_INFO)
	return total


def firstRank(wormid):
	global rank
	# TODO: too lazy to do cycle
	try:
		for k in rank:
			if rank[k][3] == 1: w1 = k
			elif rank[k][3] == 2: w2 = k
			elif rank[k][3] == 3: w3 = k
			elif rank[k][3] == 4: w4 = k
			elif rank[k][3] == 5: w5 = k
		io.privateMsg(wormid, "The best players are :")
		io.privateMsg(wormid, "1) " + w1 + " (" + str(rank[w1][0]) + " kills, " + str(rank[w1][1]) + " deaths, " + str(rank[w1][2]) + " suicides)")
		io.privateMsg(wormid, "2) " + w2 + " (" + str(rank[w2][0]) + " kills, " + str(rank[w2][1]) + " deaths, " + str(rank[w2][2]) + " suicides)")
		io.privateMsg(wormid, "3) " + w3 + " (" + str(rank[w3][0]) + " kills, " + str(rank[w3][1]) + " deaths, " + str(rank[w3][2]) + " suicides)")
		io.privateMsg(wormid, "4) " + w4 + " (" + str(rank[w4][0]) + " kills, " + str(rank[w4][1]) + " deaths, " + str(rank[w4][2]) + " suicides)")
		io.privateMsg(wormid, "5) " + w5 + " (" + str(rank[w5][0]) + " kills, " + str(rank[w5][1]) + " deaths, " + str(rank[w5][2]) + " suicides)")
	except Exception:
		pass


def rankSingle(wormName, wormid):
	k = wormName
	io.privateMsg(wormid, str(rank[k][3]) + ") " + k + " (" + str(rank[k][0]) + " kills, " + str(rank[k][1]) + " deaths, " + str(rank[k][2]) + " suicides)")


def myRank(wormName, wormid):
	global rank
	try:
		rankPos = rank[wormName][3]
		rankNames = [False, False, False, False]
		for k in rank:
			if rank[k][3] >= rankPos - 2 and rank[k][3] <= rankPos + 1:
				rankNames[rank[k][3] - rankPos + 2] = k
		for k in rankNames:
			if k:
				rankSingle(k, wormid)
	except KeyError:
		io.privateMsg(wormid, wormName + " has not played yet")


def refreshRank(useRatios=True):
	global rank

	def sortFunc(s1, s2):
		kills1 = float(rank[s1][0])  # kills
		kills2 = float(rank[s2][0])
		deaths1 = rank[s1][1]
		deaths2 = rank[s2][1]
		if useRatios:  # You can change this to have the ratio you want... Here 2/3 is number of kill, and 1/3 is kills/deaths
			kills1 = kills1 / (deaths1 + 1 + rank[s1][2]) * kills1 * kills1  # rank[s1][2] = suicides
			kills2 = kills2 / (deaths2 + 1 + rank[s2][2]) * kills2 * kills2
		if kills1 < kills2: return 1
		if kills1 > kills2: return -1
		if deaths1 < deaths2: return -1
		if deaths1 > deaths2: return 1
		return 0

	sortedRank = sorted(rank.keys(), key=cmp_to_key(sortFunc))
	oldrank = rank
	rank = {}
	count = 0
	for k in sortedRank:
		count += 1
		rank[k] = [oldrank[k][0], oldrank[k][1], oldrank[k][2], count]


def ParseAuthInfo():
	try:
		f = open(io.getWriteFullFileName("pwn0meter_auth.txt"), "r")
	except IOError:
		return {}

	authInfo = {}

	l = f.readline()
	while l != "":
		l = l.strip()
		if not (l.count("\t") == 1):
			l = f.readline()
			continue
		(worm, auth) = l.split("\t")
		auth = auth.split(" ")
		authInfo[worm] = (int(auth[0]), " ".join(auth[1:]).lower())
		l = f.readline()
	f.close()
	return authInfo


rank = ParseRank()
auth = ParseAuthInfo()

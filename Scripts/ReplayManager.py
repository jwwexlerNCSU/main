#!/usr/bin/python
import os
import time
import OctetList
from UCNAUtils import *
from optparse import OptionParser	
	
def processOctets(sim=False):
	pcmd = "cd ~/mpmAnalyzer/Source/; ./UCNAnalyzer pr oct %i x x\n"
	freplaylist = open("oct_replaylist.txt","w")
	for r in range(60):
		if sim:
			freplaylist.write(pcmd%(-r-1));
		else:
			freplaylist.write(pcmd%r);
	freplaylist.close()
	os.system("cat oct_replaylist.txt")
	if sim:
		os.system("parallel -P 3 --nice 10 < oct_replaylist.txt")
	else:
		os.system("parallel --nice 10 < oct_replaylist.txt")
	os.system("rm oct_replaylist.txt")
	if sim:
		os.system("cd ~/mpmAnalyzer/Source/; ./UCNAnalyzer pr oct -1000 x x\n");
	else:
		os.system("cd ~/mpmAnalyzer/Source/; ./UCNAnalyzer pr oct 1000 x x\n");
	

def processSources(rmin,rmax):
		pcmd = "cd ~/mpmAnalyzer/Source/; ./UCNAnalyzer sr %i %i x\n"
		freplaylist = open("source_replaylist.txt","w")
		for rg in sourcegroups:
			for r in range(rg[0],rg[1]+1):
				if rmin<=r<=rmax:
					freplaylist.write(pcmd%(r,r))
		freplaylist.close()
		os.system("cat source_replaylist.txt")
		os.system("parallel --nice 10 < source_replaylist.txt")
		os.system("rm source_replaylist.txt")

			
	
if __name__ == "__main__":
	
	parser = OptionParser()
	parser.add_option("-k", "--kill", dest="kill", action="store_true", default=False, help="kill running replays")
	parser.add_option("-o", "--octs", dest="octs", action="store_true", default=False, help="process octets")
	parser.add_option("-z", "--simocts", dest="simocts", action="store_true", default=False, help="re-simulate octets")
	parser.add_option("-s", "--sources", dest="sources", action="store_true", default=False, help="process (official) source data")
	parser.add_option("--rmin", type="int", dest="rmin", default=0)
	parser.add_option("--rmax", type="int", dest="rmax", default=100000)
	
	options, args = parser.parse_args()
	if options.kill:
		os.system("killall -9 parallel")
		os.system("killall -9 UCNAnalyzer")
		os.system("killall -9 ReplayManager.py")
		exit(0)
	
	if len(os.popen("ps -a | grep ReplayManager").readlines()) > 1:
		print "Already running! I die!"
		exit(1)
		
	if options.octs:
		processOctets(False)
		
	if options.simocts:
		processOctets(True)
		
	if options.sources:
		processSources(options.rmin,options.rmax)
		exit(0)
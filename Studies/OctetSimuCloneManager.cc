#include "OctetSimuCloneManager.hh"
#include "PathUtils.hh"

OctetSimuCloneManager::OctetSimuCloneManager(const std::string& dname, const std::string& bdir):
outputDir(dname), baseDir(bdir), doPlots(false), simFactor(1.0), nTot(0), stride(0), ownSimData(false), simData(NULL) {
	RunAccumulator::processedLocation = baseDir+"/"+outputDir+"/"+outputDir;
}

void OctetSimuCloneManager::scanOct(RunAccumulator& RA, const Octet& oct) {
	if(!oct.getNRuns()) return;
	RunAccumulator* octRA = (RunAccumulator*)RA.makeAnalyzer(oct.octName(),"");
	processOctets(*octRA,oct.getSubdivs(nextDiv(oct.divlevel),false),0*24*3600, doPlots);
	delete octRA;
}

void OctetSimuCloneManager::scanOct(RunAccumulator& RA, unsigned int octn) {
		Octet oct = Octet::loadOctet(QFile(getEnvSafe("UCNA_OCTET_LIST")),octn);
		scanOct(RA,oct);
}

void OctetSimuCloneManager::combineOcts(RunAccumulator& RA) {
	RA.mergeOcts(Octet::loadOctets(QFile(getEnvSafe("UCNA_OCTET_LIST"))));
}

unsigned int OctetSimuCloneManager::recalcAllOctets(RunAccumulator& RA, bool doPlots) {
	return recalcOctets(RA, Octet::loadOctets(QFile(getEnvSafe("UCNA_OCTET_LIST"))), doPlots);
}

void OctetSimuCloneManager::setSimData(Sim2PMT* s2p) {
	if(simData && ownSimData) delete simData;
	ownSimData=false;
	simData = s2p;
}

void OctetSimuCloneManager::setOctetSimdata(unsigned int octn) {
	if(simData && ownSimData) delete simData;
	ownSimData = true;
	simData = new G4toPMT();
	for(unsigned int i=0; i<stride; i++)
		simData->addFile(simFile+itos((stride*octn+i)%nTot)+".root");
	simData->PGen[EAST].xscatter = simData->PGen[WEST].xscatter = 0.005;
}

void OctetSimuCloneManager::simOct(RunAccumulator& SimRA, const Octet& oct) {
	if(!oct.getNRuns()) return;
	assert(simData);
	RunAccumulator* octSim = (RunAccumulator*)SimRA.makeAnalyzer(oct.octName(),"");
	octSim->simuClone(getEnvSafe("UCNA_ANA_PLOTS")+"/"+outputDir+"/"+oct.octName(), *simData, simFactor, 0.*3600, doPlots);
	delete octSim;
}

void OctetSimuCloneManager::simOct(RunAccumulator& SimRA, unsigned int octn) {
		Octet oct = Octet::loadOctet(QFile(getEnvSafe("UCNA_OCTET_LIST")),octn);
		setOctetSimdata(octn);
		simOct(SimRA,oct);
}

void OctetSimuCloneManager::combineSims(RunAccumulator& SimRA, RunAccumulator* OrigRA) {
	SimRA.mergeSims(getEnvSafe("UCNA_ANA_PLOTS")+"/"+outputDir, OrigRA);
}
#include "PlotMakers.hh"
#include <TH2F.h>
#include <TProfile.h>
#include <TGraphErrors.h>
#include <TStyle.h>
#include <TRandom.h>
#include <map>
#include <cassert>
#include <vector>
#include "GraphicsUtils.hh"
#include "QFile.hh"
#include "PathUtils.hh"
#include "ReSource.hh"
#include "Types.hh"
#include "KurieFitter.hh"
#include "G4toPMT.hh"
#include "BetaSpectrum.hh"
#include "LinHistCombo.hh"
#include "PostOfficialAnalyzer.hh"
#include "SimAsymmetryAnalyzer.hh"
#include <TColor.h>


TRandom3 plRndSrc;

void plotGMScorrections(const std::vector<RunNum>& runs, const std::string& foutPath) {
	QFile fout(foutPath+"/GMS_Plot.txt",false);
	for(unsigned int i=0; i<runs.size(); i++) {
		PMTCalibrator LC(runs[i]);
		Stringmap m = LC.calSummary();
		printf("----- %i -----\n",runs[i]);
		m.display();
		fout.insert("gmsinfo",m);
	}
	fout.commit();
}

void dumpPosmap(std::string basepath, unsigned int pnum) {
	
	PositioningCorrector* PCor;
	PMTCalibrator* PCal = NULL;
	
	if(pnum < 5000) {
		PCor = CalDBSQL::getCDB()->getPositioningCorrectorByID(pnum);
	} else {
		PCal = new PMTCalibrator(pnum);
		PCor = CalDBSQL::getCDB()->getPositioningCorrector(pnum);
	}
	
	if(!PCor) {
		printf("***ERROR*** Position map %i not found.\n",pnum);
		return;
	}
	
	makePath(basepath);
	QFile fout(basepath+"/Posmap_"+itos(pnum)+".txt",false);
	
	for(Side s = EAST; s<=WEST; ++s) {
		for(int t=0; t<=nBetaTubes; t++) {
			
			if(t==nBetaTubes && !PCal)
				continue;
			
			SectorCutter& psects = PCor->getSectors(s,0);
			
			for(unsigned int n=0; n<psects.nSectors(); n++) {
				
				float x,y;
				psects.sectorCenter(n,x,y);
				
				Stringmap m;
				m.insert("side",ctos(sideNames(s)));
				m.insert("tube",itos(t));
				m.insert("sector",n);
				m.insert("x",x);
				m.insert("y",y);
				if(t<nBetaTubes)
					m.insert("light",PCor->eval(s,t,x,y,true)); 
				else
					m.insert("light",PCal->nPE(s, nBetaTubes, 1000, x, y, 0));
				m.insert("energy",1.0);
				
				fout.insert("PosmapPoint",m);
			}
		}
		
	}
	fout.commit();
}


void SimSpectrumInfo(Sim2PMT& S, OutputManager& OM) {
	double emax = 1000;
	double nsegs = 100;
	double nbins = nsegs*5;
	
	// set up histograms
	std::vector<TH1*> hEnergy[2][TYPE_III_EVENT][2];	// energy [side][type][vis/true] for each input bin
	TProfile* inputEnergy[2][TYPE_III_EVENT][2];		// input bins [side][type][vis/true]
	TProfile* inputTotal = (TProfile*)OM.addObject(new TProfile("inputTotal","Input Visible Energy Total",nsegs,0,emax));
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int t = TYPE_0_EVENT; t <= TYPE_II_EVENT; t++) {
			inputEnergy[s][t][false] = (TProfile*)OM.addObject(new TProfile((sideSubst("inputEvis_%c",s)+itos(t)).c_str(),"Input Visible Energy",nsegs,0,emax));
			inputEnergy[s][t][true] = (TProfile*)OM.addObject(new TProfile((sideSubst("inputEtrue_%c",s)+itos(t)).c_str(),"Input True Energy",nsegs,0,emax));
			for(unsigned int i=0; i<nsegs; i++) {
				hEnergy[s][t][false].push_back(OM.registeredTH1F(sideSubst("hEvis_%c",s)+itos(t)+"_"+itos(i),"Visible Energy",nbins,0,emax));
				hEnergy[s][t][true].push_back(OM.registeredTH1F(sideSubst("hEtrue_%c",s)+itos(t)+"_"+itos(i),"True Energy",nbins,0,emax));
				hEnergy[s][t][false].back()->GetXaxis()->SetTitle("Energy [keV]");
				hEnergy[s][t][true].back()->GetXaxis()->SetTitle("Energy [keV]");
				hEnergy[s][t][true].back()->GetYaxis()->SetTitle("Counts");
				hEnergy[s][t][false].back()->GetYaxis()->SetTitle("Counts");
			}
		}
	}
	
	// scan data
	S.startScan();
	while(S.nextPoint()) {
		inputTotal->Fill(S.ePrim,S.ePrim);
		if(S.fPID != PID_BETA || S.fType > TYPE_II_EVENT || S.radius(S.fSide)>50.)
			continue;
		inputEnergy[S.fSide][S.fType][true]->Fill(S.ePrim,S.ePrim);
		int trueSeg = inputEnergy[S.fSide][S.fType][true]->FindBin(S.ePrim)-1;
		inputEnergy[S.fSide][S.fType][false]->Fill(S.getEnergy(),S.getEnergy());
		int visSeg = inputEnergy[S.fSide][S.fType][false]->FindBin(S.getEnergy())-1;
		if(0 <= trueSeg && trueSeg < nsegs)
			hEnergy[S.fSide][S.fType][false][trueSeg]->Fill(S.getEnergy());
		if(0 <= visSeg && visSeg < nsegs)
			hEnergy[S.fSide][S.fType][true][visSeg]->Fill(S.ePrim);
	}
	
	// collect histogram data
	for(unsigned int i=0; i<nsegs; i++) {
		for(Side s = EAST; s <= WEST; ++s) {
			for(unsigned int t = TYPE_0_EVENT; t <= TYPE_II_EVENT; t++) {
				Stringmap m;
				
				m.insert("segment",i);
				m.insert("side",sideSubst("%s",s));
				m.insert("type",t);
				
				m.insert("etrue_input",inputEnergy[s][t][true]->GetBinContent(i+1));
				m.insert("etrue_input_w",inputEnergy[s][t][true]->GetBinError(i+1));
				m.insert("etrue_input_n",inputEnergy[s][t][true]->GetBinEntries(i+1));
				m.insert("etrue_avg",hEnergy[s][t][true][i]->GetMean());
				m.insert("etrue_rms",hEnergy[s][t][true][i]->GetRMS());
				m.insert("etrue_counts",hEnergy[s][t][true][i]->Integral());
				
				m.insert("evis_input",inputEnergy[s][t][false]->GetBinContent(i+1));
				m.insert("evis_input_w",inputEnergy[s][t][false]->GetBinError(i+1));
				m.insert("evis_avg",hEnergy[s][t][false][i]->GetMean());
				m.insert("evis_rms",hEnergy[s][t][false][i]->GetRMS());
				m.insert("evis_counts",hEnergy[s][t][false][i]->Integral());
				
				m.insert("total_counts",inputTotal->GetBinEntries(i+1));
				
				OM.qOut.insert("spectrumInfo",m);
			}
		}
	}
	
	// plots
	OM.defaultCanvas->SetLogy(false);
	OM.defaultCanvas->cd();
	gStyle->SetOptStat("");
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int t = TYPE_0_EVENT; t <= TYPE_II_EVENT; t++) {
			for(unsigned int n=0; n<=1; n++) {
				for(unsigned int i=0; i<nsegs; i++) {
					hEnergy[s][t][n][i]->Scale(1.0/hEnergy[s][t][n][i]->GetBinWidth(1));
					hEnergy[s][t][n][i]->SetLineColor(2+(i%6));
				}
				double hmax = drawSimulHistos(hEnergy[s][t][n]);
				TH1D* hCounts = inputEnergy[s][t][1-n]->ProjectionX("_px","B");
				hCounts->Scale(hmax/hCounts->GetMaximum());
				hCounts->SetLineWidth(3);
				hCounts->Draw("SAME");
				OM.printCanvas(sideSubst("Energies_%c_Type_",s)+itos(t)+"_"+itos(n));
			}
		}
	}
}

void makeCorrectionsFile(int A, int Z, double Endpt) {
	
	std::string fout = getEnvSafe("UCNA_ANA_PLOTS")+"/SpectrumCorrection/SpectrumCorrection_";
	fout += itos(A)+"_"+itos(Z)+"_"+dtos(Endpt)+".txt";
	
	double R = pow(A,1./3.)*neutron_R0;
	double W0 = (Endpt+m_e)/m_e;
	double M0 = fabs(Z)*proton_M0+(A-fabs(Z))*neutron_M0;
	
	QFile Q;
	Stringmap sm;
	sm.insert("A",A);
	sm.insert("Z",Z);
	sm.insert("endpt",Endpt);
	sm.insert("W0",W0);
	sm.insert("R",R);
	sm.insert("M0",M0);
	Q.insert("decayInfo",sm);
	
	for(double e = 0.5; e < Endpt; e+=1.) {
		Stringmap m;
		double W = e/m_e+1.;
		m.insert("energy",e);
		m.insert("W",W);
		m.insert("F0m1",WilkinsonF0(Z,W,R)-1.0);
		m.insert("L0m1",WilkinsonL0(Z,W,R)-1.0);
		m.insert("RVm1",WilkinsonRV(W,W0,M0)-1.0);
		m.insert("RAm1",WilkinsonRA(W,W0,M0)-1.0);
		m.insert("BiRWM",Bilenkii_1958_11(W));
		m.insert("VCm1",WilkinsonVC(Z,W,W0,R)-1.0);
		m.insert("ACm1",WilkinsonAC(Z,W,W0,R)-1.0);
		m.insert("Qm1",WilkinsonQ(Z,W,W0,M0)-1.0);
		m.insert("g",Wilkinson_g(W,W0));
		m.insert("hmg",shann_h_minus_g(W,W0));
		m.insert("RWM",WilkinsonACorrection(W));
		m.insert("S0",plainPhaseSpace(W,W0));
		m.insert("S",correctedBetaSpectrum(e,A,Z,Endpt));
		m.insert("dSm1",spectrumCorrectionFactor(e,A,Z,Endpt)-1.0);
		m.insert("A0",plainAsymmetry(e,0.5));
		m.insert("A",correctedAsymmetry(e,0.5));
		m.insert("dAm1",asymmetryCorrectionFactor(e)-1);
		Q.insert("spectrumPoint",m);
	}
	Q.commit(fout);
}


//-------------------------------------------------------------//

PosPlotter::PosPlotter(OutputManager* O): rscale(1.0), nbin(100), OM(O) {
	gStyle->SetOptStat("");
	OM->defaultCanvas->SetRightMargin(0.125);
	OM->defaultCanvas->SetBottomMargin(0.125);
}

void PosPlotter::startScan(TH2* h) {
	hCurrent = h;
	nx=ny=0;
	x=y=1.e6;
}

bool PosPlotter::nextPoint() {
	do {
		nx = (nx%hCurrent->GetNbinsX())+1;
		ny += (nx==1);
		x = hCurrent->GetXaxis()->GetBinCenter(nx);
		y = hCurrent->GetYaxis()->GetBinCenter(ny);
	} while(x*x+y*y > r0*r0*rscale && ny <= hCurrent->GetNbinsY());
	return ny <= hCurrent->GetNbinsY();
}

TH2F* PosPlotter::makeHisto(const std::string& nm, const std::string& title) {
	TH2F* interpogrid = OM->registeredTH2F(nm,title,nbin,-60,60,nbin,-60,60);
	interpogrid->GetXaxis()->SetTitle("x Position [mm]");
	interpogrid->GetYaxis()->SetTitle("y Position [mm]");
	return interpogrid;
}

void PosPlotter::npePlot(PMTCalibrator* PCal) {
	
	SectorCutter& Sects = PCal->P->getSectors(EAST,0);
	r0 = Sects.r;
	
	float e0 = 1000;
	float s0 = 0.5;
	
	for(Side s = EAST; s<=WEST; ++s) {
		for(unsigned int t=0; t<=nBetaTubes; t++) {
			
			TH2F* interpogrid = makeHisto(sideSubst("%c",s)+itos(t+1)+"_nPE",
										  (t==nBetaTubes?sideSubst("%s",s):sideSubst("%c",s)+itos(t+1))+" PE per MeV");
			interpogrid->SetAxisRange(0,250,"Z");
			if(t==nBetaTubes)
				interpogrid->SetAxisRange(0,500,"Z");
			
			startScan(interpogrid);
			while(nextPoint())
				interpogrid->SetBinContent(nx,ny,PCal->nPE(s,t,e0*s0,x,y,0)/s0);
			
			interpogrid->Draw("COL Z");
			//drawSectors(Sects,6);
			OM->printCanvas(sideSubst("nPE_%c",s)+itos(t));			
		}
	}
}

void PosPlotter::npeGradPlot(PMTCalibrator* PCal) {
	
	SectorCutter& Sects = PCal->P->getSectors(EAST,0);
	r0 = Sects.r;
	
	for(Side s = EAST; s<=WEST; ++s) {
		for(int t=0; t<=nBetaTubes; t++) {
			TH2F* interpogrid = makeHisto(sideSubst("%c",s)+itos(t+1)+"_Grad",
										  (t==nBetaTubes?sideSubst("%s",s):sideSubst("%c",s)+itos(t+1))+" Light Transport Gradient");
			interpogrid->SetAxisRange(0,10,"Z");
			
			float gradsum = 0;
			float nn = 0;
			startScan(interpogrid);
			while(nextPoint()) {
				float e0 = PCal->nPE(s,t,500,x,y,0);
				float e1 = PCal->nPE(s,t,500,x+0.01,y,0);
				float e2 = PCal->nPE(s,t,500,x,y+0.01,0);
				float gd = sqrt( (e1-e0)*(e1-e0)+(e2-e0)*(e2-e0) )/0.01/e0;
				gradsum += gd*gd;
				nn += 1.0;
				interpogrid->SetBinContent(nx,ny,100.0*gd);
			}
			gradsum = sqrt(gradsum/nn);
			printf("**** %c%i RMS gradient = %g 1/mm ****\n",sideNames(s),t,gradsum);
			interpogrid->Draw("COL Z");
			drawSectors(Sects,6);
			OM->printCanvas(sideSubst("Posmap_Grad_%c",s)+itos(t));
		}
	}
}

void PosPlotter::etaPlot(PositioningCorrector* P, double axisRange) {
	
	SectorCutter& Sects = P->getSectors(EAST,0);
	r0 = Sects.r;
	
	for(Side s = EAST; s<=WEST; ++s) {
		for(unsigned int t=0; t<nBetaTubes; t++) {
			
			if(!P->eval(s, t, 0, 0))
				continue;
			
			TH2F* interpogrid = makeHisto(sideSubst("%c ",s)+itos(t+1), sideSubst("%s PMT ",s)+itos(t+1)+" Light Transport");
			interpogrid->SetAxisRange(0,axisRange,"Z");
			
			startScan(interpogrid);
			while(nextPoint())
				interpogrid->SetBinContent(nx,ny,P->eval(s, t, x, y, true));
			
			interpogrid->Draw("COL Z");
			drawSectors(Sects,6);
			OM->printCanvas(sideSubst("Posmap_%c_",s)+itos(t));
		}
	}
}

//-------------------------------------------------------------//

void showSimSpectrum(const std::string& nm, OutputManager& OM, NucDecayLibrary& NDL, PMTCalibrator& PCal) {
	double emax = 1000;
	int nbins = 1000;
	NucDecaySystem& NDS = NDL.getGenerator(nm);
	NDS.display(true);
	
	TH1F* hSpec = OM.registeredTH1F("hSpec","",nbins,0,emax);
	std::vector<NucDecayEvent> v;
	for(unsigned int i=0; i<1e7; i++) {
		v.clear();
		NDS.genDecayChain(v);
		for(std::vector<NucDecayEvent>::iterator it = v.begin(); it < v.end(); it++)
			if(it->d == D_ELECTRON)
				hSpec->Fill(it->E);
	}
	OM.defaultCanvas->SetLogy(true);
	hSpec->Draw();
	OM.printCanvas(nm+"_GenSpectrum");
	
	std::string g4dat = "/home/mmendenhall/geant4/output/WideKev_";
	G4toPMT g2p;
	g2p.addFile(g4dat + nm + "/analyzed_*.root");
	
	if(!g2p.getnFiles()) return;
	
	g2p.setCalibrator(PCal);
	TH1F* hSim = OM.registeredTH1F("hSim","",240,0,1200);
	hSim->GetXaxis()->SetTitle("Energy [keV]");
	g2p.startScan();
	while(g2p.nextPoint() && g2p.nCounted < 1.e6)
		if(g2p.fPID == PID_BETA)
			hSim->Fill(g2p.getEtrue());
	hSim->Scale(1.0/hSim->GetMaximum());
	hSim->Draw();
	OM.defaultCanvas->SetLogy(false);
	OM.printCanvas(nm+"_SimSpectrum");
}

void compareXenonSpectra() {
	
	std::string isotlist[] = {
		"Xe125_1-2+",	"Xe127_1-2+",	"Xe129_11-2-",
		"Xe131_11-2-",	"Xe133_3-2+",	"Xe133_11-2-",
		"Xe135_3-2+",	"Xe135_11-2-",	"Xe137_7-2-"};
	std::vector<std::string> isots(isotlist,isotlist+9);
	
	OutputManager OM("XeIsots",getEnvSafe("UCNA_ANA_PLOTS")+"/test/XeIsots");
	NucDecayLibrary NDL(getEnvSafe("UCNA_AUX")+"/NuclearDecays",1e-6);
	NDL.BEL.display();
	
	PMTCalibrator PCal(16000);
	gStyle->SetOptStat("");
	
	for(unsigned int n=0; n<isots.size(); n++) {
		printf("\n\n---------------------- %s ---------------------------\n",isots[n].c_str());
		showSimSpectrum(isots[n],OM,NDL,PCal);
	}
	
	OM.setWriteRoot(true);
	OM.write();
}

void decomposeXenon(RunNum rn, bool includeFast) {
	
	gStyle->SetOptStat("");
	
	OutputManager OM("XeDecomp_"+itos(rn),getEnvSafe("UCNA_ANA_PLOTS")+"/test/XeDecomp");
	NucDecayLibrary NDL(getEnvSafe("UCNA_AUX")+"/NuclearDecays",1e-6);
	
	std::vector<std::string> isots;
	isots.push_back("Xe125_1-2+");
	//isots.push_back("Xe127_1-2+");	// unlikely 36d HL, rare parent
	isots.push_back("Xe129_11-2-");
	isots.push_back("Xe131_11-2-");
	isots.push_back("Xe133_3-2+");
	isots.push_back("Xe133_11-2-");
	isots.push_back("Xe135_3-2+");
	if(includeFast) {
		isots.push_back("Xe135_11-2-");
		isots.push_back("Xe137_7-2-");
	}
	
	double emax = 1200;
	int nbins = 240;
	double fidrad = 50.;
	
	// fill true energy spectrum
	TH1F* hSpec = OM.registeredTH1F("hSpec","Data Spectrum",nbins,0,emax);
	hSpec->Sumw2();
	hSpec->SetLineColor(2);
	PostOfficialAnalyzer POA(true);
	POA.addRun(rn);
	POA.startScan();
	while(POA.nextPoint()) {
		Side s = POA.fSide;
		if(POA.fPID == PID_BETA && POA.fType <= TYPE_III_EVENT && (s==EAST || s==WEST) && POA.radius(s)<fidrad)
			hSpec->Fill(POA.getEtrue());
	}
	
	SectorCutter SC(5,fidrad);
	
	// simulate each isotope
	std::vector<TH1F*> hIsot;
	LinHistCombo LHC;
	for(unsigned int i=0; i<isots.size(); i++) {
		std::string g4dat = "/home/mmendenhall/geant4/output/WideKev_";
		G4SegmentMultiplier g2p(SC);
		g2p.addFile(g4dat + isots[i] + "/analyzed_*.root");
		assert(g2p.getnFiles());
		g2p.setCalibrator(*POA.ActiveCal);
		TH1F* hSim = OM.registeredTH1F(isots[i]+"_Sim","",nbins,0,emax);
		hSim->GetXaxis()->SetTitle("Energy [keV]");
		g2p.startScan(hSpec->GetEntries());
		double simFactor = (isots[i]=="Xe135_3-2+")?0.75:0.25;
		while(hSim->GetEntries() < simFactor*hSpec->GetEntries()) {
			g2p.nextPoint();
			Side s = g2p.fSide;
			if(g2p.fPID == PID_BETA && g2p.fType <= TYPE_III_EVENT && (s==EAST || s==WEST) && g2p.radius(s)<50.)
				hSim->Fill(g2p.getEtrue());
		}
		hIsot.push_back((TH1F*)hSim->Clone());
		LHC.addTerm(hSim);
	}
	
	
	// fit composition
	double w = hSpec->GetBinWidth(1);
	hSpec->Scale(w/hSpec->Integral());
	LHC.getFitter()->SetLineColor(4);
	LHC.getFitter()->SetLineWidth(1);
	LHC.getFitter()->SetNpx(nbins);
	LHC.forceNonNegative();
	LHC.Fit(hSpec,75,emax);
	TH1F* hSim = OM.registeredTH1F("hSim","Simulated Spectrum",nbins,0,emax);
	Stringmap m;
	m.insert("run",rn);
	m.insert("counts",hSpec->GetEntries());
	for(unsigned int i=0; i<isots.size(); i++) {
		hIsot[i]->Scale(LHC.coeffs[i]);
		hIsot[i]->SetLineStyle(3);
		hSim->Add(hIsot[i]);
		m.insert("name_"+itos(i),isots[i]);
		m.insert("prop_"+itos(i),hIsot[i]->Integral()/w);
		m.insert("err_"+itos(i),LHC.dcoeffs[i]/LHC.coeffs[i]);
	}
	m.display();
	OM.qOut.insert("xedecomp",m);
	
	// draw
	hSpec->SetMinimum(0);
	hSpec->SetTitle("Xenon spectrum decomposition");
	hSpec->GetXaxis()->SetTitle("Energy [keV]");
	hSpec->Draw();
	for(unsigned int i=0; i<isots.size(); i++)
		hIsot[i]->Draw("Same");
	OM.printCanvas("XeDecomp_"+itos(rn));
	
	OM.write();
	//OM.setWriteRoot(true);
}

//-------------------------------------------------------------//

ErrTables::ErrTables(const std::string& datset): 
OM("nameUnused",getEnvSafe("UCNA_ANA_PLOTS")),
Adat(&OM, "nameUnused", getEnvSafe("UCNA_ANA_PLOTS")+"/"+datset+"/"+datset) {
	Adat.calculateResults();
	for(Side s = EAST; s <= WEST; ++s)
		for(AFPState afp = AFP_OFF; afp <= AFP_ON; ++afp)
			S[s][afp] = TH1toTGraph(*Adat.qTotalSpectrum[s]->fgbg[afp]->h[GV_OPEN]);
}

ErrTables::~ErrTables() {
	for(Side s = EAST; s <= WEST; ++s)
		for(AFPState afp = AFP_OFF; afp <= AFP_ON; ++afp)
			delete S[s][afp];
}

double ErrTables::getRexp(double e) const {
	return (S[EAST][AFP_OFF]->Eval(e)*S[WEST][AFP_ON]->Eval(e) / 
			(S[EAST][AFP_ON]->Eval(e)*S[WEST][AFP_OFF]->Eval(e)));
}

double ErrTables::AofR(double R) {
	double A =  (1-sqrt(R))/(1+sqrt(R));
	return (A==A)?A:0;
}

double ErrTables::getAexp(double e) const {
	double A = AofR(getRexp(e));
	return fabs(A)>.01?A:-.01;
}

void ErrTables::gainfluctsTable(double delta) {
	FILE* f = fopen((getEnvSafe("UCNA_AUX")+"/Corrections/GainFlucts.txt").c_str(),"w");
	fprintf(f,"# Run-to-run gain fluctuations uncertainty for anticorrelated delta = %g\n",delta);
	fprintf(f,"#\n#E_lo\tE_hi\tcorrection\tuncertainty\n");
	for(unsigned int b=0; b<800; b+=10) {
		double e = b+5.;
		double A = getAexp(e);
		double Rp = (S[EAST][AFP_OFF]->Eval(e)*S[WEST][AFP_ON]->Eval(e/(1-delta)) / 
					 (S[EAST][AFP_ON]->Eval(e/(1+delta))*S[WEST][AFP_OFF]->Eval(e)));
		double Ap = AofR(Rp);
		fprintf(f,"%i\t%i\t%g\t%g\n",b,b+10,0.,(A-Ap)/A);
	}
	fclose(f);	
}

void ErrTables::pedShiftsTable(double delta) {
	FILE* f = fopen((getEnvSafe("UCNA_AUX")+"/Corrections/PedShifts.txt").c_str(),"w");
	fprintf(f,"# Run-to-run pedestal shifts uncertainty for anticorrelated delta/sqrt(N) = %g keV\n",delta);
	fprintf(f,"#\n#E_lo\tE_hi\tcorrection\tuncertainty\n");
	for(unsigned int b=0; b<800; b+=10) {
		double e = b+5.;
		double A = getAexp(e);
		double Rp = (S[EAST][AFP_OFF]->Eval(e)*S[WEST][AFP_ON]->Eval(e+delta) / 
					 (S[EAST][AFP_ON]->Eval(e-delta)*S[WEST][AFP_OFF]->Eval(e)));
		double Ap = AofR(Rp);
		fprintf(f,"%i\t%i\t%g\t%g\n",b,b+10,0.,(A-Ap)/A);
	}
	fclose(f);
}

void ErrTables::muonVetoEfficTable(double delta) {
	// load spectrum data
	TGraphErrors* M[2][2];
	for(Side s = EAST; s <= WEST; ++s)
		for(AFPState afp = AFP_OFF; afp <= AFP_ON; ++afp)
			M[s][afp] = TH1toTGraph(*Adat.qMuonSpectra[s][false]->fgbg[afp]->h[GV_OPEN]);
	
	// write uncertainty file
	FILE* f = fopen((getEnvSafe("UCNA_AUX")+"/Corrections/MuonEffic.txt").c_str(),"w");
	fprintf(f,"# Run-to-run muon veto efficiency shifts by anticorrelated delta/sqrt(N) = %g%%\n",delta*100);
	fprintf(f,"#\n#E_lo\tE_hi\tcorrection\tuncertainty\n");
	for(unsigned int b=0; b<800; b+=10) {
		double e = b+5.;
		double A = getAexp(e);
		double Rp = ((S[EAST][AFP_OFF]->Eval(e)+0.5*delta*M[EAST][AFP_OFF]->Eval(e)) * 
					 (S[WEST][AFP_ON]->Eval(e)+0.5*delta*M[WEST][AFP_ON]->Eval(e)) / 
					 (S[EAST][AFP_ON]->Eval(e)-0.5*delta*M[EAST][AFP_ON]->Eval(e)) / 
					 (S[WEST][AFP_OFF]->Eval(e)-0.5*delta*M[WEST][AFP_OFF]->Eval(e)));
		double Ap = AofR(Rp);
		fprintf(f,"%i\t%i\t%g\t%g\n",b,b+10,0.,(A-Ap)/A);
	}
	fclose(f);
	
	// cleanup
	for(Side s = EAST; s <= WEST; ++s)
		for(AFPState afp = AFP_OFF; afp <= AFP_ON; ++afp)
			delete M[s][afp];
}

void ErrTables::efficShiftTable(double delta) {
	FILE* f = fopen((getEnvSafe("UCNA_AUX")+"/Corrections/EfficShifts.txt").c_str(),"w");
	fprintf(f,"# Uncertainty for uniform anticorrelated efficiency shift of %g\n",delta);
	fprintf(f,"#\n#E_lo\tE_hi\tcorrection\tuncertainty\n");
	for(unsigned int b=0; b<800; b+=10) {
		double e = b+5.;
		double A = getAexp(e);
		double Rp = getRexp(e)*pow((1+0.5*delta)/(1-0.5*delta),2);
		double Ap = AofR(Rp);
		fprintf(f,"%i\t%i\t%g\t%g\n",b,b+10,0.,(A-Ap)/A);
	}
	fclose(f);
}

void scalevars(double Eoff, double Eon, double Woff, double Won, double& Eoff1, double& Eon1, double& Woff1, double& Won1, bool comshift) {
	if(comshift) {
		double c = sqrt(Eon*Won/(Eoff*Woff));
		Eoff1 = (Eon+Eoff)/(1+c);
		Woff1 = (Won+Woff)/(1+c);
		Eon1 = c*Eoff1;
		Won1 = c*Woff1;
	} else {
		Eoff1 = Eoff;
		Eon1 = Eon;
		Woff1 = Woff;
		Won1 = Won;
	}
}

void ErrTables::NGBGTable(double Eoff, double Eon, double Woff, double Won, double err, bool comshift) {
	FILE* f = fopen((getEnvSafe("UCNA_AUX")+"/Corrections/NGBG.txt").c_str(),"w");
	fprintf(f,"# Uncertainty for neutron-generated backgrounds of Eoff,Eon,Woff,Won = %g,%g,%g,%g Hz/keV\n",Eoff,Eon,Woff,Won);
	fprintf(f,"#\n#E_lo\tE_hi\tcorrection\tuncertainty\n");
	double de = 10;
	double tOn = Adat.totalTime[AFP_ON][GV_OPEN][BOTH];
	double tOff = Adat.totalTime[AFP_OFF][GV_OPEN][BOTH];
	
	double errscale = 10;
	err /= errscale;
	
	for(unsigned int b=0; b<800; b+=de) {
		double Eoff1,Eon1,Woff1,Won1;
		scalevars(Eoff,Eon,Woff,Won,Eoff1,Eon1,Woff1,Won1,comshift);
		double e = b+5.;
		double A = getAexp(e);
		double Rp = ((S[EAST][AFP_OFF]->Eval(e)+de*Eoff1*tOff)*(S[WEST][AFP_ON]->Eval(e)+de*Won1*tOn) / 
					 ((S[EAST][AFP_ON]->Eval(e)+de*Eon1*tOn)*(S[WEST][AFP_OFF]->Eval(e)+de*Woff1*tOff)));
		double Ap = AofR(Rp);

		// MC for errors
		double sx = 0;
		double sxx = 0;
		unsigned int n = 500;
		for(unsigned int i=0; i<n; i++) {
			scalevars(Eoff+plRndSrc.Gaus(0.,err),Eon+plRndSrc.Gaus(0.,err),
					  Woff+plRndSrc.Gaus(0.,err),Won+plRndSrc.Gaus(0.,err),
					  Eoff1,Eon1,Woff1,Won1,comshift);
			double Rpp = ((S[EAST][AFP_OFF]->Eval(e)+de*Eoff1*tOff)*(S[WEST][AFP_ON]->Eval(e)+de*Won1*tOn) / 
						 ((S[EAST][AFP_ON]->Eval(e)+de*Eon1*tOn)*(S[WEST][AFP_OFF]->Eval(e)+de*Woff1*tOff)));
			double App = AofR(Rpp);
			sx += (A-App)/A;
			sxx += (A-App)/A*(A-App)/A;
		}
		sx /= n;
		sxx /= n;
		
		printf("%i\t%i\t%g\t%g\t%g\n",b,b+10,(A-Ap)/A,sx,errscale*sqrt(sxx-sx*sx));
		fprintf(f,"%i\t%i\t%g\t%g\n",b,b+10,(A-Ap)/A,errscale*sqrt(sxx-sx*sx));
	}
	fclose(f);
}

void lowStatsTest() {
	int muMax=100;
	unsigned int nBins = 10000;
	TF1 cFit("cFit","pol0",0,nBins);
	
	for(double mu = 0; mu <= muMax; mu++) {
		TH1F hCounts("hCounts","hCounts",nBins,-0.5,nBins+0.5);
		hCounts.Sumw2();
		for(unsigned int i=0; i<nBins; i++) {
			int n = plRndSrc.Poisson(mu);
			//hCounts.SetBinContent(i+1,n+1.-(n>0?1./n:0));
			//hCounts.SetBinError(i+1,sqrt(n+1));
			hCounts.SetBinContent(i+1,n);
			hCounts.SetBinError(i+1,sqrt(n));
		}
		hCounts.Fit(&cFit,"Q");
		double hint = hCounts.Integral()/nBins;
		printf("mu = %g:\tobs = %.1f\t%.1f\t%.1f\t%.1f\n",mu,cFit.GetParameter(0),cFit.GetParameter(0)-mu,hint,hint-mu);
	}
}

//-------------------------------------------------------------//

void NGBGSpectra(std::string datname) {
	OutputManager OM("NGBG",getEnvSafe("UCNA_ANA_PLOTS")+"/NGBG/");
	G4toPMT g2p;
	g2p.addFile(getEnvSafe("G4WORKDIR")+"/output/"+datname+"/analyzed_*.root");
	g2p.setAFP(AFP_OFF);
	g2p.weightAsym = false;
	PMTCalibrator PCal(15668);
	g2p.setCalibrator(PCal);
	
	
	SimAsymmetryAnalyzer AH(&OM,datname);
	AH.loadSimData(g2p);
	
	AH.calculateResults();
	AH.makePlots();
	AH.write();
	AH.setWriteRoot(true);
}


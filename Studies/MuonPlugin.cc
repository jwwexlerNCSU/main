#include "MuonPlugin.hh"
#include "GraphicsUtils.hh"

MuonPlugin::MuonPlugin(OctetAnalyzer* OA): OctetAnalyzerPlugin(OA,"muon"), nEnergyBins(150), energyMax(1500) {
	for(Side s = EAST; s <= WEST; ++s) {
		qMuonSpectra[s][false] = registerCoreHist("hMuonNoSub", "Tagged Muon Events Energy",
													   nEnergyBins, 0, energyMax, s);
		qMuonSpectra[s][false]->setSubtraction(false);
		qMuonSpectra[s][false]->setAxisTitle(X_DIRECTION,"Energy [keV]");
		qMuonSpectra[s][true] = registerCoreHist("hMuonSpectrum", "Tagged Muon Events Energy",
													  nEnergyBins, 0, energyMax, s);
		qMuonSpectra[s][true]->setAxisTitle(X_DIRECTION,"Energy [keV]");
		
		qBackMuons[s][false] = registerCoreHist("hBackMuNoSub", "Tagged Muon Events Energy",
													 nEnergyBins, 0, energyMax, s);
		qBackMuons[s][false]->setSubtraction(false);
		qBackMuons[s][false]->setAxisTitle(X_DIRECTION,"Energy [keV]");
		qBackMuons[s][true] = registerCoreHist("hBackMu", "Tagged Muon Events Energy",
													nEnergyBins, 0, energyMax, s);
		qBackMuons[s][true]->setAxisTitle(X_DIRECTION,"Energy [keV]");
		
		TH2F hMuPosTemplate("hMuonPos","Muon event positions",100,-65,65,100,-65,65);
		TH2F hBackMuPosTemplate("hBackMuPos","Backing veto muon positions",100,-65,65,100,-65,65);
		pMuonPos[s] = registerFGBGPair(hMuPosTemplate,AFP_OTHER,s);
		pMuonPos[s]->doSubtraction = false;
		pBackMuPos[s] = registerFGBGPair(hBackMuPosTemplate,AFP_OTHER,s);
		pBackMuPos[s]->doSubtraction = false;
		pMuonPos[s]->setAxisTitle(X_DIRECTION,"x Position [mm]");
		pMuonPos[s]->setAxisTitle(Y_DIRECTION,"y Position [mm]");
		pBackMuPos[s]->setAxisTitle(X_DIRECTION,"x Position [mm]");
		pBackMuPos[s]->setAxisTitle(Y_DIRECTION,"y Position [mm]");
	}
}

void MuonPlugin::fillCoreHists(ProcessedDataScanner& PDS, double weight) {
	Side s = PDS.fSide;
	if(!(s==EAST || s==WEST)) return;
	if(!(PDS.fPID == PID_MUON && PDS.fType <= TYPE_III_EVENT)) return;
	if(PDS.passesPositionCut(s)) {
		qMuonSpectra[s][false]->fillPoint->Fill(PDS.getErecon(),weight);
		qMuonSpectra[s][true]->fillPoint->Fill(PDS.getErecon(),weight);
	}
	((TH2F*)pMuonPos[s]->h[currentGV])->Fill(PDS.wires[s][X_DIRECTION].center,PDS.wires[s][Y_DIRECTION].center,weight);
	
	if(!PDS.fTaggedBack[s]) return;
	if(PDS.passesPositionCut(s)) {
		qBackMuons[s][false]->fillPoint->Fill(PDS.getErecon(),weight);
		qBackMuons[s][true]->fillPoint->Fill(PDS.getErecon(),weight);
	}
	((TH2F*)pBackMuPos[s]->h[currentGV])->Fill(PDS.wires[s][X_DIRECTION].center,PDS.wires[s][Y_DIRECTION].center,weight);
}

void MuonPlugin::calculateResults() {
	
	TF1 fLandau("fLandau","landau",600,1100);
	double emin = 220;
	double emax = 670;
	for(Side s = EAST; s <= WEST; ++s) {
		fLandau.SetLineColor(2+2*s);
		for(AFPState afp = AFP_OFF; afp <= AFP_ON; ++afp) {
			for(GVState gv = GV_CLOSED; gv <= GV_OPEN; ++gv) {
				Stringmap m;
				m.insert("side",sideSubst("%c",s));
				m.insert("afp",afpWords(afp));
				m.insert("gv",gvWords(gv));
				
				// Landau fit to backing muon spectrum peak
				if(gv) {
					qBackMuons[s][gv?false:true]->fgbg[afp]->h[gv]->Fit(&fLandau,"QR+");
					m.insert("height",fLandau.GetParameter(0));
					m.insert("d_height",fLandau.GetParError(0));
					m.insert("mpv",fLandau.GetParameter(1));
					m.insert("d_mpv",fLandau.GetParError(1));
					m.insert("sigma",fLandau.GetParameter(2));
					m.insert("d_sigma",fLandau.GetParError(2));
				}
				
				// muon rate passing energy and position cuts
				TH1* h = qMuonSpectra[s][gv?false:true]->fgbg[afp]->h[gv];
				float tm = myA->totalTime[afp][gv][s];
				Double_t ierr;
				m.insert("ecut_eMin",emin);
				m.insert("ecut_eMax",emax);
				m.insert("mu_rate",h->IntegralAndError(1,h->GetNbinsX(),ierr)/tm);
				m.insert("d_mu_rate",ierr/tm);
				m.insert("ecut_mu_rate",h->IntegralAndError(h->FindBin(emin),h->FindBin(emax),ierr)/tm);
				m.insert("d_ecut_mu_rate",ierr/tm);
				
				TH1* hb = qBackMuons[s][gv?false:true]->fgbg[afp]->h[gv];
				m.insert("mu_back_rate",hb->IntegralAndError(1,h->GetNbinsX(),ierr)/tm);
				m.insert("d_mu_back_rate",ierr/tm);
				m.insert("ecut_mu_back_rate",hb->IntegralAndError(hb->FindBin(emin),hb->FindBin(emax),ierr)/tm);
				m.insert("d_ecut_mu_back_rate",ierr/tm);
				
				myA->qOut.insert("muons_info",m);
			}
		}
	}
}

void MuonPlugin::makePlots() {

	myA->defaultCanvas->SetRightMargin(0.04);
	myA->defaultCanvas->SetLeftMargin(0.12);

	drawQuadSides(qMuonSpectra[EAST][true], qMuonSpectra[WEST][true], true, "MuonSpectra");
	drawQuadSides(qMuonSpectra[EAST][false], qMuonSpectra[WEST][false], true, "MuonSpectra");
	drawQuadSides(qBackMuons[EAST][true], qBackMuons[WEST][true], true, "MuonSpectra");
	drawQuadSides(qBackMuons[EAST][false], qBackMuons[WEST][false], true, "MuonSpectra");
	
	// positions
	if(myA->depth <= 0) {
		for(Side s = EAST; s <= WEST; ++s) {
			pMuonPos[s]->h[GV_OPEN]->Draw("COL");
			printCanvas(sideSubst("MuonSpectra/MuPos_%c",s));
			pBackMuPos[s]->h[GV_OPEN]->Draw("COL");
			printCanvas(sideSubst("MuonSpectra/BackMuPos_%c",s));
		}
	}
	
	// energy spectra
	std::vector<TH1*> hToPlot_allmu;
	std::vector<TH1*> hToPlot_backmu;
	for(Side s = EAST; s <= WEST; ++s) {
		TH1* hMuRt = myA->flipperSummedRate(qMuonSpectra[s][false], GV_OPEN);
		TH1* hMuBkRt = myA->flipperSummedRate(qBackMuons[s][false], GV_OPEN);
		
		hMuRt->GetYaxis()->SetTitleOffset(1.45);
		hMuRt->Scale(1000);
		hMuRt->GetYaxis()->SetTitle("event rate [mHz/keV]");
		hMuRt->SetTitle("Muon tagged events");
		hMuRt->SetMinimum(0);
		hMuRt->SetLineStyle(1+s);
		hMuRt->SetLineColor(1);
		hToPlot_allmu.push_back(hMuRt);
		
		hMuBkRt->GetYaxis()->SetTitleOffset(1.45);
		hMuBkRt->Scale(1000);
		hMuBkRt->GetYaxis()->SetTitle("event rate [mHz/keV]");
		hMuBkRt->SetTitle("Muon backing tagged events");
		hMuBkRt->SetMinimum(0);
		hMuBkRt->SetLineStyle(1+s);
		hMuBkRt->SetLineColor(1);
		hToPlot_backmu.push_back(hMuBkRt);
	}
	drawSimulHistos(hToPlot_allmu,"HIST");
	printCanvas("MuonSpectra/MuRate");
	drawSimulHistos(hToPlot_backmu,"HIST");
	printCanvas("MuonSpectra/MuRate_Back");

}

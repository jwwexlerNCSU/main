#include "PositionAnalyzer.hh"

PositionAnalyzer::PositionAnalyzer(OctetAnalyzer* OA): OctetAnalyzerPlugin(OA,"position"), offSects(5,45.0) {
	myA->ignoreMissingHistos = true;
	for(unsigned int m=0; m<offSects.nSectors(); m++) {
		for(AxisDirection d = X_DIRECTION; d <= Y_DIRECTION; ++d) {
			poff[d].push_back(registerFGBGPair((d==X_DIRECTION?"pOff_X_":"pOff_Y_")+itos(m), "Type I Position Offsets",50,-25, 25));
			poff[d].back()->setAxisTitle(X_DIRECTION,"E-W offset [mm]");
		}
	}
	
	for(Side s = EAST; s <= WEST; ++s) {
		for(EventType t=TYPE_0_EVENT; t<=TYPE_III_EVENT; ++t) {
			TH2F hPositionsTemplate(("hPos_Type_"+itos(t)).c_str(),
									("Type "+itos(t)+" Positions").c_str(),
									200,-65,65,200,-65,65);
			qPositions[s][t] = registerCoreHist(hPositionsTemplate,s);
			qPositions[s][t]->setAxisTitle(X_DIRECTION,"x Position [mm]");
			qPositions[s][t]->setAxisTitle(Y_DIRECTION,"y Position [mm]");
		}
	}
	myA->ignoreMissingHistos = false;
}

void PositionAnalyzer::fillCoreHists(ProcessedDataScanner& PDS, double weight) {
	Side s = PDS.fSide;
	if(PDS.fPID!=PID_BETA || !(s==EAST||s==WEST)) return;
	if(PDS.fType <= TYPE_III_EVENT)
		((TH2F*)qPositions[s][PDS.fType]->fillPoint)->Fill(PDS.wires[s][X_DIRECTION].center,PDS.wires[s][Y_DIRECTION].center,weight);
	if(PDS.fType != TYPE_I_EVENT) return;
	double x = 0.5*(PDS.wires[EAST][X_DIRECTION].center+PDS.wires[WEST][X_DIRECTION].center);
	double y = 0.5*(PDS.wires[EAST][Y_DIRECTION].center+PDS.wires[WEST][Y_DIRECTION].center);
	unsigned int m = offSects.sector(x,y);
	if(m>=offSects.nSectors()) return;
	for(AxisDirection d = X_DIRECTION; d <= Y_DIRECTION; ++d)
		poff[d][m]->h[currentGV]->Fill(PDS.wires[EAST][d].center-PDS.wires[WEST][d].center,weight);
}

void PositionAnalyzer::calculateResults() {
	float x,y;
	TF1 gausFit("gasufit","gaus",-25,25);
	gausFit.SetLineColor(2);
	for(unsigned int m=0; m<offSects.nSectors(); m++) {
		Stringmap odat;
		offSects.sectorCenter(m,x,y);
		odat.insert("m",itos(m));
		odat.insert("x",x);
		odat.insert("y",y);
		for(AxisDirection d = X_DIRECTION; d <= Y_DIRECTION; ++d) {
			std::string axname = (d==X_DIRECTION?"x":"y");
			poff[d][m]->h[GV_OPEN]->Fit(&gausFit,"QR");
			odat.insert("d"+axname,gausFit.GetParameter(1));
			odat.insert("d_d"+axname,gausFit.GetParError(1));
			odat.insert("w"+axname,gausFit.GetParameter(2));
			odat.insert("d_w"+axname,gausFit.GetParError(2));
		}
		myA->qOut.insert("posOffset",odat);
	}
}

void PositionAnalyzer::makePlots() {
	if(myA->depth > 0) return;
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int t=TYPE_0_EVENT; t<=TYPE_II_EVENT; t++) {
			qPositions[s][t]->setDrawRange(0,false);
			drawQuad(qPositions[s][t],"Positions","COL");
		}
	}
}

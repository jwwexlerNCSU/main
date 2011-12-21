#include "bmSourceHolderConstruction.hh"
#include "G4SubtractionSolid.hh"

void bmSourceHolderConstruction::Construct() {
	
	const G4double fSourceRingRadius = 0.5*inch;
	const G4double fSourceWindowRadius = fSourceRingRadius-3.*mm;
	const G4double fSourceRingThickness = 3.2*mm;			// suspiciously close to 1/8 in
	const G4double fSourceHolderHeight = 1.5*inch;
	const G4double fSourceHolderWidth = 1.5*inch;
	const G4double fSourceHolderThickness = 3./16.*inch;	// need to measure this
	
	// source holder container
	G4Box* holder_box = new G4Box("source_holder_box",0.5*fSourceHolderWidth,0.5*fSourceHolderHeight,0.5*fSourceHolderThickness);
	container_log = new G4LogicalVolume(holder_box,Vacuum,"source_constainer_log");
	container_log->SetVisAttributes(G4VisAttributes::Invisible);
	
	// source holder paddle
	G4Tubs* holder_hole = new G4Tubs("source_holder_hole",0.,fSourceRingRadius,fSourceHolderThickness,0.,2*M_PI);
	G4SubtractionSolid* holder = new G4SubtractionSolid("source_holder", holder_box, holder_hole);
	G4LogicalVolume* holder_log = new G4LogicalVolume(holder,Brass,"source_holder_log");
	holder_log->SetVisAttributes(new G4VisAttributes(G4Colour(0.7,0.7,0,0.5)));
	G4VPhysicalVolume* holder_phys = NULL;
	holder_phys = new G4PVPlacement(NULL,G4ThreeVector(),holder_log,"source_holder_phys",container_log,false,0);
	
	// sealed source foil
	G4Tubs* window_tube = new G4Tubs("window_tube",0.,fSourceWindowRadius,fWindowThick,0.,2*M_PI);
	window_log = new G4LogicalVolume(window_tube,Mylar,"source_window_log");
	G4VisAttributes* visWindow = new G4VisAttributes(G4Colour(0,1.0,0,1));
	window_log->SetVisAttributes(visWindow);
	window_phys = new G4PVPlacement(NULL,G4ThreeVector(),window_log,"source_window_phys",container_log,false,0);
	
	// source foil coating
	G4Tubs* coating_tube = new G4Tubs("source_coating_tube",0.,fSourceWindowRadius,fCoatingThick,0.,2*M_PI);
	for(Side s = EAST; s <= WEST; ++s) {
		coating_log[s] = new G4LogicalVolume(coating_tube,Al,sideSubst("source_coating_log_%c",s));
		coating_log[s]->SetVisAttributes(new G4VisAttributes(G4Colour(0,1,0,0.5)));
		coating_phys[s] = new G4PVPlacement(NULL,G4ThreeVector(0.,0.,sign(s)*(fWindowThick+fCoatingThick)/2),
											coating_log[s],sideSubst("source_coating_phys_%c",s),container_log,false,0);
	}
	
	// source retaining ring
	G4Tubs* ring_tube = new G4Tubs("source_ring_tube",fSourceWindowRadius,fSourceRingRadius,
								   fSourceRingThickness/2,0.,2*M_PI);
	G4LogicalVolume* ring_log = new G4LogicalVolume(ring_tube,Al,"source_ring_log");
	ring_log->SetVisAttributes(new G4VisAttributes(G4Colour(0.7,0.7,0.7,0.5)));
	G4VPhysicalVolume* ring_phys;
	ring_phys = new G4PVPlacement(NULL,G4ThreeVector(),ring_log,"source_ring_phys",container_log,false,0);
}
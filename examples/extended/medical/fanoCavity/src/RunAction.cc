//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file medical/fanoCavity/src/RunAction.cc
/// \brief Implementation of the RunAction class
//
// $Id: RunAction.cc 68996 2013-04-15 09:19:55Z gcosmo $
// 
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "RunAction.hh"
#include "DetectorConstruction.hh"
#include "PrimaryGeneratorAction.hh"
#include "HistoManager.hh"

#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4UnitsTable.hh"
#include "G4EmCalculator.hh"
#include "G4Electron.hh"

#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include <iomanip>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction(DetectorConstruction* det, PrimaryGeneratorAction* kin)
:fDetector(det),fKinematic(kin),fProcCounter(0),fHistoManager(0),
 fMateWall(0),fMateCavity(0)
{
 fHistoManager = new HistoManager(); 
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction()
{
 delete fHistoManager;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run* aRun)
{  
  G4cout << "### Run " << aRun->GetRunID() << " start." << G4endl;
  
  // do not save Rndm status
  G4RunManager::GetRunManager()->SetRandomNumberStore(false);
  CLHEP::HepRandom::showEngineStatus();
  
  //geometry
  //
  fWallThickness = fDetector->GetWallThickness();
  fWallRadius    = fDetector->GetWallRadius();
  fMateWall      = fDetector->GetWallMaterial();
  fDensityWall   = fMateWall->GetDensity();

  fCavityThickness = fDetector->GetCavityThickness();
  fCavityRadius    = fDetector->GetCavityRadius();
  fSurfaceCavity   = pi*fCavityRadius*fCavityRadius;
  fVolumeCavity    = fSurfaceCavity*fCavityThickness;                        
  fMateCavity      = fDetector->GetCavityMaterial();
  fDensityCavity   = fMateCavity->GetDensity();
  fMassCavity      = fVolumeCavity*fDensityCavity;
    
  //process counter
  //
  fProcCounter = new ProcessesCount;
  
  //kinetic energy of charged secondary a creation
  //
  fEsecondary = fEsecondary2 = 0.;
  fNbSec = 0;
  
  //charged particles and energy flow in cavity
  //
  fPartFlowCavity[0] = fPartFlowCavity[1] = 0;
  fEnerFlowCavity[0] = fEnerFlowCavity[1] = 0.;
       
  //total energy deposit and charged track segment in cavity
  //
  fEdepCavity = fEdepCavity2 = fTrkSegmCavity = 0.;
  fNbEventCavity = 0; 
   
  //stepLenth of charged particles
  //
  fStepWall = fStepWall2 = fStepCavity = fStepCavity2 =0.;
  fNbStepWall = fNbStepCavity = 0;
    
  //histograms
  //
  G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
  if ( analysisManager->IsActive() ) {
    analysisManager->OpenFile();
  }             
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::CountProcesses(G4String procName)
{
   //does the process  already encounted ?
   size_t nbProc = fProcCounter->size();
   size_t i = 0;
   while ((i<nbProc)&&((*fProcCounter)[i]->GetName()!=procName)) i++;
   if (i == nbProc) fProcCounter->push_back( new OneProcessCount(procName));

   (*fProcCounter)[i]->Count();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::SurveyConvergence(G4int NbofEvents)
{  
  if (NbofEvents == 0) return;
    
  //mean kinetic energy of secondary electrons
  //
  G4double meanEsecond = 0.;
  if (fNbSec > 0) meanEsecond = fEsecondary/fNbSec;
  G4double rateEmean = 0.;
  // compute variation rate (%), iteration to iteration
  if (fOldEmean > 0.) rateEmean = 100*(meanEsecond/fOldEmean - 1.);
  fOldEmean = meanEsecond;
                  
  //beam energy fluence
  //
  G4double rBeam = fWallRadius*(fKinematic->GetBeamRadius());
  G4double surfaceBeam = pi*rBeam*rBeam;
  G4double beamEnergy = fKinematic->GetParticleGun()->GetParticleEnergy();  
         
  //total dose in cavity
  //                   
  G4double doseCavity = fEdepCavity/fMassCavity;
  G4double doseOverBeam = doseCavity*surfaceBeam/(NbofEvents*beamEnergy);
  G4double rateDose = 0.;
  // compute variation rate (%), iteration to iteration  
  if (fOldDose > 0.) rateDose = 100*(doseOverBeam/fOldDose - 1.);
  fOldDose = doseOverBeam;  

  std::ios::fmtflags mode = G4cout.flags();
  G4cout.setf(std::ios::fixed,std::ios::floatfield);
  G4int prec = G4cout.precision(3);
    
  G4cout << "\n ---> NbofEvents= " << NbofEvents 
         << "   NbOfelectr= " << fNbSec
         << "   Tkin= " << G4BestUnit(meanEsecond,"Energy")
         << " (" << rateEmean << " %)"
         << "   NbOfelec in cav= " << fPartFlowCavity[0]
         << "   Dose/EnFluence= " << G4BestUnit(doseOverBeam,"Surface/Mass")
         << " (" << rateDose << " %)"
         << G4endl;
                  
  // reset default formats
  G4cout.setf(mode,std::ios::floatfield);
  G4cout.precision(prec);  
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* aRun)
{
  std::ios::fmtflags mode = G4cout.flags();
  G4cout.setf(std::ios::fixed,std::ios::floatfield);
  
  G4int NbofEvents = aRun->GetNumberOfEvent();
  if (NbofEvents == 0) return;
  
  //run conditions
  //     
  G4ParticleDefinition* particle = fKinematic->GetParticleGun()
                                          ->GetParticleDefinition();
  G4String partName = particle->GetParticleName();                             
  G4double energy = fKinematic->GetParticleGun()->GetParticleEnergy();
  
  G4cout << "\n ======================== run summary ======================\n";
  
  G4int prec = G4cout.precision(3);
  
  G4cout << "\n The run consists of " << NbofEvents << " "<< partName << " of "
         << G4BestUnit(energy,"Energy") << " through 2*" 
         << G4BestUnit(fWallThickness,"Length") << " of "
         << fMateWall->GetName() << " (density: " 
         << G4BestUnit(fDensityWall,"Volumic Mass") << ")" << G4endl;
         
  G4cout << "\n the cavity is "
         << G4BestUnit(fCavityThickness,"Length") << " of "
         << fMateCavity->GetName() << " (density: " 
         << G4BestUnit(fDensityCavity,"Volumic Mass") << "); Mass = " 
         << G4BestUnit(fMassCavity,"Mass") << G4endl;
                  
  G4cout << "\n ============================================================\n";

  //frequency of processes
  //
  G4cout << "\n Process calls frequency --->";
  for (size_t i=0; i< fProcCounter->size();i++) {
     G4String procName = (*fProcCounter)[i]->GetName();
     G4int    count    = (*fProcCounter)[i]->GetCounter(); 
     G4cout << "  " << procName << "= " << count;
  }
  G4cout << G4endl;
    
  //extract cross sections with G4EmCalculator
  //
  G4EmCalculator emCalculator;  
  G4cout << "\n Gamma crossSections in wall material :";
  G4double sumc = 0.0;  
  for (size_t i=0; i< fProcCounter->size();i++) {
    G4String procName = (*fProcCounter)[i]->GetName();
    G4double massSigma = 
    emCalculator.ComputeCrossSectionPerVolume(energy,particle,
                                              procName,fMateWall)/fDensityWall;
    if (massSigma > 0.) {
      sumc += massSigma;
      G4cout << "  " << procName << "= "
             << G4BestUnit(massSigma, "Surface/Mass");
    }             
  }             
  G4cout << "   --> total= " 
         << G4BestUnit(sumc, "Surface/Mass") << G4endl;
  
  //mean kinetic energy of secondary electrons
  //
  if (fNbSec == 0) return;
  G4double meanEsecond = fEsecondary/fNbSec, meanEsecond2 = fEsecondary2/fNbSec;
  G4double varianceEsec = meanEsecond2 - meanEsecond*meanEsecond;
  G4double dToverT = 0.;
  if (varianceEsec>0.) dToverT = std::sqrt(varianceEsec/fNbSec)/meanEsecond;
  G4double csdaRange =
      emCalculator.GetCSDARange(meanEsecond,G4Electron::Electron(),fMateWall);

  G4cout.precision(4);       
  G4cout 
    << "\n Mean energy of secondary e- = " << G4BestUnit(meanEsecond,"Energy")
    << " +- " << 100*dToverT << " %"
    << "  (--> range in wall material = "  << G4BestUnit(csdaRange,"Length")
    << ")"   
    << G4endl;
    
  //compute mass energy transfer coefficient
  //
  G4double massTransfCoef = sumc*meanEsecond/energy;
   
  G4cout << " Mass_energy_transfer coef: "  
         << G4BestUnit(massTransfCoef, "Surface/Mass")
         << G4endl;
         
  //stopping power from EmCalculator
  //
  G4double dedxWall = 
      emCalculator.GetDEDX(meanEsecond,G4Electron::Electron(),fMateWall);
  dedxWall /= fDensityWall;
  G4double dedxCavity = 
      emCalculator.GetDEDX(meanEsecond,G4Electron::Electron(),fMateCavity);
  dedxCavity /= fDensityCavity;
  
  G4cout 
    << "\n StoppingPower in wall   = " 
    << G4BestUnit(dedxWall,"Energy*Surface/Mass")
    << "\n               in cavity = " 
    << G4BestUnit(dedxCavity,"Energy*Surface/Mass")
    << G4endl;  
          
  //charged particle flow in cavity
  //
  G4cout 
    << "\n Charged particle flow in cavity :"
    << "\n      Enter --> nbParticles = " << fPartFlowCavity[0]
    << "\t Energy = " << G4BestUnit (fEnerFlowCavity[0], "Energy")
    << "\n      Exit  --> nbParticles = " << fPartFlowCavity[1]
    << "\t Energy = " << G4BestUnit (fEnerFlowCavity[1], "Energy")
    << G4endl;
             
  if (fPartFlowCavity[0] == 0) return;
                  
  //beam energy fluence
  //
  G4double rBeam = fWallRadius*(fKinematic->GetBeamRadius());
  G4double surfaceBeam = pi*rBeam*rBeam;
  
  //error on Edep in cavity
  //
  if (fNbEventCavity == 0) return;
  G4double meanEdep  = fEdepCavity/fNbEventCavity;
  G4double meanEdep2 = fEdepCavity2/fNbEventCavity;
  G4double varianceEdep = meanEdep2 - meanEdep*meanEdep;
  G4double dEoverE = 0.;
  if(varianceEdep>0.) dEoverE = std::sqrt(varianceEdep/fNbEventCavity)/meanEdep;
           
  //total dose in cavity
  //                   
  G4double doseCavity = fEdepCavity/fMassCavity;
  G4double doseOverBeam = doseCavity*surfaceBeam/(NbofEvents*energy);
  
  //track length in cavity
  G4double meantrack = fTrkSegmCavity/fPartFlowCavity[0];
                    
  G4cout.precision(4);       
  G4cout 
    << "\n Total edep in cavity = "      << G4BestUnit(fEdepCavity,"Energy")
    << " +- " << 100*dEoverE << " %"    
    << "\t Total charged trackLength = " << G4BestUnit(fTrkSegmCavity,"Length")
    << "   (mean value = " << G4BestUnit(meantrack,"Length") << ")"       
    << "\n Total dose in cavity = " << doseCavity/(MeV/mg) << " MeV/mg"
    << "\n Dose/EnergyFluence   = " << G4BestUnit(doseOverBeam,"Surface/Mass")
    << G4endl;
    
  //ratio simulation/theory
  //
  G4double ratio = doseOverBeam/massTransfCoef;
  G4double error = ratio*std::sqrt(dEoverE*dEoverE + dToverT*dToverT);
  
  G4cout.precision(5);  
  G4cout 
    << "\n (Dose/EnergyFluence)/Mass_energy_transfer = " << ratio 
    << " +- " << error << G4endl; 
              
  //compute mean step size of charged particles
  //
  fStepWall /= fNbStepWall; fStepWall2 /= fNbStepWall;
  G4double rms = fStepWall2 - fStepWall*fStepWall;        
  if (rms>0.) rms = std::sqrt(rms); else rms = 0.;

  G4cout.precision(4);       
  G4cout 
    << "\n StepSize of ch. tracks in wall   = " 
    << G4BestUnit(fStepWall,"Length") << " +- " << G4BestUnit( rms,"Length")
    << "\t (nbSteps/track = " << double(fNbStepWall)/fNbSec << ")";
    
  fStepCavity /= fNbStepCavity; fStepCavity2 /= fNbStepCavity;
  rms = fStepCavity2 - fStepCavity*fStepCavity;        
  if (rms>0.) rms = std::sqrt(rms); else rms = 0.;

  G4cout 
   << "\n StepSize of ch. tracks in cavity = " 
   << G4BestUnit(fStepCavity,"Length") << " +- " << G4BestUnit( rms,"Length")
   << "\t (nbSteps/track = " << double(fNbStepCavity)/fPartFlowCavity[0] << ")";
        
  G4cout << G4endl;
  
   // reset default formats
  G4cout.setf(mode,std::ios::floatfield);
  G4cout.precision(prec);
  
  // delete and remove all contents in fProcCounter 
  while (fProcCounter->size()>0){
    OneProcessCount* aProcCount=fProcCounter->back();
    fProcCounter->pop_back();
    delete aProcCount;
  }
  delete fProcCounter;
  
  // save histograms
  G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
  if ( analysisManager->IsActive() ) {
    analysisManager->Write();
    analysisManager->CloseFile();
  }      
 
  // show Rndm status
  CLHEP::HepRandom::showEngineStatus();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

/** \file aoSystem.cpp
  * \author Jared R. Males (jaredmales@gmail.com)
  * \brief Application class and main definition for the aoSystem app.
  * \ingroup files
  * 
  */

//***********************************************************************//
// Copyright 2018 Jared R. Males (jaredmales@gmail.com)
//
// This file is part of aoSystem.
//
// aoSystem is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// aoSystem is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with aoSystem.  If not, see <http://www.gnu.org/licenses/>.
//***********************************************************************//


#include <iostream>
#include <fstream>

#include <Eigen/Dense>


#include <mx/improc/fitsFile.hpp>
#include <mx/math/func/airyPattern.hpp>

#define MX_APP_DEFAULT_configPathGlobal_env "MXAOSYSTEM_GLOBAL_CONFIG"
#define MX_APP_DEFAULT_configPathLocal "aoSystem.conf"

#include <mx/app/application.hpp>
#include <mx/fft/fftwEnvironment.hpp>

#include <mx/ao/analysis/aoSystem.hpp>
#include <mx/ao/analysis/aoPSDs.hpp>
#include <mx/ao/analysis/aoWFS.hpp>
#include <mx/ao/analysis/varmapToImage.hpp>
#include <mx/ao/analysis/fourierTemporalPSD.hpp>

using namespace mx::app;

///
/**
  * Star Magnitudes:
  * - if <b>starMag</b> alone is set, then results are provided for just this one star magnitude.
  * - if <b>starMags</b>, a vector, is set, then many results are provided for each magnitude. E.g. <b>--mode</b>=ErrorBudget will produce a table.
  */  
template<typename _realT>
class mxAOSystem_app : public mx::app::application
{

public:
   typedef _realT realT; ///<Real floating point type for calculations

   typedef Eigen::Array<realT, -1,-1> imageT; ///< The image type
   
   typedef mx::AO::analysis::aoSystem<realT, mx::AO::analysis::vonKarmanSpectrum<realT>> aosysT; ///< The AO system type.

   /// Default constructor
   mxAOSystem_app(); 

   /// Desctructor
   ~mxAOSystem_app();
   
protected:
   
   aosysT aosys; ///< The ao system.
   
   mx::AO::analysis::wfs<realT> idealWFS; ///< An ideal WFS
   mx::AO::analysis::pywfsUnmod<realT> unmodPyWFS; ///< An unmodulated Pyramid WFS 
   mx::AO::analysis::pywfsModAsymptotic<realT> asympModPyWFS; ///< A modulated Pyramid WFS in its asymptotic limit
   
   realT lam_0;
   
   bool dumpSetup;
   std::string setupOutName;
   
   std::string mode;
   
   std::string wfeUnits;
   
   int mnMap;
   
   std::vector<realT> starMags;
   
   realT dfreq;
   realT fmax;
   realT k_m;
   realT k_n;
   std::string gridDir;       ///<The directory for writing the grid of PSDs.
   std::string subDir;       ///< The sub-directory of gridDir where to write the analysis results.
   int lpNc;                 ///< Number of linear predictor coefficients.  If <= 1 then not used.
   bool m_uncontrolledLifetimes {false}; ///< Whether or not lifetimes are calcualated for uncontrolled modes.
   int m_lifetimeTrials {0}; ///< Number of trials to use for calculating speckle lifetimes.  If 0, lifetimes are not calcualted.
   bool writePSDs {false};   ///< Flag controlling whether output temporal PSDs are written to disk or not.
   
   virtual void setupConfig();

   virtual void loadConfig();
   
   virtual int execute();
   
   int C_MapCon( const std::string & mapFile,
                 imageT & map 
               );
              
   int C0Raw();
   int C0Map();
   
   int C1Raw();
   int C1Map();
   
   int C2Raw();
   int C2Map();
   
   int C4Raw();
   int C4Map();
   
   int C6Raw();
   int C6Map();
   
   int C7Raw();
   int C7Map();
   
   int CAllRaw();
   
   int CProfAll();
   
   int ErrorBudget();
   
   int Strehl();
   
   int temporalPSD();
   
   int temporalPSDGrid();
   
   int temporalPSDGridAnalyze();
};

template<typename realT>
mxAOSystem_app<realT>::mxAOSystem_app()
{
   lam_0 = 0;
   
   dumpSetup = true;
   setupOutName = "mxAOAnalysisSetup.txt";
   
   mode = "C2Raw";
   
   aosys.wfsBeta( idealWFS );
   
   wfeUnits = "rad";
   
   aosys.loadMagAOX(); 
   
   mnMap = 50;
   
   dfreq = 0.1;
   fmax = 0;
   k_m = 1;
   k_n = 0;
   lpNc = 0;
}

template<typename realT>
mxAOSystem_app<realT>::~mxAOSystem_app()
{
}   

template<typename realT>
void mxAOSystem_app<realT>::setupConfig()
{
   //App config
   config.add("mode"        ,"m", "mode" , argType::Required, "", "mode",     false,  "string", "Mode of calculation: C<N>Raw, C<N>Map, CAllRaw, CProfAll, ErrorBudget, Strehl, temporalPSD, temporalPSDGrid, temporalPSDGridAnalyze");
   config.add("setupOutFile"        ,"", "setupOutFile" , argType::Required, "", "setupOutFile", false, "string", "Filename for output of setup data");

   config.add("wfeUnits"        ,"", "wfeUnits" , argType::Required, "", "wfeUnits", false, "string", "Units for WFE in ErrorBudget: rad or nm");

   config.add("mnMap"        ,"", "mnMap" , argType::Required, "", "mnMap",     false,  "string", "Maximum spatial frequency index to include in maps.");
   
   //Load a model
   config.add("model"        ,"", "model" , argType::Required, "", "model", false, "string", "Model to load: Guyon2005, MagAOX, or GMagAOX");
   
   //Atmosphere configuration
   config.add("lam_0"        ,"", "lam_0" , argType::Required, "atmosphere", "lam_0",        false, "real", "The reference wavlength for r_0 [m]");
   config.add("r_0"          ,"", "r_0"   , argType::Required, "atmosphere", "r_0",          false, "real", "Fried's parameter [m]");
   config.add("L_0"          ,"", "L_0"   , argType::Required, "atmosphere", "L_0",          false, "real", "Outer scale [m]");
   config.add("layer_Cn2"    ,"", ""      , argType::None,     "atmosphere", "layer_Cn2",    false, "real vector", "Layer Cn^2");  
   config.add("layer_v_wind" ,"", ""      , argType::None,     "atmosphere", "layer_v_wind", false, "real vector", "Layer wind speeds [m/s]");
   config.add("layer_dir"    ,"", ""      , argType::None,     "atmosphere", "layer_dir",    false, "real vector", "Layer wind directions [rad]");
   config.add("layer_z"      ,"", ""      , argType::None,     "atmosphere", "layer_z",      false, "real vector", "layer heights [m]");
   config.add("h_obs"        ,"", ""      , argType::None,     "atmosphere", "h_obs",        false, "real", "height of observatory [m]");
   config.add("H"            ,"", ""      , argType::None,     "atmosphere", "H",            false, "real", "atmospheric scale heights [m]");
   config.add("v_wind"       ,"", "v_wind", argType::Required, "atmosphere", "v_wind",       false, "real", "Mean windspeed (5/3 momement), rescales layers [m/s]");
   config.add("z_mean"       ,"", "z_mean", argType::Required, "atmosphere", "z_mean",       false, "real", "Mean layer height (5/3 momemnt), rescales layers [m/s]");
   
   //PSD Configuration
   config.add("subTipTilt"    ,"", "subTipTilt",    argType::Required, "PSD", "subTipTilt",    false, "bool", "If set to true, the Tip/Tilt component is subtracted from the PSD.");
   config.add("scintillation" ,"", "scintillation", argType::Required, "PSD", "scintillation", false, "bool", "If set to true, then scintillation is included in the PSD.");
   config.add("component"     ,"", "component",     argType::Required, "PSD", "component",     false, "string", "Can be phase [default], amplitude, or dispersion.");
   
   //AO System configuration
   config.add("wfs"           ,"", "wfs"           , argType::Required, "system", "wfs",            false, "string", "The WFS type: idealWFS, unmodPyWFS, asympModPyWFS");
   config.add("D"             ,"", "D"             , argType::Required, "system", "D",              false, "real", "The telescope diameter [m]");
   config.add("d_min"         ,"", "d_min"         , argType::Required, "system", "d_min",          false, "real", "The minimum actuator spacing [m]");
   config.add("optd"          ,"", "optd"          , argType::Optional, "system", "optd",           false, "bool", "Whether or not the actuator spacing is optimized");
   config.add("optd_delta"    ,"", "optd_delta"    , argType::Required, "system", "optd_delta",     false, "bool", "The fractional change from d_min used in optimization.  Set to 1 (default) for integer binnings, > 1 for finer sampling.");
   config.add("F0"            ,"", "F0"            , argType::Required, "system", "F0",             false, "real", "Zero-mag photon flux, [photons/sec]");     
   config.add("lam_wfs"       ,"", "lam_wfs"       , argType::Required, "system", "lam_wfs",        false, "real", "WFS wavelength [m]" );
   config.add("npix_wfs"      ,"", "npix_wfs"      , argType::Required, "system", "npix_wfs",       false, "real", "The number of pixels in the WFS");
   config.add("ron_wfs"       ,"", "ron_wfs"       , argType::Required, "system", "ron_wfs",        false, "real", "WFS readout noise [photons/read]");
   config.add("bin_npix"      ,"", "bin_npix"      , argType::Required, "system", "bin_npix",       false, "bool", "Whether or not WFS pixels are re-binned along with actuator spacing optimization");
   config.add("Fbg"           ,"", "Fbg"           , argType::Required, "system", "Fbg",            false, "real", "Background counts, [counts/pix/sec]");\
   config.add("tauWFS"        ,"", "tauWFS"        , argType::Required, "system", "tauWFS",         false, "real", "WFS integration time [s]");
   config.add("minTauWFS"     ,"", "minTauWFS"     , argType::Required, "system", "minTauWFS",      false, "real", "Minimum WFS integration time [s]");
   config.add("deltaTau"      ,"", "deltaTau"      , argType::Required, "system", "deltaTau",       false, "real", "Loop delay [s]");
   config.add("optTau"        ,"", "optTau"        , argType::Optional, "system", "optTau",         false, "bool", "Whether or not the integration time is optimized");
   config.add("lam_sci"       ,"", "lam_sci"       , argType::Required, "system", "lam_sci",        false, "real", "Science wavelength [m]");
   config.add("zeta"          ,"", "zeta"          , argType::Required, "system", "zeta",           false, "real", "Zenith distance [rad]");
   config.add("fit_mn_max"    ,"", "fit_mn_max"    , argType::Required, "system", "fit_mn_max",     false, "real", "Maximum spatial frequency index to use for analysis");
   config.add("ncp_wfe"       ,"", "ncp_wfe"       , argType::Required, "system", "ncp_wfe",        false, "real", "NCP WFE between 1 lambda/D and fit_mn_max [rad^2]");
   config.add("ncp_alpha"     ,"", "ncp_alpha"     , argType::Required, "system", "ncp_alpha",      false, "real", "PSD index for NCP WFE");
   config.add("starMag"       ,"", "starMag"       , argType::Required,  "system", "starMag",       false, "real", "Star magnitude");
   config.add("starMags"      ,"", "starMags"      , argType::Required,  "system", "starMags",      false, "real vector", "A vector of star magnitudes");
   config.add("circularLimit" ,"", "circularLimit" , argType::Optional,  "system", "circularLimit", false, "bool", " Flag to indicate that the spatial frequency limit is circular, not square.");
   
   //Temporal configuration
   config.add("fmax"     ,"", "fmax"    , argType::Required,  "temporal", "fmax",     false, "real", "Maximum temporal frequency at which to explicitly calculate PSDs.  If 0 (default) this is based on highest wind peak.  A -17/3 power law is used above this frequency.");
   config.add("dfreq"     ,"", "dfreq"    , argType::Required,  "temporal", "dfreq",     false, "real", "Spacing of frequencies in the analysis.");
   config.add("k_m"     ,"", "k_m"    , argType::Required,  "temporal", "k_m",     false, "real", "The spatial frequency m index.");
   config.add("k_n"     ,"", "k_n"    , argType::Required,  "temporal", "k_n",     false, "real", "The spatial frequency n index.");
   config.add("gridDir"     ,"", "gridDir"    , argType::Required,  "temporal", "gridDir",     false, "string", "The directory to store the grid of PSDs.");
   config.add("subDir"     ,"", "subDir"    , argType::Required,  "temporal", "subDir",     false, "string", "The directory to store the analysis results.");
   config.add("lpNc"      ,"", "lpNc",    argType::Required,  "temporal", "lpNc",     false, "int", "The number of linear prediction coefficients to use (if <= 1 ignored)");   
   config.add("uncontrolledLifetimes", "", "uncontrolledLifetimes", argType::Required, "temporal", "uncontrolledLifetimes", false, "bool", "If true, lifetimes are calculated for uncontrolled modes.  Default is false.");
   config.add("lifetimeTrials", "", "lifetimeTrials", argType::Required, "temporal", "lifetimeTrials", false, "int", "Number of trials to use for calculating speckle lifetimes.  If 0, lifetimes are not calcualted.");
   config.add("writePSDs", "", "writePSDs", argType::True, "temporal", "writePSDs", false, "bool", "Flag.  If set then output PSDs are written to disk.");
   
}

template<typename realT>
void mxAOSystem_app<realT>::loadConfig()
{
   std::vector<realT> vecTmp;
   
   
   /**********************************************************/
   /* App setup                                              */
   /**********************************************************/
   config(doHelp, "help");
      
   config(mode, "mode");
   
   config(wfeUnits, "wfeUnits");
   
   config(mnMap, "mnMap");
   
   /**********************************************************/
   /* Models                                                 */
   /**********************************************************/
   //These are called before anything else, so all parameters are modifications.
   std::string model;
   config(model, "model");
   
   if(model != "")
   {
      if( model == "Guyon2005" ) aosys.loadGuyon2005(); 
      else if( model == "MagAOX" ) aosys.loadMagAOX();
      else if( model == "GMagAOX" ) aosys.loadGMagAOX();
      else
      {
         std::cerr << "Unknown model: " << model << "\n";
         return;
      }
   }
   
   /**********************************************************/
   /* Atmosphere                                             */
   /**********************************************************/
   
   //The order of l0, Cn2, and r_0 is so that r_0 overrides the value set with Cn2 if l0 != 0.
   //lam_0 comes first because it calibrates r0 and Cn2
   config(lam_0, "lam_0");
   
   //layer Cn2
   if( config.isSet("layer_Cn2") )
   {
      std::vector<realT> lcn2 = aosys.atm.layer_Cn2();
      config(lcn2, "layer_Cn2");
      aosys.atm.layer_Cn2(lcn2, lam_0);
   }
   
   //r_0
   if(config.isSet("r_0") )
   {
      realT r_0;
      config(r_0, "r_0");
      aosys.atm.r_0(r_0, lam_0);
   }
   
   //Outer scale
   if(config.isSet("L_0") )
   {
      realT L_0;
      config(L_0, "L_0");
      aosys.atm.L_0(L_0);
   }
   
   //layer winds
   if( config.isSet("layer_v_wind") )
   {
      std::vector<realT> lvw = aosys.atm.layer_v_wind();
      config(lvw, "layer_v_wind");
      
      aosys.atm.layer_v_wind(lvw);
   }
   
   //layer direction
   if( config.isSet("layer_dir") )
   {
      std::vector<realT> ld = aosys.atm.layer_dir();
      config(ld, "layer_dir");
      aosys.atm.layer_dir(ld);
   }
 
   if( config.isSet("layer_z") )
   {
      std::vector<realT> lz = aosys.atm.layer_z();
      config(lz, "layer_z");
      
      aosys.atm.layer_z(lz);
   }
   

   //v_wind --> this rescales layer_v_wind
   if(config.isSet("v_wind") )
   {
      realT  vw = aosys.atm.v_wind();
      config(vw, "v_wind");
      aosys.atm.v_wind(vw);
   }

   //z_mean --> this rescales layer_z
   if(config.isSet("z_mean") )
   {
      realT zm = aosys.atm.z_mean();
      config(zm, "z_mean");
      aosys.atm.z_mean(zm);
   }

   /**********************************************************/
   /* PSD                                                    */
   /**********************************************************/
   if(config.isSet("subTipTilt"))
   {
      bool subtt;
      config.get(subtt, "subTipTilt");
      aosys.psd.subTipTilt(subtt);
   }
   
   if(config.isSet("scintillation"))
   {
      bool scint = aosys.psd.scintillation();
      config(scint, "scintillation");
      
      aosys.psd.scintillation(scint);
   }
   
   if(config.isSet("component"))
   {
      std::string comp;
      
      config(comp, "component");
      
      if(comp == "phase") aosys.psd.component(mx::AO::analysis::PSDComponent::phase);
      else if(comp == "amplitude") aosys.psd.component(mx::AO::analysis::PSDComponent::amplitude);
      else if(comp == "dispPhase") aosys.psd.component(mx::AO::analysis::PSDComponent::dispPhase);
      else if(comp == "dispAmplitude") aosys.psd.component(mx::AO::analysis::PSDComponent::dispAmplitude);
      else
      {
         
      }
   }
   
   
   /**********************************************************/
   /* System                                                 */
   /**********************************************************/
   //WFS 
   if(config.isSet("wfs"))
   {
      std::string wfs;
      config(wfs, "wfs");
      
      if(wfs == "ideal")
      {
         aosys.wfsBeta(idealWFS);// = &idealWFS;
      }
      else if(wfs == "unmodPyWFS")
      {
         aosys.wfsBeta(unmodPyWFS);// = &unmodPyWFS;
      }
      else if(wfs == "asympModPyWFS")
      {
         aosys.wfsBeta(asympModPyWFS); // = &asympModPyWFS;
      }
      else
      {
         std::cerr << "Unkown WFS type\n";
         exit(-1);
      }
   }
   
   //diameter
   if(config.isSet("D") )
   {
      realT D = aosys.D();
      config(D, "D");
      aosys.D(D);
   }
   
   //d_min
   if(config.isSet("d_min") )
   {
      realT d_min = aosys.d_min();
      config(d_min, "d_min");
      aosys.d_min(d_min);
   }
      
   if(config.isSet("optd"))
   {
      bool optd = true;
      config(optd, "optd");
      aosys.optd(optd);
   }
   
   realT optd_delta = aosys.optd_delta();
   config(optd_delta, "optd_delta");
   aosys.optd_delta(optd_delta);
   
   if(config.isSet("circularLimit"))
   {
      bool cl = true;
      config(cl, "circularLimit");
      aosys.circularLimit(cl);
   }
   
   //F0
   if(config.isSet("F0") )
   {
      realT F0 = aosys.F0();
      config(F0, "F0");
      aosys.F0(F0);
   }
   
   
   //lam_wfs
   if(config.isSet("lam_wfs") )
   {
      realT lam_wfs = aosys.lam_wfs();
      config(lam_wfs, "lam_wfs");
      
      aosys.lam_wfs(lam_wfs);
   }
   
   //npix_wfs
   if(config.isSet("npix_wfs") )
   {
      realT npix_wfs = aosys.npix_wfs();
      config(npix_wfs, "npix_wfs");
      aosys.npix_wfs(npix_wfs);
   }
      
   //ron_wfs
   if(config.isSet("ron_wfs") )
   {
      realT rwfs = aosys.ron_wfs();
      config( rwfs, "ron_wfs");
      aosys.ron_wfs(rwfs);
   }
      
   //Fbg
   realT Fbg = aosys.Fbg();
   config( Fbg, "Fbg");
   aosys.Fbg(Fbg);
   
   
   if(config.isSet("bin_npix"))
   {
      bool bin_npix = true;
      config(bin_npix, "bin_npix");
      aosys.bin_npix(bin_npix);
   }
   
   //minTauWFS
   realT mtwfs = aosys.minTauWFS();
   config( mtwfs, "minTauWFS");
   aosys.minTauWFS(mtwfs);
    
   //tauWFS
   realT twfs = aosys.tauWFS();
   config( twfs, "tauWFS");
   aosys.tauWFS(twfs);
   
   
   //deltaTau
   realT dt = aosys.deltaTau();
   config( dt, "deltaTau");
   aosys.deltaTau(dt);
      
   bool optTau = aosys.optTau();
   config(optTau, "optTau");
   aosys.optTau(optTau);
   
   //lam_sci
   if(config.isSet("lam_sci") )
   {
      realT lsci = aosys.lam_sci();
      config( lsci, "lam_sci");
      aosys.lam_sci( lsci );
   }
   
   //zeta
   if(config.isSet("zeta") )
   {
      realT zeta = aosys.zeta();
      config(zeta , "zeta");
      aosys.zeta(zeta);
   }
   
   //fit_mn_max
   if(config.isSet("fit_mn_max") )
   {
      realT fmnm = aosys.fit_mn_max();
      config( fmnm, "fit_mn_max");
      aosys.fit_mn_max(fmnm);
   }
   
   //ncp_wfe
   if(config.isSet("ncp_wfe") )
   {
      realT nwfe = aosys.ncp_wfe();
      config( nwfe, "ncp_wfe");
      aosys.ncp_wfe(nwfe);
   }
    
   //ncp_alpha
   if(config.isSet("ncp_alpha") )
   {
      realT na = aosys.ncp_alpha();
      config( na, "ncp_alpha");
      aosys.ncp_alpha( na );
   }
      
   //star_mag
   if(config.isSet("starMag") )
   {
      realT smag = aosys.starMag();
      config( smag, "starMag");
      aosys.starMag( smag );
   }
   
   if( config.isSet("starMags") )
   {
      config( starMags, "starMags");
   }
   
   /**********************************************************/
   /* Temporal PSDs                                          */
   /**********************************************************/
   config(fmax, "fmax");
   config(dfreq, "dfreq");
   config(k_m, "k_m");
   config(k_n, "k_n");
   
   config(gridDir, "gridDir");
   config(subDir, "subDir");
   config(lpNc, "lpNc");

   config(m_uncontrolledLifetimes, "uncontrolledLifetimes");
   config(m_lifetimeTrials, "lifetimeTrials");   
   config(writePSDs, "writePSDs");

   
   if(config.m_unusedConfigs.size() > 0)
   {
      std::cerr<< "****************************************************\n";
      std::cerr << "WARNING: unrecognized config options:\n";
      
      for( auto it = config.m_unusedConfigs.begin(); it != config.m_unusedConfigs.end(); ++it )
      {
         std::cerr << "   " << it->second.name;
         if(config.m_sources) std::cerr << " [" << it->second.sources[0] <<"]\n";
         else std::cerr << "\n";
      }
      
      std::cerr<< "****************************************************\n";
   }
   
   if(config.nonOptions.size() > 0)
   {
      std::cerr<< "****************************************************\n";
      std::cerr << "WARNING: unrecognized command line arguments\n";
   }
}





template<typename realT>
int mxAOSystem_app<realT>::execute()
{
   int rv;
   
   if(mode == "C0Raw")
   {
      rv = C0Raw();
   }
   else if (mode == "C0Map")
   {
      rv = C0Map();
   }
   else if(mode == "C1Raw")
   {
      rv = C1Raw();
   }
   else if (mode == "C1Map")
   {
      rv = C1Map();
   }
   else if(mode == "C2Raw")
   {
      rv = C2Raw();
   }
   else if (mode == "C2Map")
   {
      rv = C2Map();
   }
   else if(mode == "C4Raw")
   {
      rv = C4Raw();
   }
   else if (mode == "C4Map")
   {
      rv = C4Map();
   }
   else if(mode == "C6Raw")
   {
      rv = C6Raw();
   }
   else if (mode == "C6Map")
   {
      rv = C6Map();
   }
   else if(mode == "C7Raw")
   {
      rv = C7Raw();
   }
   else if (mode == "C7Map")
   {
      rv = C7Map();
   }
   else if (mode == "CAllRaw")
   {
      rv = CAllRaw();
   }
   else if (mode == "CProfAll")
   {
      rv = CProfAll();
   }
   else if (mode == "ErrorBudget")
   {
      rv = ErrorBudget();
   }
   else if (mode == "Strehl")
   {
      rv = Strehl();
   }
   else if (mode == "temporalPSD")
   {
      rv = temporalPSD();
   }
   else if (mode == "temporalPSDGrid")
   {
      rv = temporalPSDGrid();
   }
   else if (mode == "temporalPSDGridAnalyze")
   {
      std::cerr << "1\n";
      rv = temporalPSDGridAnalyze();
   }
   else
   {
      std::cerr << "Unknown mode: " << mode << "\n";
      rv = -1;
   }
   
   
   
   if(dumpSetup && rv == 0)
   {
      std::ofstream fout;
      fout.open(setupOutName);
      
      aosys.dumpAOSystem(fout);
      
      fout.close();
   }
   
   return rv;
}

template<typename realT>
int mxAOSystem_app<realT>::C_MapCon( const std::string & mapFile,
                                     imageT & map
                                   )
{
   imageT im, psf;
   
   psf.resize(map.rows(),map.cols());
   for(int i=0;i<psf.rows();++i)
   {
      for(int j=0;j<psf.cols();++j)
      {
         psf(i,j) = mx::math::func::airyPattern(sqrt( pow( i-floor(.5*psf.rows()),2) + pow(j-floor(.5*psf.cols()),2)));
      }
   }
   
   mx::AO::analysis::varmapToImage(im, map, psf);
   
   for(int i=0; i< mnMap; ++i)
   {
      std::cout << i << " " << im( mnMap+1, mnMap+1 + i) << "\n";
   }
   
   mx::improc::fitsFile<realT> ff;
   ff.write(mapFile, im);

   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C0Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C0Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C0Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C0(i,0, false) << "\n";
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C0Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C0Map(map);
   
   C_MapCon("C0Map.fits", map);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C1Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C1Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C1Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C1(i,0, false) << "\n";
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C1Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C1Map(map);
   
   C_MapCon("C1Map.fits", map);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C2Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C2Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C2Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C2(i,0, false) << "\n";
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C2Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C2Map(map);
   
   C_MapCon("C2Map.fits", map);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C4Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C4Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C4Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C4(i,0, false) << "\n";
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C4Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C4Map(map);
   
   C_MapCon("C4Map.fits", map);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::C6Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C6Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C6Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C6(i,0, false) << "\n";
   }
   
   return 0;
   
}

template<typename realT>
int mxAOSystem_app<realT>::C6Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C6Map(map);
   
   C_MapCon("C6Map.fits", map);
   
   return 0;

}


template<typename realT>
int mxAOSystem_app<realT>::C7Raw()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   aosys.C7Map(map);
   
   mx::improc::fitsFile<realT> ff;
   ff.write("C7Raw.fits", map);
   
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C7(i,0, false) << "\n";
   }
   
   return 0;
   
}

template<typename realT>
int mxAOSystem_app<realT>::C7Map()
{
   imageT map;
   
   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   aosys.C7Map(map);
   
   C_MapCon("C7Map.fits", map);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::CAllRaw()
{
   for(int i=0;i< aosys.fit_mn_max(); ++i)
   {
      std::cout << i << " " << aosys.C0(i,0, false) << " " << aosys.C1(i,0, false) << " " << aosys.C2(i,0, false) << " " << aosys.C4(i,0, false);
      std::cout << " " << aosys.C6(i,0, false) << " " << aosys.C7(i,0, false) << "\n";
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::CProfAll()
{
   imageT map, im0, im1, im2, im4, im6, im7;

   map.resize( mnMap*2+1, mnMap*2 + 1);
   
   imageT psf;
   
   psf.resize(map.rows(),map.cols());
   for(int i=0;i<psf.rows();++i)
   {
      for(int j=0;j<psf.cols();++j)
      {
         psf(i,j) = mx::math::func::airyPattern(sqrt( pow( i-floor(.5*psf.rows()),2) + pow(j-floor(.5*psf.cols()),2)));
      }
   }
   
   aosys.C0Map(map);
   mx::AO::analysis::varmapToImage(im0, map, psf);
   
   aosys.C1Map(map);
   mx::AO::analysis::varmapToImage(im1, map, psf);

   aosys.C2Map(map);
   mx::AO::analysis::varmapToImage(im2, map, psf);
   
   aosys.C4Map(map);
   mx::AO::analysis::varmapToImage(im4, map, psf);
   
   aosys.C6Map(map);
   mx::AO::analysis::varmapToImage(im6, map, psf);
   
   aosys.C7Map(map);
   mx::AO::analysis::varmapToImage(im7, map, psf);
   
   std::cout << "#PSF-convolved PSF profiles.\n";
   std::cout << "#Sep    C0    C1    C2    C4     C6    C7\n";
   for(int i=0;i< mnMap; ++i)
   {
      std::cout << i << " " << im0( mnMap+1, mnMap+1 + i) << " " << im1( mnMap+1, mnMap+1 + i) << " " << im2( mnMap+1, mnMap+1 + i) << " ";
      std::cout << im4( mnMap+1, mnMap+1 + i) << " " << im6( mnMap+1, mnMap+1 + i) << " " << im7( mnMap+1, mnMap+1 + i) << "\n";      
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::ErrorBudget()
{
   realT units = 1;
   
   if(wfeUnits == "nm")
   {
      std::cerr << aosys.lam_sci() << "\n";
      units = aosys.lam_sci() / (2.0*pi<realT>()) / 1e-9;
   }
   
   if(starMags.size() == 0)
   {
      std::cout << "Measurement: " << sqrt(aosys.measurementError())*units << "\n";
      std::cout << "Time-delay:  " << sqrt(aosys.timeDelayError())*units << "\n";
      std::cout << "Fitting:     " << sqrt(aosys.fittingError())*units << "\n";
      std::cout << "NCP error:   " << sqrt(aosys.ncpError())*units << "\n";
      std::cout << "Strehl:      " << aosys.strehl() << "\n";
   }
   else
   {
      std::cout << "#mag     d_opt        Measurement     Time-delay      Fitting    Chr-Scint-OPD      Chr-Index   Disp-Ansio-OPD  NCP-error         Strehl\n";
      
      for(size_t i=0; i< starMags.size(); ++i)
      {
               
         aosys.starMag(starMags[i]);
         std::cout << starMags[i] << "\t    ";
         std::cout << aosys.d_opt() << "\t  ";
         std::cout << sqrt(aosys.measurementError())*units << "\t   ";
         std::cout << sqrt(aosys.timeDelayError())*units << "\t ";
         std::cout << sqrt(aosys.fittingError())*units << "\t ";
         std::cout << sqrt(aosys.chromScintOPDError())*units << "\t    ";
         std::cout << sqrt(aosys.chromIndexError())*units << "\t\t    ";
         std::cout << sqrt(aosys.dispAnisoOPDError())*units << "\t    ";
         std::cout << sqrt(aosys.ncpError())*units << "\t\t";
         std::cout << aosys.strehl() << "\n";
      }
   }
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::Strehl()
{
   std::cout << aosys.strehl() << "\n";
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::temporalPSD()
{
   std::vector<realT> freq, psdOL, psdN, psdSI, psdLP;

   mx::AO::analysis::fourierTemporalPSD<realT, aosysT> ftPSD;
   ftPSD.m_aosys = &aosys;
   
   if(aosys.minTauWFS() <= 0)
   {
      std::cerr << "temporalPSD: You must set minTauWFS to be > 0 to specify loop frequency.\n";
      return -1;
   }
   
   if(dfreq <= 0)
   {
      std::cerr << "temporalPSD: You must set dfreq to be > 0 to specify frequency sampling.\n";
      return -1;
   }
   
   realT fs = 1.0/aosys.minTauWFS();
   
   mx::math::vectorScale(freq, 0.5*fs/dfreq, dfreq, dfreq);
   psdOL.resize(freq.size());
   
   ftPSD.multiLayerPSD( psdOL, freq, k_m, k_n, 1, fmax);
   
   //Create WFS Noise PSD
   psdN.resize(freq.size());
   mx::AO::analysis::wfsNoisePSD<realT>( psdN, aosys.beta_p(k_m,k_n), aosys.Fg(), (1.0/fs), aosys.npix_wfs(), aosys.Fbg(), aosys.ron_wfs());
   
   //optimize
   mx::AO::analysis::clAOLinearPredictor<realT> tflp;
            
   mx::AO::analysis::clGainOpt<realT> go_si(1.0/fs, 1.5/fs);
   mx::AO::analysis::clGainOpt<realT> go_lp(1.0/fs, 1.5/fs);
            
   go_si.f(freq);
   
   realT gmaxSI = 0;
   realT varSI;
   realT goptSI = go_si.optGainOpenLoop(varSI, psdOL, psdN, gmaxSI);

   //Only bother if number of coefficients is > 1.
   realT goptLP = -1;
   realT varLP = -1;
   if(lpNc > 1)
   {
      go_lp.f(freq);
      realT gmaxLP;
      tflp.regularizeCoefficients( gmaxLP, goptLP, varLP, go_lp, psdOL, psdN, lpNc);      
   }
   
   std::cout << "# aoSystem single temporal PSD\n";
   std::cout << "#    var OL = " << "\n";
   std::cout << "#    opt-gain SI = " << goptSI << "\n";
   std::cout << "#    var SI = " << varSI << "\n";
   std::cout << "#    LP Num. coeff = " << lpNc << "\n";
   std::cout << "#    opt-gain LP = " << goptLP << "\n";
   std::cout << "#    var LP = " << varLP << "\n";
   std::cout << "#################################################################\n";
   std::cout << "# freq    PSD-OL    PSD-N    ETF-SI  NTF-SI   ETF-LP   NTF-LP \n";
   
   realT ETF_SI, NTF_SI, ETF_LP = -1, NTF_LP =-1;
   for(size_t i=0; i < freq.size(); ++i)
   {
      go_si.clTF2(ETF_SI, NTF_SI, i, goptSI);
      
      if(goptLP != -1)
      {
         go_lp.clTF2(ETF_LP, NTF_LP, i, goptLP);
      }
      
      std::cout << freq[i] << " " << psdOL[i] << " " << psdN[i] << " " << ETF_SI << " " << NTF_SI << " " << ETF_LP << " " << NTF_LP << "\n";
   }
   
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::temporalPSDGrid()
{
   mx::AO::analysis::fourierTemporalPSD<realT, aosysT> ftPSD;
   ftPSD.m_aosys = &aosys;
   
   if(gridDir == "")
   {
      std::cerr << "temporalPSDGrid: You must set gridDir.\n";
      return -1;
   }
   
   if(aosys.fit_mn_max() <= 0)
   {
      std::cerr << "temporalPSDGrid: You must set fit_mn_max to be > 0.\n";
      return -1;
   }
   

   if(dfreq <= 0)
   {
      std::cerr << "temporalPSDGrid: You must set dfreq to be > 0 to specify frequency sampling.\n";
      return -1;
   }
   
   if(aosys.minTauWFS() <= 0)
   {
      std::cerr << "temporalPSDGrid: You must set minTauWFS to be > 0 to specify loop frequency.\n";
      return -1;
   }
   
   realT fs = 1.0/aosys.minTauWFS();
   
   ftPSD.makePSDGrid( gridDir, aosys.fit_mn_max(), dfreq, fs, 0);
   
   return 0;
}

template<typename realT>
int mxAOSystem_app<realT>::temporalPSDGridAnalyze()
{
   mx::AO::analysis::fourierTemporalPSD<realT, aosysT> ftPSD;
   ftPSD.m_aosys = &aosys;
   
   if(gridDir == "")
   {
      std::cerr << "temporalPSDGridAnalyze: You must set gridDir.\n";
      return -1;
   }
   
   if(subDir == "")
   {
      std::cerr << "temporalPSDGridAnalyze: You must set subDir.\n";
      return -1;
   }
   
   if(aosys.fit_mn_max() <= 0)
   {
      std::cerr << "temporalPSDGridAnalyze: You must set fit_mn_max to be > 0.\n";
      return -1;
   }
      
   int mnCon = aosys.D()/aosys.d_min()/2;
   

   std::vector<realT> mags;
   
   if(starMags.size() == 0)
   {
      mags = { aosys.starMag() };
   }
   else
   {
      mags = starMags;
   }
   
   return ftPSD.analyzePSDGrid( subDir, gridDir, aosys.fit_mn_max(), mnCon, lpNc, mags, m_lifetimeTrials, m_uncontrolledLifetimes, writePSDs); 
   
}

int main(int argc, char ** argv)
{
   mx::fftwEnvironment<double,false> fftwEnv;
 
   mxAOSystem_app<double> aosysA;
   
   return aosysA.main(argc, argv);
   
}


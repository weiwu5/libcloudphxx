#include <cstdlib> // system()
#include <set>
#include <string>
#include <sstream> // std::ostringstream

#include "../common.hpp"
#include "bins.hpp"

using std::ostringstream;
using std::set;
using std::string;

int main(int ac, char** av)
{
  if (ac != 2) error_macro("expecting one argument - CMAKE_BINARY_DIR");

  string bins_wet_str, bins_dry_str;

  {
    ostringstream tmp;
    vector<quantity<si::length>> left_edges = bins_dry();
    for (int i = 0; i < left_edges.size()-1; ++i)
      tmp << float(left_edges[i] / si::metres) << ":" << float(left_edges[i + 1] / si::metres) << "|0;";
    bins_dry_str = tmp.str();
  }

  {
    ostringstream tmp;
    vector<quantity<si::length>> left_edges = bins_wet();
    for (int i = 0; i < left_edges.size()-1; ++i)
      tmp << float(left_edges[i] / si::metres) << ":" << float(left_edges[i + 1] / si::metres) << "|0;";
    bins_wet_str = tmp.str();
  }

  string opts_common = 
    "--outfreq=200  --nt=9000 --spinup=7200 --nx=76 --nz=76 --relax_th_rv=false";
  set<string> opts_micro({
    "--micro=lgrngn_chem --outdir=out_lgrngn_chem --backend=CUDA --adv_serial=False --sd_conc=128 "
                    "--sstp_cond=10 --sstp_chem=10 --coal=True --sedi=True " 
                    "--chem_switch=True --chem_dsl=True --chem_dsc=True --chem_rho=1.8e3 "  
                    //chem_rct switched on afetr spinup in set_chem  
                    "--mean_rd1=0.05e-6 --sdev_rd1=1.8 --n1_stp=50e6 "
                    "--mean_rd2=0.1e-6  --sdev_rd2=1.4 --n2_stp=0 "
                    "--SO2_g_0=.2e-9 --O3_g_0=50e-9 --H2O2_g_0=.5e-9 --CO2_g_0=360e-6 --NH3_g_0=.1e-9 --HNO3_g_0=0 "
      " --out_wet=\""                                                                                //TODO 100e-12
        ".5e-6:25e-6|0,1,2,3;" // FSSP
        "25e-6:1|0,3;"         // "rain"
        + bins_wet_str +       // aerosol spectrum (wet)
      "\""
      " --out_dry=\""
        "0.:1.|0,1;"
        + bins_dry_str +       // aerosol spectrum (dry)
      "\""
      " --out_chem=\""
        "0:1|0;"               // chem spectrum (dry)
      "\""
      " --out_wet_pH=\""
        + bins_wet_str +       // spectrum for S_VI and H+ (wet)
      "\""
  });

  for (auto &opts_m : opts_micro)
  {
    ostringstream cmd;
    cmd << av[1] << "/src/icicle " << opts_common << " " << opts_m;  
    notice_macro("about to call: " << cmd.str())

    if (EXIT_SUCCESS != system(cmd.str().c_str()))
      error_macro("model run failed: " << cmd.str())
  }
}

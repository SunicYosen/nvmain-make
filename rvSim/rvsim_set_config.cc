/* 
 * Name:  rvsim_set_config.cc
 * Author:  SunicYosen
 * Time:  2019.04.11
 * Introduction:
 */

#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>
#include <assert.h>

#include "rvSim/rvSim.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;

extern "C" int rvsim_set_config(){
	int argc;
  char *argv[4];

  argc = 4 ;
  argv[0] = (char*)"nvmain";
 	argv[1] = (char*)"/home/sun/File/RISCV/Projects/nvmain/Config/RRAM_ISSCC_2012_4GB.config";
 	argv[2] = (char*)"/home/sun/File/RISCV/Projects/nvmain/Tests/Traces/test.nvt";
 	argv[3] = (char*)"10000";

	assert(argc = 4);
  std::cout << "[+](SetConfig)-----------------------------------------------------" << std::endl;
  riscv_sim->SetConfig(argc, argv);
	return 0;
}

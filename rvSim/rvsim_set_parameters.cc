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

#include "rvSim/rvSim.h"
#include "src/Params.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;
extern GlobalParams globalparams;

extern "C" int rvsim_set_parameters()
{
  riscv_sim->setParameters();
  return 0;
}
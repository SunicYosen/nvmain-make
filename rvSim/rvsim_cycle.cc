/* 
 * Name:  rvsim_sycle.cc
 * Author:  SunicYosen
 * Time:  2019.04.11
 * Introduction:
 */

#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>

#include "rvSim/rvSim.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;

extern "C" void rvsim_cycle()
{
  riscv_sim->Cycle(1);
  return;
}
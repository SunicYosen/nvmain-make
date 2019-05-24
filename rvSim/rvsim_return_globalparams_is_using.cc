/* 
 * Name         :  rvsim_return_globalparams_is_using.cc
 * Author       :  SunicYosen
 * Time         :  2019.05.24
 * Introduction :
 */

#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>
#include <stdio.h>

#include "vpi_user.h"
#include "acc_user.h"
#include "rvSim/rvSim.h"
#include "src/Params.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;
extern GlobalParams globalparams;

// $rvsim_return_globalparams_is_using()
extern "C" int rvsim_return_globalparams_is_using()
{
  bool is_globalparams_using_flag = globalparams.isUsing;

  std::cout << "[+](Is globalparams using) globalparams is using: " << (is_globalparams_using_flag ? 1 : 0) << std::endl;

   //handle reg = acc_handle_object("tb_nvmain.vpi_test_nvmain.is_globalparams_using_flag");
   handle reg = acc_handle_object("TestDriver.testHarness.dut.tile.RoCCInterfaceImp.rocc_nvmain.exe_is_globalparams_using_flag");
   static s_setval_delay delay_s = {{0, 0, 0, 0.0},accNoDelay};
   static s_setval_value value_s = {accIntVal};
   
   if(is_globalparams_using_flag)
      {
         value_s.value.integer = 1;
      }
   else
      {
         value_s.value.integer = 0;
      }
  
   acc_set_value(reg, &value_s, &delay_s);
   return;
}
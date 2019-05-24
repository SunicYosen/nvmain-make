/* 
 * Name         :  rvsim_set_weight.cc
 * Author       :  SunicYosen
 * Time         :    2019.05.24
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

//$rvsim_set_weight((uint64_t)col, (uint64_t)row, (uint64_t)nums, (uint64_t)bitwidth)
extern "C" int rvsim_set_weight()
{
  vpiHandle sysTfH, argI;
  vpiHandle argv[5];
  s_vpi_value argv_val[5];

  PLI_UINT64 value_arg0;
  PLI_UINT64 value_arg1;
  PLI_UINT64 value_arg2;
  PLI_UINT64 value_arg3;

  argv_val[0].format = vpiIntVal;
  argv_val[1].format = vpiIntVal;
  argv_val[2].format = vpiIntVal;
  argv_val[3].format = vpiIntVal;

  sysTfH = vpi_handle(vpiSysTfCall, NULL);
  argI = vpi_iterate(vpiArgument, sysTfH);

  for (int i = 0; i < 4; i++)
  {
     argv[i] = vpi_scan(argI);
     vpi_get_value(argv[i], &argv_val[i]);
  }

  value_arg0 = argv_val[0].value.integer;
  value_arg1 = argv_val[1].value.integer;
  value_arg2 = argv_val[2].value.integer;
  value_arg3 = argv_val[3].value.integer;

  vpi_free_object(argI);

  if(!globalparams.isUsing)
	{
    uint64_t kernel_col      = value_arg0;
    uint64_t kernel_row      = value_arg1;
    uint64_t kernel_nums     = value_arg2;
    uint64_t kernel_bitwidth = value_arg3;

    printf("[+](set_weight) kernel col=%d row=%d channels=%d bitwidth=%d\n", \
                          kernel_col, kernel_row, kernel_nums, kernel_bitwidth);

		globalparams.K_Col          = kernel_col;
    globalparams.K_Row          = kernel_row;
    globalparams.K_num          = kernel_nums;
    globalparams.Weight_Width   = kernel_bitwidth;
    globalparams.K_Channel      = globalparams.Input_Channel;
		return true;
	}  

	else
  {
    printf("[-](set_kernel) Failed: globalparams is using!\n");
    return false;
  }
}
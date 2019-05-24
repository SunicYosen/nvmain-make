/* 
 * Name         :  rvsim_set_input.cc
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

//$rvsim_set_input((uint64_t)col, (uint64_t)row, (uint64_t)channels, (uint64_t)bitwidth)
extern "C" int rvsim_set_input()
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
    uint64_t input_col      = value_arg0;
    uint64_t input_row      = value_arg1;
    uint64_t input_channels = value_arg2;
    uint64_t input_bitwidth = value_arg3;

    printf("[+](set_input) Input col=%d row=%d channels=%d bitwidth=%d\n", \
                          input_col, input_row, input_channels, input_bitwidth);

		globalparams.Input_Col     = input_col;
    globalparams.Input_Row     = input_row;
    globalparams.Input_Channel = input_channels;
    globalparams.Input_Width   = input_bitwidth;
		return true;
	}  

	else
  {
    printf("[-](set_input) Failed: globalparams is using!\n");
    return false;
  }
}
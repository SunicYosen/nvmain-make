/* 
 * Name:  rvsim_set_func.cc
 * Author:  SunicYosen
 * Time:    2019.05.24
 * Introduction:
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

// $rvsim_set_func((uint64_t)func_value)
extern "C" int rvsim_set_func()
{
  vpiHandle sysTfH, argI;
  vpiHandle argv[2];
  s_vpi_value argv_val[2];

  PLI_UINT64 value_arg0;

  argv_val[0].format = vpiIntVal;

  sysTfH = vpi_handle(vpiSysTfCall, NULL);
  argI = vpi_iterate(vpiArgument, sysTfH);

  for (int i = 0; i < 1; i++)
  {
     argv[i] = vpi_scan(argI);
     vpi_get_value(argv[i], &argv_val[i]);
  }

  value_arg0 = argv_val[0].value.integer;

  vpi_free_object(argI);

  if(!globalparams.isUsing)
	{
    uint64_t func_value = value_arg0;
    printf("[+](set_func) Func_n = %d\n", func_value);
		globalparams.Func_n = 0;
		return true;
	}

	else
  {
    printf("[-](set_func) Failed: globalparams is using!\n");
    return false;
  }
}
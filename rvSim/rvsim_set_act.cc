/* 
 * Name:  rvsim_set_act.cc
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

// $rvsim_set_act((uint64_t)act_value)
extern "C" int rvsim_set_act()
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
    uint64_t act_value = value_arg0;
    printf("[+](set_act) act_n = %d\n", act_value);
    if(act_value == 0)
		  globalparams.act_mode = ACT_RELU;

    else if(act_value == 1)
      globalparams.act_mode = ACT_Tanh;

    else if(act_value == 2)
      globalparams.act_mode = ACT_Sigmoid;
    
    else
    {
      globalparams.act_mode = ACT_RELU;
      printf("[-](set_act) Unknown act mode! Please check! Recieved: %d\n", act_value);
    }
		return true;
	}

	else
  {
    printf("[-](set_act) Failed: globalparams is using!\n");
    return false;
  }
}
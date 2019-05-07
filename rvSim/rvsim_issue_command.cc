/* 
 * Name:  rvsim_issue_command.cc
 * Author:  SunicYosen
 * Time:  2019.04.11
 * Introduction:
 */

#include <iostream>
#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>
#include "stdio.h"

#include "vpi_user.h"
#include "acc_user.h"
#include "rvSim/rvSim.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;

extern "C" void rvsim_issue_command()
{
   vpiHandle sysTfH, argI;
   vpiHandle argv[6];
   s_vpi_value argv_val[6];

   PLI_UBYTE8 value_arg0;
   PLI_UINT32 value_arg1;
   PLI_UINT32 value_arg2;
   PLI_UINT32 value_arg3;
   PLI_UBYTE8 value_arg4;

   argv_val[0].format = vpiIntVal;
   argv_val[1].format = vpiIntVal;
   argv_val[2].format = vpiIntVal;
   argv_val[3].format = vpiIntVal;
   argv_val[4].format = vpiIntVal;

   sysTfH = vpi_handle(vpiSysTfCall, NULL);
   argI = vpi_iterate(vpiArgument, sysTfH);

   for (int i = 0; i < 5; i++)
   {
      argv[i] = vpi_scan(argI);
      vpi_get_value(argv[i], &argv_val[i]);
   }

   value_arg0 = argv_val[0].value.integer;
   value_arg1 = argv_val[1].value.integer;
   value_arg2 = argv_val[2].value.integer;
   value_arg3 = argv_val[3].value.integer;
   value_arg4 = argv_val[4].value.integer;

   vpi_free_object(argI);

   if(value_arg0 == 'C' | value_arg0 == 'c')
   {
      riscv_sim -> IssueCommand((uint64_t)value_arg1, (uint64_t)value_arg3, \
           (char)value_arg0, (uint64_t)value_arg2, (char)value_arg4);

      printf("[+](Issue Command) Issue Command: [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n", \
             value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);
   }

   else if(value_arg0 == 'L' | value_arg0 == 'l' | value_arg0 == 'W' | \
           value_arg0 == 'w' | value_arg0 == 'R' | value_arg0 == 'r')
   {
      riscv_sim -> IssueCommand((uint64_t)value_arg1, (char)value_arg0, \
                                (uint64_t)value_arg2, (uint64_t)value_arg3);

      printf("[+](Issue Command) Issue Command: [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n", \
             value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);
   }

   else
   {
      printf("[-]Issue Command Error get opt! [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n", \
             value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);

      exit(0);
   }

   return;
}

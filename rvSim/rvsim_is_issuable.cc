/* 
 * Name         :  rvsim_is_issuable.cc
 * Author       :  SunicYosen
 * Time         :  2019.04.11
 * Introduction :
 */

#include <iostream>
#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>

#include "vpi_user.h"
#include "acc_user.h"
#include "rvSim/rvSim.h"
#include "stdio.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;

extern "C" void rvsim_is_issuable()
{
   // vpiHandle sysTfH, argI;
   // vpiHandle arg0, arg1, arg2, arg3, arg4;
   // s_vpi_value arg0_val, arg1_val, arg2_val, arg3_val, arg4_val;

   // PLI_UBYTE8 value_arg0;
   // PLI_UINT32 value_arg1;
   // PLI_UINT32 value_arg2;
   // PLI_UINT32 value_arg3;
   // PLI_UBYTE8 value_arg4;

   // arg0_val.format = vpiIntVal;
   // arg1_val.format = vpiIntVal;
   // arg2_val.format = vpiIntVal;
   // arg3_val.format = vpiIntVal;
   // arg4_val.format = vpiIntVal;

   // sysTfH = vpi_handle(vpiSysTfCall, NULL);
   // argI = vpi_iterate(vpiArgument, sysTfH);

   // arg0 = vpi_scan(argI);
   // vpi_get_value(arg0, &arg0_val);

   // arg1 = vpi_scan(argI);
   // vpi_get_value(arg1, &arg1_val);

   // arg2 = vpi_scan(argI);
   // vpi_get_value(arg2, &arg2_val);

   // arg3 = vpi_scan(argI);
   // vpi_get_value(arg3, &arg3_val);

   // arg4 = vpi_scan(argI);
   // vpi_get_value(arg4, &arg4_val);

   // value_arg0 = arg0_val.value.integer;
   // value_arg1 = arg1_val.value.integer;
   // value_arg2 = arg2_val.value.integer;
   // value_arg3 = arg3_val.value.integer;
   // value_arg4 = arg4_val.value.integer;

   // //printf("value_arg0 = %c \n", value_arg0);

   // vpi_free_object(argI);
   
   bool is_issuable_flag=0;

   is_issuable_flag = riscv_sim -> IsIssuable();

   // if(value_arg0 == 'C' | value_arg0 == 'c')
   // {
   //    is_issuable_flag = riscv_sim -> IsIssuable ((uint64_t)value_arg1, \
   //                          (uint64_t)value_arg3, (char)value_arg0, \
   //                          (uint64_t)value_arg2, (char)value_arg4);

   //    printf("[+](Is Issuable) Is Issuable comand: [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n",\
   //           value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);
   // }

   // else if(value_arg0 == 'L' | value_arg0 == 'l' | value_arg0 == 'W' | \
   //         value_arg0 == 'w' | value_arg0 == 'R' | value_arg0 == 'r')
   // {
   //    is_issuable_flag = riscv_sim -> IsIssuable((uint64_t)value_arg1, \
   //                                               (char)value_arg0, \
   //                                               (uint64_t)value_arg2, \
   //                                               (uint64_t)value_arg3);

   //    printf("[+](Is Issuable) Is Issuable comand: [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n", \
   //           value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);
   // }

   // else
   // {
   //    printf("[+](Is Issuable) Is Issuable get wrong opt: [%c, 0x%.8x, 0x%.8x, 0x%.8x, %c] \n", \
   //           value_arg0, value_arg1, value_arg2, value_arg3, value_arg4);
   //    exit(0); 
   // }

   std::cout << "[+](Is Issuable) Is Issuable a command: " << (is_issuable_flag ? 1 : 0) << std::endl;

   //handle reg = acc_handle_object("tb_nvmain.vpi_test_nvmain.is_issuable_flag");
   handle reg = acc_handle_object("TestDriver.testHarness.dut.tile.RoCCInterfaceImp.rocc_nvmain.exe_is_issuable");
   static s_setval_delay delay_s = {{0, 0, 0, 0.0},accNoDelay};
   static s_setval_value value_s = {accIntVal};
   
   if(is_issuable_flag)
      {
         value_s.value.integer = 1;
      }
   else
      {
         value_s.value.integer = 0;
      }
      
   
   //value_s.value.integer = (PLI_UBYTE8)(*(unsigned char *)&is_issuable_flag);  
   //Turn float to binary according to ieee standards.

   acc_set_value(reg, &value_s, &delay_s);
   return;
}

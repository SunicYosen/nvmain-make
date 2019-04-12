/* 
 * Name:  rvsim_issue_command.cc
 * Author:  SunicYosen
 * Time:  2019.04.11
 * Introduction:
 */


#include <sstream>
#include <cmath>
#include <stdlib.h>
#include <fstream>

#include "src/Interconnect.h"
#include "Interconnect/InterconnectFactory.h"
#include "src/Config.h"
#include "src/TranslationMethod.h"
#include "traceReader/TraceReaderFactory.h"
#include "src/AddressTranslator.h"
#include "Decoders/DecoderFactory.h"
#include "src/MemoryController.h"
#include "MemControl/MemoryControllerFactory.h"
#include "Endurance/EnduranceDistributionFactory.h"
#include "SimInterface/NullInterface/NullInterface.h"
#include "include/NVMHelpers.h"
#include "Utils/HookFactory.h"
#include "src/EventQueue.h"
#include "NVM/nvmain.h"
#include "rvSim/rvSim.h"

using namespace NVM;

//Define some params
extern rvSim *riscv_sim;

extern "C" void rvsim_issue_command()
{
   riscv_sim->IssueCommand( 384, 'R', 1223, 0);
   return;
}

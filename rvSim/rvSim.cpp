// By Mengyu Guo

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

//Define some global params
GlobalParams globalparams;
rvSim *riscv_sim = new rvSim();

/*
int main()
{
	char start;

	std::cout << "[+] Ready? Y/N" << std::endl;
	std::cin >> start;
	
	if( ( start == 'Y' ) || ( start == 'y' ) )
	{
        int argc;
	    char *argv[4];
	
	    argc = 4 ;
	    argv[0] = (char*)"nvmain";
    	argv[1] = (char*)"Config/RRAM_ISSCC_2012_4GB.config";
    	argv[2] = (char*)"Tests/Traces/test.nvt";
    	//argv[3] = (char*)"1000000";
    	argv[3] = (char*)"10000";
      
			rvSim *riscv_sim = new rvSim( );
		
		if (riscv_sim->setParameters( ))
		{		}
		else
		{
			std::cout << "[+] something wrong" << std::endl;
		}
		
    //std::cout << "[+] test global params " << globalparams.K_Col << std::endl;

		assert(argc = 4);

		int result;
        std::cout << "[+] //-----------------------------------------//" << std::endl;
        
    riscv_sim->SetConfig( argc, argv );
		//riscv_sim->IssueCommand( 384, 'L', 12312, 0 );
		//riscv_sim->IssueCommand( 384, 191991292, 'C', 12331, 'X');
		int command=0;
		while (true)
		{
			char conti;
			std::cout << "[+]  continue cycle ? Y/N " << " currentCycle is " << riscv_sim->getCycle() << std::endl;
			std::cin >> conti;
			if ((conti == 'Y') || (conti == 'y'))
			{
				if(riscv_sim->IsIssuable( ))
				{
					if(command < 1)
					{
						//std::cout << "[+] I send a load command" << std::endl;
						
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'X');
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'X');
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'X');
						
						// riscv_sim->IssueCommand(333,'T', 123123, 0, 'I', 128 );
						// riscv_sim->IssueCommand(333,'T', 123123, 0, 'I', 128 );
						// riscv_sim->IssueCommand(333,'T', 123123, 0, 'O', 128 );
						// riscv_sim->IssueCommand(333,'T', 123123, 0, 'O', 128 );
						
						// riscv_sim->IssueCommand( 333, 'T', 123123, 0, 'O', 128 );
						// riscv_sim->IssueCommand( 333, 'T', 0, 0, 'O', 128 ); // transfer 128 bit data
						// riscv_sim->IssueCommand( 333, 'T', 123123, 0, 'O', 128 );
						// riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						// riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						riscv_sim->IssueCommand( 0, 0, 'C', 11, 'X');
						riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						riscv_sim->IssueCommand( 0, 0, 'C', 11, 'X');
						riscv_sim->IssueCommand( 0, 0, 'C', 11, 'Y');
						
						//riscv_sim->IssueCommand( 384, 'L', 12312, 0);
						command++;
					}
					else
					{
						std::cout << "[+] there are no commands" << std::endl;
					}	
				}
				else
				{
					std::cout << "[+] the queue is full" << std::endl;
				}

				riscv_sim->Cycle(1000000);
			}
		}
        std::cout << "[+] All is done" << std::endl ;
        std::cout << "[+] //-----------------------------------------//" << std::endl;
        return result;
    }
	else if( ( start == 'N' ) || ( start == 'n' ) )
	{	
		std::cout << "[+] //-----------------------------------------//" << std::endl;
		std::cout << "[+] Nothing happened" << std::endl ;
		std::cout << "[+] //-----------------------------------------//" << std::endl;
		return 0;
	}
	else
	{
		std::cout << "[+] wrong answer" << std::endl;
		std::cout << "[+] Program is stopped" << std::endl;
	}
}
*/

rvSim::rvSim()
{
	
}

rvSim::~rvSim()
{
	
}

bool rvSim::setParameters()
{
	if(!globalparams.isUsing)
	{
		//std::cout << "[+](setParameters) Test" << std::endl;
		globalparams.Func_n = 0;
    globalparams.Input_Row = 5; //28;
    globalparams.Input_Col = 5; //28;
    globalparams.Input_Channel = 3;
    globalparams.BitWidth = 4;
    globalparams.K_Row = 3;
    globalparams.K_Col = 3;
    globalparams.K_Channel = globalparams.Input_Channel;
    globalparams.K_num = 32;
    globalparams.Input_Width = 8;
    globalparams.Weight_Width = 8;
    globalparams.act_mode = ACT_RELU;
    globalparams.pooling_mode = Pooling_Max;
		globalparams.Buffer_n = 4;

		//globalparams.Input_Addr.SetPhysicalAddress(0);
		//globalparams.Output_Addr.SetPhysicalAddress(100000000);
		
		return true;
	}

	else 
		return false;
}

void rvSim::SetConfig( int argc, char *argv[] )
{
	if (argc != 4)
	{
		std::cout << "[+] Warnning! Warnning!";
		return ;
	}
	/* Init */
	stats = new Stats( );
	config = new Config( );
	//trace = NULL;
	//tl = new TraceLine( );
	simInterface = new NullInterface( );
	nvmain = new NVMain( );
	mainEventQueue = new EventQueue( );
	globalEventQueue = new GlobalEventQueue( );
	tagGenerator = new TagGenerator( 1000 );
	
  config->Read( argv[1] );
  config->SetSimInterface( simInterface );
  SetEventQueue( mainEventQueue );
  SetGlobalEventQueue( globalEventQueue );
  SetStats( stats );
  SetTagGenerator( tagGenerator );
  std::ofstream statStream;	

  if( config->KeyExists( "StatsFile" ) )
  {
      statStream.open( config->GetString( "StatsFile" ).c_str(), 
                       std::ofstream::out | std::ofstream::app );
  }
  
	/*  Add any specified hooks */
  std::vector<std::string>& hookList = config->GetHooks( );

  for( size_t i = 0; i < hookList.size( ); i++ )
  {
    //std::cout << "[+] Creating hook " << hookList[i] << std::endl;

    NVMObject *hook = HookFactory::CreateHook( hookList[i] );
    
    if( hook != NULL )
    {
        AddHook( hook );
        hook->SetParent( this );
        hook->Init( config );
    }
    else
    {
        //std::cout << "[+] Warning: Could not create a hook named `" 
        //    << hookList[i] << "'." << std::endl;
    }
  }
    
  AddChild( nvmain );
  nvmain->SetParent( this );
  globalEventQueue->SetFrequency( config->GetEnergy( "CPUFreq" ) * 1000000.0 );
  globalEventQueue->AddSystem( nvmain, config );
  simInterface->SetConfig( config, true );
  nvmain->SetConfig( config, "defaultMemory", true );
  currentCycle = 0;
	outstandingRequests = 0;

	CommandQueueSize = 5;
}

uint64_t rvSim::getCycle()
{
	return currentCycle;
}
void rvSim::Cycle( ncycle_t steps )
{
	/* how to deal with draining?? */
	for ( uint64_t i=1 ; i <= steps ; i++ )
		if ( outstandingRequests > 0 ) 
		{
			if( !CommandQueue.empty() )
				if( IsIssuable(CommandQueue.front()))
				{
					//std::cout << "[+](Cycle) Send a commmmmand" << std::endl;
					GetChild( )->IssueCommand( CommandQueue.front() );
					CommandQueue.pop_front();
				}
			//std::cout << "[+](Cycle) Now it is: " << currentCycle << std::endl;
			globalEventQueue->Cycle( 1 );
			currentCycle++;
			
		}
		
}


bool rvSim::IsIssuable( uint64_t input_addr, uint64_t output_addr, char opt, uint64_t data, char slide)
{
	if ( opt == 'C')
	{
		//globalparams.Input_Addr.SetPhysicalAddress(input_addr);
		//globalparams.Output_Addr.SetPhysicalAddress(output_addr);
		if( slide == 'X')
			//globalparams.slide=X;
		{
			std::cout << "[+] output_addr" << output_addr << std::endl;
		}
		else if ( slide == 'Y' )
			//globalparams.slide=Y;
		{

		}
		else
		{
			std::cout << "[-](IsIssuable) Wrong slide_mode!" << std::endl;
			return false;
		}
		return IsIssuable(input_addr, opt, data, (uint64_t)0);
	}
	else
	{
		std::cout << "[-](IsIssuable) Wrong command!" <<std::endl;
		return false;
	}
}


bool rvSim::IsIssuable( uint64_t addr, char opt, uint64_t data, uint64_t threadId = 0 )
{
  //	NVMainRequest *request = new NVMainRequest;
  //	request = rvSim::linetocommand( addr, opt, data, threadId )
	NVMainRequest *request = new NVMainRequest();

	/* translate traceline to command */
	//uint64_t cycle = currentCycle;
	
	/* opt translator */

	if ( opt == 'R' )
		request->type = READ;
	else if ( opt == 'W')
		request->type = WRITE;
	else if ( opt == 'L')
		request->type = LOAD_WEIGHT;
	else if ( opt == 'C')
		request->type = COMPUTE;
	else 
		std::cout << "[-](IsIssuable) Warning: Unknown operation '" \
		          << opt << "'" << std::endl;
	
	//int byte;
	//int start, end;
	
	assert(sizeof(uint64_t)*8 == 64);
	
	NVMDataBlock dataBlock;
    NVMDataBlock oldDataBlock;
	dataBlock.SetSize( 64 );
	for (int byte = 0; byte < 64; byte ++)
	{
		dataBlock.rawData[byte] = data % 256 ;
		data = data / 256;
	}
	
	NVMAddress nAddress;
	nAddress.SetPhysicalAddress( addr );
	request->address = nAddress;
	request->bulkCmd = CMD_NOP;
	request->threadId = threadId;
	request->data = dataBlock;
	request->oldData = oldDataBlock;
	request->status = MEM_REQUEST_INCOMPLETE;
	request->owner = (NVMObject *)this;
	
	return IsIssuable( request );
}

bool rvSim::IsIssuable( NVMainRequest* request , FailReason * /*fail*/)
{
	return GetChild( request )->IsIssuable( request );
}

bool rvSim::IsIssuable( )
{
	if ( CommandQueue.size() < CommandQueueSize )
		return true;
	else
	{
		return false;
	}
}
bool rvSim::IssueCommand( uint64_t input_addr, uint64_t output_addr, char opt, uint64_t data, char slide)
{
	NVMainRequest *request = new NVMainRequest( );

	if ( opt == 'C')
	{
		NVMAddress nAddress1;
		nAddress1.SetPhysicalAddress( input_addr );
		request->C_address1 = nAddress1;
		NVMAddress nAddress2;
		nAddress2.SetPhysicalAddress( output_addr );
		request->C_address2 = nAddress2;
		//globalparams.Input_Addr.SetPhysicalAddress(input_addr);
		//globalparams.Output_Addr.SetPhysicalAddress(output_addr);
		request->BufferSize = globalparams.Buffer_n;
		request->type = COMPUTE;
		if( slide == 'X')
		{
			//globalparams.slide=X;
			request->slide = X;
		}
		else if ( slide == 'Y' )
		{
			//globalparams.slide=Y;
			request->slide = Y;
		}
		else
		{
			std::cout << "[+] wrong slide_mode " << std::endl;
			return false;
		}
		//return IssueCommand(input_addr, opt, data, (uint64_t)0);
	}
	else
	{
		std::cout << "[-](IssueCommand) wrong command" <<std::endl;
		return false;
	}
	
	assert(sizeof(uint64_t)*8 == 64);
	
	NVMDataBlock dataBlock;
    NVMDataBlock oldDataBlock;
	dataBlock.SetSize( 64 );
	for (int byte = 0; byte < 64; byte ++)
	{
		dataBlock.rawData[byte] = data % 256 ;
		data = data / 256;
	}

	NVMAddress nAddress;
	nAddress.SetPhysicalAddress( input_addr );
	request->address = nAddress;
	request->bulkCmd = CMD_NOP;
	request->threadId = 0;
	request->data = dataBlock;
	request->oldData = oldDataBlock;
	request->status = MEM_REQUEST_INCOMPLETE;
	request->owner = (NVMObject *)this;
	
  return IssueCommand(request);	
}
bool rvSim::IssueCommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId, char transfer_mode, uint64_t transfer_size )
{
	NVMainRequest *request = new NVMainRequest( );

	if( opt== 'T')
	{
		request->type = TRANSFER;
		request->t_size = transfer_size;
		if( transfer_mode == 'I')
			request->t_mode = Move_In;
		else if( transfer_mode == 'O')
			request->t_mode = Move_Out;
		else
		{
			std::cout << "[-](IssueCommand) Wrong slide_mode " << std::endl;
			return false;
		}
	}
	else 
	{
		std::cout << "[-](IssueCommand) Wrong command" <<std::endl;
		return false;
	}

	assert(sizeof(uint64_t)*8 == 64);
	
	NVMDataBlock dataBlock;
    NVMDataBlock oldDataBlock;
	dataBlock.SetSize( 64 );
	for (int byte = 0; byte < 64; byte ++)
	{
		dataBlock.rawData[byte] = data % 256 ;
		data = data / 256;
	}
	
	NVMAddress nAddress;
	nAddress.SetPhysicalAddress( addr );
	request->address = nAddress;
	request->bulkCmd = CMD_NOP;
	request->threadId = threadId;
	request->data = dataBlock;
	request->oldData = oldDataBlock;
	request->status = MEM_REQUEST_INCOMPLETE;
	request->owner = (NVMObject *)this;
	
    return IssueCommand( request );	
}

bool rvSim::IssueCommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId )
{
	NVMainRequest *request = new NVMainRequest( );
	/* translate traceline to command */
	//cycle = currentCycle;
	
	/* opt translator */
	if ( opt == 'R' )
		request->type = READ;
	else if ( opt == 'W')
		request->type = WRITE;
	else if ( opt == 'L')
		request->type = LOAD_WEIGHT;
	else if ( opt == 'C')
	{
		request->BufferSize = globalparams.Buffer_n;
		request->type = COMPUTE;
	}
	else if ( opt == 'T')
	{
		request->type = TRANSFER;
	}
	else 
		std::cout << "[-](IssueCommand) Warning: Unknown operation '" \
		         << opt << "'" << std::endl;
	
	//int byte;
	//int start, end;
	
	assert(sizeof(uint64_t)*8 == 64);
	
	NVMDataBlock dataBlock;
    NVMDataBlock oldDataBlock;
	dataBlock.SetSize( 64 );
	for (int byte = 0; byte < 64; byte ++)
	{
		dataBlock.rawData[byte] = data % 256 ;
		data = data / 256;
	}
	
	NVMAddress nAddress;
	nAddress.SetPhysicalAddress( addr );
	request->address = nAddress;
	request->bulkCmd = CMD_NOP;
	request->threadId = threadId;
	request->data = dataBlock;
	request->oldData = oldDataBlock;
	request->status = MEM_REQUEST_INCOMPLETE;
	request->owner = (NVMObject *)this;
	
  return IssueCommand( request );
}

bool rvSim::IssueCommand( NVMainRequest *request )
{	
	outstandingRequests++;
	//GetChild( )->IssueCommand( request );
	if (IsIssuable())
		CommandQueue.push_back(request);
	return 1;
}

bool rvSim::RequestComplete( NVMainRequest* request )
{
    assert( request->owner == this );
	
	std::cout << "[+] ******************************** [+]" << std::endl;
	if( request->type == READ )
		std::cout << "[+] Read from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
	else if( request->type == WRITE )
		std::cout << "[+] Write from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
    else if ( request->type == LOAD_WEIGHT )
		std::cout << "[+] Load from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
	else if ( request->type == COMPUTE )
		std::cout << "[+] Compute from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
	else if ( request->type == TRANSFER )
		std::cout << "[+] Transfer from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
	for (int i = 0; i<64; i++)
    {
        std::cout << "[+]" << (int) request->data.rawData[i] << " ";
    }
	std::cout << "[+]" << std::endl;
	outstandingRequests--;

    delete request;

    return true;
}	
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

int main()
{
	char start;

	std::cout << "Ready? Y/N" << std::endl;
	std::cin >> start;
	
	if( ( start == 'Y' ) || ( start == 'y' ))
	{
        int argc;
	    char *argv[4];
	
	    argc = 4 ;
	    argv[0] = (char*)"nvmain";
    	argv[1] = (char*)"Config/PCM_ISSCC_2012_4GB.config";
    	argv[2] = (char*)"Tests/Traces/test.nvt";
    	//argv[3] = (char*)"1000000";
    	argv[3] = (char*)"10000";
        rvSim *risc5sim = new rvSim( );
		
		assert(argc = 4);

		int result;
        std::cout << "//-----------------------------------------//" << std::endl;
        
        risc5sim->SetConfig( argc, argv );
		risc5sim->IssueCommand( 384, 'R', 1223, 0);
		while (true)
		{
			char conti;
			std::cout << " continue cycle ? Y/N " << " currentCycle is " << risc5sim->getCycle() << std::endl;
			std::cin >> conti;
			if ((conti == 'Y') || (conti == 'y'))
				risc5sim->Cycle(100);
		}
        std::cout << "All is done" << std::endl ;
        std::cout << "//-----------------------------------------//" << std::endl;
        return result;
    }
	else if( ( start == 'N' ) || ( start == 'n' ) )
	{	
		std::cout << "//-----------------------------------------//" << std::endl;
		std::cout << "Nothing happened" << std::endl ;
		std::cout << "//-----------------------------------------//" << std::endl;
		return 0;
	}
	else
	{
		std::cout << "wrong answer" << std::endl;
		std::cout << "Program is stopped" << std::endl;
	}
}

rvSim::rvSim( )
{
	
}

rvSim::~rvSim( )
{
	
}

void rvSim::SetConfig( int argc, char *argv[] )
{
	if (argc != 4)
	{
		std::cout << "Warnning! Warnning!";
		return ;
	}
	/* Init */
	stats = new Stats( );
	config = new Config( );
	//trace = NULL;
	tl = new TraceLine( );
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
        //std::cout << "Creating hook " << hookList[i] << std::endl;

        NVMObject *hook = HookFactory::CreateHook( hookList[i] );
        
        if( hook != NULL )
        {
            AddHook( hook );
            hook->SetParent( this );
            hook->Init( config );
        }
        else
        {
            //std::cout << "Warning: Could not create a hook named `" 
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
			globalEventQueue->Cycle( 1 );
			currentCycle++;
		}
		
}

bool rvSim::IsIssuable( uint64_t addr, char opt, uint64_t data, uint64_t threadId = 0 )
{
//	NVMainRequest *request = new NVMainRequest;
//	request = rvSim::linetocommand( addr, opt, data, threadId )
	NVMainRequest *request = new NVMainRequest( );
	/* translate traceline to command */
	//uint64_t cycle = currentCycle;
	
	/* opt translator */
	if ( opt == 'R' )
		request->type = READ;
	else if ( opt == 'W')
		request->type = WRITE;
	else 
		std::cout << "Warning: Unknown operation '" << opt << "'" << std::endl;
	
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
	else 
		std::cout << "Warning: Unknown operation '" << opt << "'" << std::endl;
	
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
	GetChild( )->IssueCommand( request );
	return 1;
}

bool rvSim::RequestComplete( NVMainRequest* request )
{
    assert( request->owner == this );
	
	std::cout << "********************************" << std::endl;
	if( request->type == READ )
		std::cout << "read from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
	else if( request->type == WRITE )
		std::cout << "write from " << request->arrivalCycle << " to " << request->completionCycle << std::endl;
    
	for (int i = 0; i<64; i++)
    {
        std::cout << (int) request->data.rawData[i] << " ";
    }
	std::cout << std::endl;
	outstandingRequests--;

    delete request;

    return true;
}
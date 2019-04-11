// By Mengyu Guo

#ifndef __TRACESIM_TRACEMAIN_H__
#define __TRACESIM_TRACEMAIN_H__


#include "src/NVMObject.h"


namespace NVM {


class rvSim : public NVMObject
{
  public:
    rvSim( );
    ~rvSim( );

    void SetConfig( int argc, char *argv[] );

    void Cycle( ncycle_t steps );

  	NVMainRequest *linetocommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
  	bool IsIssuable( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
    bool IsIssuable( NVMainRequest *request, FailReason * fail=NULL);
  	bool IssueCommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
  	bool IssueCommand( NVMainRequest * request );
    bool RequestComplete( NVMainRequest *request );
    
    uint64_t getCycle();
    
  protected:
  
  private:
    ncounter_t outstandingRequests;
    
	  Stats *stats ;
    Config *config ;
    GenericTraceReader *trace ;
    TraceLine *tl ;
    SimInterface *simInterface ;
    NVMain *nvmain ;
    EventQueue *mainEventQueue ;
    GlobalEventQueue *globalEventQueue ;
    TagGenerator *tagGenerator ;
	
	  uint64_t simulateCycles;
    uint64_t currentCycle;
};


};


#endif


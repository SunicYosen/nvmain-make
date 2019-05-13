// By Mengyu Guo

#ifndef RVSIM_H
#define RVSIM_H

#include "src/NVMObject.h"
#include "MemControl/DRAMCache/DRAMCache.h"

namespace NVM {
  class rvSim : public NVMObject
  {
    public:
      rvSim( );
      ~rvSim( );

    void SetConfig(int argc, char *argv[]);

    void Cycle( ncycle_t steps );

  	NVMainRequest *linetocommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
    bool IsIssuable( uint64_t input_addr, uint64_t output_addr, char opt, uint64_t data, char slide);
  	bool IsIssuable( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
    bool IsIssuable( NVMainRequest *request, FailReason * fail=NULL);
    bool IsIssuable( );
    bool IssueCommand( uint64_t input_addr, uint64_t output_addr, char opt, uint64_t data, char slide);
    bool IssueCommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId, char transfer_mode, uint64_t transfer_size);
  	bool IssueCommand( uint64_t addr, char opt, uint64_t data, uint64_t threadId );
  	bool IssueCommand( NVMainRequest * request );
    bool RequestComplete( NVMainRequest *request );
    
    bool setParameters();
    
    uint64_t getCycle();
    
    protected:
    
    private:
      ncounter_t outstandingRequests;
      
      Stats *stats ;
      Config *config ;
      //GenericTraceReader *trace ;
      //TraceLine *tl ;
      SimInterface *simInterface ;
      NVMain *nvmain ;
      EventQueue *mainEventQueue ;
      GlobalEventQueue *globalEventQueue ;
      TagGenerator *tagGenerator ;
      std::list<NVMainRequest *> CommandQueue;
      uint64_t CommandQueueSize;
    
      uint64_t simulateCycles;
      uint64_t currentCycle;
  };
};

#endif
/*******************************************************************************
* Copyright (c) 2012-2014, The Microsystems Design Labratory (MDL)
* Department of Computer Science and Engineering, The Pennsylvania State University
* All rights reserved.
* 
* This source code is part of NVMain - A cycle accurate timing, bit accurate
* energy simulator for both volatile (e.g., DRAM) and non-volatile memory
* (e.g., PCRAM). The source code is free and you can redistribute and/or
* modify it by providing that the following conditions are met:
* 
*  1) Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
* 
*  2) Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the documentation
*     and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
* Author list: 
*   Matt Poremba    ( Email: mrp5060 at psu dot edu 
*                     Website: http://www.cse.psu.edu/~poremba/ )
*   Tao Zhang       ( Email: tzz106 at cse dot psu dot edu
*                     Website: http://www.cse.psu.edu/~tzz106 )
*******************************************************************************/

#include "src/MemoryController.h"
#include "include/NVMainRequest.h"
#include "src/EventQueue.h"
#include "src/Interconnect.h"
#include "Interconnect/InterconnectFactory.h"
#include "src/Rank.h"
#include "src/SubArray.h"
#include "include/NVMHelpers.h"

#include <sstream>
#include <cassert>
#include <cstdlib>
#include <csignal>
#include <limits>
#include <algorithm>

using namespace NVM;

extern GlobalParams globalparams;

/* Command queue removal predicate. */
bool WasIssued( NVMainRequest *request );
bool WasIssued( NVMainRequest *request ) { return (request->flags & NVMainRequest::FLAG_ISSUED); }

MemoryController::MemoryController( )
{
    transactionQueues = NULL;
    transactionQueueCount = 0;
    commandQueues = NULL;
    commandQueueCount = 0;

    lastCommandWake = 0;
    wakeupCount = 0;
    lastIssueCycle = 0;

    starvationThreshold = 4;
    subArrayNum = 1;
    starvationCounter = NULL;
    activateQueued = NULL;
    effectiveRow = NULL;
    effectiveMuxedRow = NULL;
    activeSubArray = NULL;

    delayedRefreshCounter = NULL;
    
    curQueue = 0;
    nextRefreshRank = 0;
    nextRefreshBank = 0;

    handledRefresh = std::numeric_limits<ncycle_t>::max( );
}

MemoryController::~MemoryController( )
{
    for( ncounter_t i = 0; i < p->RANKS; i++ )
    {
        delete [] activateQueued[i];
        delete [] bankNeedRefresh[i];
        delete [] refreshQueued[i];

        for( ncounter_t j = 0; j < p->BANKS; j++ )
        {
            delete [] starvationCounter[i][j];
            delete [] effectiveRow[i][j];
            delete [] effectiveMuxedRow[i][j];
            delete [] activeSubArray[i][j];
        }
        delete [] starvationCounter[i];
        delete [] effectiveRow[i];
        delete [] effectiveMuxedRow[i];
        delete [] activeSubArray[i];
    }

    delete [] commandQueues;
    delete [] starvationCounter;
    delete [] activateQueued;
    delete [] effectiveRow;
    delete [] effectiveMuxedRow;
    delete [] activeSubArray;
    delete [] bankNeedRefresh;
    delete [] rankPowerDown;
    
    if( p->UseRefresh )
    {
        for( ncounter_t i = 0; i < p->RANKS; i++ )
        {
            /* Note: delete a NULL point is permitted in C++ */
            delete [] delayedRefreshCounter[i];
        }
    }

    delete [] delayedRefreshCounter;
}

void MemoryController::InitQueues( unsigned int numQueues )
{
    if( transactionQueues != NULL )
        delete [] transactionQueues;

    transactionQueues = new NVMTransactionQueue[ numQueues ];
    transactionQueueCount = numQueues;

    for( unsigned int i = 0; i < numQueues; i++ )
        transactionQueues[i].clear( );
}

void MemoryController::Cycle( ncycle_t /*steps*/ )
{
    /* 
     *  Recheck transaction queues for issuables. This may happen when two
     *  transactions can be issued in the same cycle, but we can't guarantee
     *  the transaction won't be blocked by the first wake up.
     */
    bool scheduled = false;
    ncycle_t nextWakeup = GetEventQueue( )->GetCurrentCycle( ) + 1;

    /* Skip this if another transaction is scheduled this cycle. */
    if( GetEventQueue( )->FindEvent( EventCycle, this, NULL, nextWakeup ) )
        return;

    for( ncounter_t queueIdx = 0; queueIdx < commandQueueCount; queueIdx++ )
    {
        if( EffectivelyEmpty( queueIdx )
            && TransactionAvailable( queueIdx ) )
        {
            GetEventQueue( )->InsertEvent( EventCycle, this, nextWakeup, NULL, transactionQueuePriority );

            /* Only allow one request. */
            scheduled = true;
            break;
        }

        if( scheduled ) 
        {
            break;
        }
    }
}

void MemoryController::Prequeue( ncounter_t queueNum, NVMainRequest *request )
{
    assert( queueNum < transactionQueueCount );

    transactionQueues[queueNum].push_front( request );
}

void MemoryController::Enqueue( ncounter_t queueNum, NVMainRequest *request )
{
    /* Retranslate once for this channel, but leave channel the same */
    ncounter_t channel, rank, bank, row, col, subarray;

    GetDecoder( )->Translate( request->address.GetPhysicalAddress( ), 
                               &row, &col, &bank, &rank, &channel, &subarray );
    channel = request->address.GetChannel( );
    request->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );

    /* Enqueue the request. */
    assert( queueNum < transactionQueueCount );

    transactionQueues[queueNum].push_back( request );
    
    /* If this command queue is empty, we can schedule a new transaction right away. */
    ncounter_t queueId = GetCommandQueueId( request->address );

    if( EffectivelyEmpty( queueId ) )
    {
        ncycle_t nextWakeup = GetEventQueue( )->GetCurrentCycle( );

        if( GetEventQueue( )->FindEvent( EventCycle, this, NULL, nextWakeup ) == NULL )
        {
            GetEventQueue( )->InsertEvent( EventCycle, this, nextWakeup, NULL, transactionQueuePriority );
        }
    }
}

bool MemoryController::TransactionAvailable( ncounter_t queueId )
{
    bool rv = false; 

    for( ncounter_t queueIdx = 0; queueIdx < transactionQueueCount; queueIdx++ )
    {
        std::list<NVMainRequest *>::iterator it;

        for( it = transactionQueues[queueIdx].begin( );
             it != transactionQueues[queueIdx].end( ); it++ )
        {
            if( GetCommandQueueId( (*it)->address ) == queueId )
            {
                rv = true;
                break;
            }
        }
    }

    return rv;
}

void MemoryController::ScheduleCommandWake( )
{
    /* Schedule wake event for memory commands if not scheduled. */
    ncycle_t nextWakeup = NextIssuable( NULL );

    /* Avoid scheduling multiple duplicate events. */
    bool nextWakeupScheduled = GetEventQueue()->FindCallback( this, 
                                (CallbackPtr)&MemoryController::CommandQueueCallback,
                                nextWakeup, NULL, commandQueuePriority );

    if( !nextWakeupScheduled )
    {
        GetEventQueue( )->InsertCallback( this, 
                          (CallbackPtr)&MemoryController::CommandQueueCallback,
                          nextWakeup, NULL, commandQueuePriority );
    }
}

void MemoryController::CommandQueueCallback( void * /*data*/ )
{
    /* Determine time since last wakeup. */
    ncycle_t realSteps = GetEventQueue( )->GetCurrentCycle( ) - lastCommandWake;
    lastCommandWake = GetEventQueue( )->GetCurrentCycle( );

    /* Schedule next wake up. */
    ncycle_t nextWakeup = NextIssuable( NULL );
    wakeupCount++;

    /* Avoid scheduling multiple duplicate events. */
    bool nextWakeupScheduled = GetEventQueue()->FindCallback( this, 
                                (CallbackPtr)&MemoryController::CommandQueueCallback,
                                nextWakeup, NULL, commandQueuePriority );

    if( !nextWakeupScheduled 
        && nextWakeup != std::numeric_limits<ncycle_t>::max( ) )
    {
        GetEventQueue( )->InsertCallback( this, 
                          (CallbackPtr)&MemoryController::CommandQueueCallback,
                          nextWakeup, NULL, commandQueuePriority );
    }

    CycleCommandQueues( );

    GetChild( )->Cycle( realSteps );
}

void MemoryController::RefreshCallback( void *data )
{
    NVMainRequest *request = reinterpret_cast<NVMainRequest *>(data);

    ncycle_t realSteps = GetEventQueue( )->GetCurrentCycle( ) - lastCommandWake;
    lastCommandWake = GetEventQueue( )->GetCurrentCycle( );
    wakeupCount++;

    ProcessRefreshPulse( request );
    HandleRefresh( );

    /* Catch up the rest of the system. */
    GetChild( )->Cycle( realSteps );
}

void MemoryController::CleanupCallback( void * /*data*/ )
{
    for( ncycle_t queueId = 0; queueId < commandQueueCount; queueId++ )
    {
        /* Remove issued requests from the command queue. */
        //std::cout << "[+] i do the cleanup " << std::endl;
        commandQueues[queueId].erase(
            std::remove_if( commandQueues[queueId].begin(), 
                            commandQueues[queueId].end(), 
                            WasIssued ),
            commandQueues[queueId].end()
        );        
    }
}

bool MemoryController::RequestComplete( NVMainRequest *request )
{
    //if( request->type == REFRESH )
    //    ProcessRefreshPulse( request );
    //else if( request->owner == this )
    if( request->owner == this )
    {
        /* 
         *  Any activate/precharge/etc commands belong to the memory controller
         *  and we are in charge of deleting them!
         */
        //if ( request->type == READCYCLE || request->type == REALCOMPUTE || request->type == POSTREAD || request->type == WRITECYCLE )
            //std::cout << "[+](setParameters) I'm fine thank you " << std::endl;
        delete request;
    }
    else
    {
        return GetParent( )->RequestComplete( request );
    }

    return true;
}

bool MemoryController::IsIssuable( NVMainRequest * /*request*/, FailReason * /*fail*/ )
{
    return true;
}

void MemoryController::SetMappingScheme( )
{
    /* Configure common memory controller parameters. */
    GetDecoder( )->GetTranslationMethod( )->SetAddressMappingScheme( p->AddressMappingScheme );
}

void MemoryController::SetConfig( Config *conf, bool createChildren )
{
    this->config = conf;

    Params *params = new Params( );
    params->SetParams( conf );
    SetParams( params );
    
    if( createChildren )
    {
        uint64_t channels, ranks, banks, rows, cols, subarrays;

        /* When selecting a child, use the bank field from the decoder. */
        AddressTranslator *mcAT = DecoderFactory::CreateDecoderNoWarn( conf->GetString( "Decoder" ) );
        mcAT->SetDefaultField( NO_FIELD );
        mcAT->SetConfig( conf, createChildren );
        SetDecoder( mcAT );

        /* Get the parent's method information. */
        GetParent()->GetTrampoline()->GetDecoder()->GetTranslationMethod()->GetCount(
                    &rows, &cols, &banks, &ranks, &channels, &subarrays );

        /* Allows for differing layouts per channel by overwriting the method. */
        if( conf->KeyExists( "MATHeight" ) )
        {
            rows = p->MATHeight;
            subarrays = p->ROWS / p->MATHeight;
        }
        else
        {
            rows = p->ROWS;
            subarrays = 1;
        }
        cols = p->COLS;
        banks = p->BANKS;
        ranks = p->RANKS;

        TranslationMethod *method = new TranslationMethod( );

        method->SetBitWidths( NVM::mlog2( rows ), 
                    NVM::mlog2( cols ), 
                    NVM::mlog2( banks ), 
                    NVM::mlog2( ranks ), 
                    NVM::mlog2( channels ), 
                    NVM::mlog2( subarrays )
                    );
        method->SetCount( rows, cols, banks, ranks, channels, subarrays );
        mcAT->SetTranslationMethod( method );

        /* Initialize interconnect */
        std::stringstream confString;

        memory = InterconnectFactory::CreateInterconnect( conf->GetString( "INTERCONNECT" ) );

        confString.str( "" );
        confString << StatName( ) << ".channel" << GetID( );
        memory->StatName( confString.str( ) );

        memory->SetParent( this );
        AddChild( memory );

        memory->SetConfig( conf, createChildren );
        memory->RegisterStats( );
        
        SetMappingScheme( );
    }

    /*
     *  The logical bank size is: ROWS * COLS * memory word size (in bytes). 
     *  memory word size (in bytes) is: device width * minimum burst length * data rate / (8 bits/byte) * number of devices
     *  number of devices = bus width / device width
     *  Total channel size is: loglcal bank size * BANKS * RANKS
     */
    //std::cout << "[+]" << StatName( ) << " capacity is " << ((p->ROWS * p->COLS * p->tBURST * p->RATE * p->BusWidth * p->BANKS * p->RANKS) / (8*1024*1024)) << " MB." << std::endl;

    if( conf->KeyExists( "MATHeight" ) )
    {
        subArrayNum = p->ROWS / p->MATHeight;
    }
    else
    {
        subArrayNum = 1;
    }

    /* Determine number of command queues. Assume per-bank queues as this was the default for older nvmain versions. */
    queueModel = PerBankQueues;
    commandQueueCount = p->RANKS * p->BANKS;
    if( conf->KeyExists( "QueueModel" ) )
    {
        if( conf->GetString( "QueueModel" ) == "PerRank" )
        {
            queueModel = PerRankQueues;
            commandQueueCount = p->RANKS;
        }
        else if( conf->GetString( "QueueModel" ) == "PerBank" )
        {
            queueModel = PerBankQueues;
            commandQueueCount = p->RANKS * p->BANKS;
        }
        else if( conf->GetString( "QueueModel" ) == "PerSubArray" )
        {
            queueModel = PerSubArrayQueues;
            commandQueueCount = p->RANKS * p->BANKS * subArrayNum;
        }
        /* Add your custom types here. */
    }

    //std::cout << "[+] Creating " << commandQueueCount << " command queues." << std::endl;
    
    commandQueues = new std::deque<NVMainRequest *> [commandQueueCount];
    activateQueued = new bool * [p->RANKS];
    refreshQueued = new bool * [p->RANKS];
    starvationCounter = new ncounter_t ** [p->RANKS];
    effectiveRow = new ncounter_t ** [p->RANKS];
    effectiveMuxedRow = new ncounter_t ** [p->RANKS];
    activeSubArray = new ncounter_t ** [p->RANKS];
    rankPowerDown = new bool [p->RANKS];

    for( ncounter_t i = 0; i < p->RANKS; i++ )
    {
        activateQueued[i] = new bool[p->BANKS];
        refreshQueued[i] = new bool[p->BANKS];
        activeSubArray[i] = new ncounter_t * [p->BANKS];
        effectiveRow[i] = new ncounter_t * [p->BANKS];
        effectiveMuxedRow[i] = new ncounter_t * [p->BANKS];
        starvationCounter[i] = new ncounter_t * [p->BANKS];

        if( p->UseLowPower )
            rankPowerDown[i] = p->InitPD;
        else
            rankPowerDown[i] = false;

        for( ncounter_t j = 0; j < p->BANKS; j++ )
        {
            activateQueued[i][j] = false;
            refreshQueued[i][j] = false;

            starvationCounter[i][j] = new ncounter_t [subArrayNum];
            effectiveRow[i][j] = new ncounter_t [subArrayNum];
            effectiveMuxedRow[i][j] = new ncounter_t [subArrayNum];
            activeSubArray[i][j] = new ncounter_t [subArrayNum];

            for( ncounter_t m = 0; m < subArrayNum; m++ )
            {
                starvationCounter[i][j][m] = 0;
                activeSubArray[i][j][m] = false;
                /* set the initial effective row as invalid */
                effectiveRow[i][j][m] = p->ROWS;
                effectiveMuxedRow[i][j][m] = p->ROWS;
            }
        }
    }

    bankNeedRefresh = new bool * [p->RANKS];
    for( ncounter_t i = 0; i < p->RANKS; i++ )
    {
        bankNeedRefresh[i] = new bool [p->BANKS];
        for( ncounter_t j = 0; j < p->BANKS; j++ )
        {
            bankNeedRefresh[i][j] = false;
        }
    }
        
    delayedRefreshCounter = new ncounter_t * [p->RANKS];

    if( p->UseRefresh )
    {
        /* sanity check */
        assert( p->BanksPerRefresh <= p->BANKS );

        /* 
         * it does not make sense when refresh is needed 
         * but no bank can be refreshed 
         */
        assert( p->BanksPerRefresh != 0 );

        m_refreshBankNum = p->BANKS / p->BanksPerRefresh;
        
        /* first, calculate the tREFI */
        m_tREFI = p->tREFW / (p->ROWS / p->RefreshRows );

        /* then, calculate the time interval between two refreshes */
        ncycle_t m_refreshSlice = m_tREFI / ( p->RANKS * m_refreshBankNum );

        for( ncounter_t i = 0; i < p->RANKS; i++ )
        {
            delayedRefreshCounter[i] = new ncounter_t [m_refreshBankNum];
            
            /* initialize the counter to 0 */
            for( ncounter_t j = 0; j < m_refreshBankNum; j++ )
            {
                delayedRefreshCounter[i][j] = 0;

                ncounter_t refreshBankHead = j * p->BanksPerRefresh;

                /* create first refresh pulse to start the refresh countdown */ 
                NVMainRequest* refreshPulse = MakeRefreshRequest( 
                                                0, 0, refreshBankHead, i, 0 );

                /* stagger the refresh */
                ncycle_t offset = (i * m_refreshBankNum + j ) * m_refreshSlice; 

                /* 
                 * insert refresh pulse, the event queue behaves like a 
                 * refresh countdown timer 
                 */
                GetEventQueue()->InsertCallback( this, 
                               (CallbackPtr)&MemoryController::RefreshCallback, 
                               GetEventQueue()->GetCurrentCycle()+m_tREFI+offset, 
                               reinterpret_cast<void*>(refreshPulse), 
                               refreshPriority );
            }
        }
    }

    if( p->PrintConfig )
        config->Print();

    SetDebugName( "MemoryController", conf );
}

void MemoryController::RegisterStats( )
{
    AddStat(simulation_cycles);
    AddStat(wakeupCount);
}

/* 
 * NeedRefresh() has three functions:
 *  1) it returns false when no refresh is used (p->UseRefresh = false) 
 *  2) it returns false if the delayed refresh counter does not
 *  reach the threshold, which provides the flexibility for
 *  fine-granularity refresh 
 *  3) it automatically find the bank group the argument "bank"
 *  specifies and return the result
 */
bool MemoryController::NeedRefresh( const ncounter_t bank, const uint64_t rank )
{
    bool rv = false;

    if( p->UseRefresh )
        if( delayedRefreshCounter[rank][bank/p->BanksPerRefresh] 
                >= p->DelayedRefreshThreshold )
            rv = true;
        
    return rv;
}

/* 
 * Set the refresh flag for a given bank group
 */
void MemoryController::SetRefresh( const ncounter_t bank, const uint64_t rank )
{
    /* align to the head of bank group */
    ncounter_t bankHead = ( bank / p->BanksPerRefresh ) * p->BanksPerRefresh;

    for( ncounter_t i = 0; i < p->BanksPerRefresh; i++ )
        bankNeedRefresh[rank][bankHead + i] = true;
}

/* 
 * Reset the refresh flag for a given bank group
 */
void MemoryController::ResetRefresh( const ncounter_t bank, const uint64_t rank )
{
    /* align to the head of bank group */
    ncounter_t bankHead = ( bank / p->BanksPerRefresh ) * p->BanksPerRefresh;

    for( ncounter_t i = 0; i < p->BanksPerRefresh; i++ )
        bankNeedRefresh[rank][bankHead + i] = false;
}

/*
 * Reset the refresh queued flag for a given bank group
 */
void MemoryController::ResetRefreshQueued( const ncounter_t bank, const ncounter_t rank )
{
    /* align to the head of bank group */
    ncounter_t bankHead = ( bank / p->BanksPerRefresh ) * p->BanksPerRefresh;

    for( ncounter_t i = 0; i < p->BanksPerRefresh; i++ )
    {
        assert( refreshQueued[rank][bankHead + i] );
        refreshQueued[rank][bankHead + i] = false;
    }
}

/* 
 * Increment the delayedRefreshCounter by 1 in a given bank group
 */
void MemoryController::IncrementRefreshCounter( const ncounter_t bank, const uint64_t rank )
{
    /* get the bank group ID */
    ncounter_t bankGroupID = bank / p->BanksPerRefresh;

    delayedRefreshCounter[rank][bankGroupID]++;
}

/* 
 * decrement the delayedRefreshCounter by 1 in a given bank group
 */
void MemoryController::DecrementRefreshCounter( const ncounter_t bank, const uint64_t rank )
{
    /* get the bank group ID */
    ncounter_t bankGroupID = bank / p->BanksPerRefresh;

    delayedRefreshCounter[rank][bankGroupID]--;
}

/* 
 * it simply checks all the banks in the refresh bank group whether their
 * command queues are empty. the result is the union of each check
 */
bool MemoryController::HandleRefresh( )
{
    for( ncounter_t rankIdx = 0; rankIdx < p->RANKS; rankIdx++ )
    {
        ncounter_t i = (nextRefreshRank + rankIdx) % p->RANKS;

        for( ncounter_t bankIdx = 0; bankIdx < m_refreshBankNum; bankIdx++ )
        {
            ncounter_t j = (nextRefreshBank + bankIdx * p->BanksPerRefresh) % p->BANKS;
            FailReason fail;

            if( NeedRefresh( j, i ) /*&& IsRefreshBankQueueEmpty( j , i )*/ )
            {
                /* create a refresh command that will be sent to ranks */
                NVMainRequest* cmdRefresh = MakeRefreshRequest( 0, 0, j, i, 0 );

                /* Always check if precharge is needed, even if REF is issublable. */
                if( p->UsePrecharge )
                {
                    for( ncounter_t tmpBank = 0; tmpBank < p->BanksPerRefresh; tmpBank++ ) 
                    {
                        /* Use modulo to allow for an odd number of banks per refresh. */
                        ncounter_t refBank = (tmpBank + j) % p->BANKS;
                        ncounter_t queueId = GetCommandQueueId( NVMAddress( 0, 0, refBank, i /*rank*/, 0, 0 ) );

                        /* Precharge all active banks and active subarrays */
                        // TODO: Will this empty() need to be effectively empty?
                        if( activateQueued[i][refBank] == true /*&& commandQueues[queueId].empty()*/ )
                        {
                            /* issue a PRECHARGE_ALL command to close all subarrays */
                            // TODO: The PRECHARGE_ALL request generated here is meant to precharge all
                            // subarrays -- We will need a different command for precharging all banks
                            NVMainRequest *cmdRefPre = MakePrechargeAllRequest( 0, 0, refBank, i, 0 );

                            commandQueues[queueId].push_back( cmdRefPre );

                            /* clear all active subarrays */
                            for( ncounter_t sa = 0; sa < subArrayNum; sa++ )
                            {
                                activeSubArray[i][refBank][sa] = false; 
                                effectiveRow[i][refBank][sa] = p->ROWS;
                                effectiveMuxedRow[i][refBank][sa] = p->ROWS;
                            }
                            activateQueued[i][refBank] = false;
                        }
                    }
                }

                ncounter_t queueId = GetCommandQueueId( NVMAddress( 0, 0, j, i, 0, 0 ) );

                /* send the refresh command to the rank */
                cmdRefresh->issueCycle = GetEventQueue()->GetCurrentCycle();
                commandQueues[queueId].push_back( cmdRefresh );

                for( ncounter_t tmpBank = 0; tmpBank < p->BanksPerRefresh; tmpBank++ )
                {
                    ncounter_t refBank = (tmpBank + j) % p->BANKS;

                    /* Disallow queuing commands to non-bank-head queues. */
                    refreshQueued[i][refBank] = true;
                }

                /* decrement the corresponding counter by 1 */
                DecrementRefreshCounter( j, i );

                /* if do not need refresh anymore, reset the refresh flag */
                if( !NeedRefresh( j, i ) )
                    ResetRefresh( j, i );

                /* round-robin */
                nextRefreshBank += p->BanksPerRefresh;
                if( nextRefreshBank >= p->BANKS )
                {
                    nextRefreshBank = 0;
                    nextRefreshRank++;

                    if( nextRefreshRank == p->RANKS )
                        nextRefreshRank = 0;
                }

                handledRefresh = GetEventQueue()->GetCurrentCycle();

                ScheduleCommandWake( );

                /* we should return since one time only one command can be issued */
                return true;  
            }
        }
    }
    return false;
}

/* 
 * it simply increments the corresponding delayed refresh counter 
 * and re-insert the refresh pulse into event queue
 */
void MemoryController::ProcessRefreshPulse( NVMainRequest* refresh )
{
    assert( refresh->type == REFRESH );

    ncounter_t rank, bank;
    refresh->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

    IncrementRefreshCounter( bank, rank );

    if( NeedRefresh( bank, rank ) )
        SetRefresh( bank, rank ); 

    GetEventQueue()->InsertCallback( this, 
                   (CallbackPtr)&MemoryController::RefreshCallback, 
                   GetEventQueue()->GetCurrentCycle()+m_tREFI, 
                   reinterpret_cast<void*>(refresh), 
                   refreshPriority );
}

/* 
 * it simply checks all banks in the refresh bank group whether their
 * command queues are empty. the result is the union of each check
 */
bool MemoryController::IsRefreshBankQueueEmpty( const ncounter_t bank, const uint64_t rank )
{
    /* align to the head of bank group */
    ncounter_t bankHead = ( bank / p->BanksPerRefresh ) * p->BanksPerRefresh;

    for( ncounter_t i = 0; i < p->BanksPerRefresh; i++ )
    {
        ncounter_t queueId = GetCommandQueueId( NVMAddress( 0, 0, bankHead + i, rank, 0, 0 ) );
        if( !EffectivelyEmpty( queueId ) )
        {
            return false;
        }
    }

    return true;
}

void MemoryController::PowerDown( const ncounter_t& rankId )
{
    OpType pdOp = POWERDOWN_PDPF;
    if( p->PowerDownMode == "SLOWEXIT" )
        pdOp = POWERDOWN_PDPS;
    else if( p->PowerDownMode == "FASTEXIT" )
        pdOp = POWERDOWN_PDPF;
    else
        std::cerr << "[-] NVMain Error: Undefined low power mode" << std::endl;

    NVMainRequest *powerdownRequest = MakePowerdownRequest( pdOp, rankId );

    /* if some banks are active, active powerdown is applied */
    NVMObject *child;
    FindChildType( powerdownRequest, Rank, child );
    Rank *pdRank = dynamic_cast<Rank *>(child);

    if( pdRank->Idle( ) == false )
    {
        /* Remake request as PDA. */
        delete powerdownRequest;

        pdOp = POWERDOWN_PDA;
        powerdownRequest = MakePowerdownRequest( pdOp, rankId );
    }

    if( RankQueueEmpty( rankId ) && GetChild()->IsIssuable( powerdownRequest ) )
    {
        GetChild()->IssueCommand( powerdownRequest );
        rankPowerDown[rankId] = true;
    }
    else
    {
        delete powerdownRequest;
    }
}

void MemoryController::PowerUp( const ncounter_t& rankId )
{
    NVMainRequest *powerupRequest = MakePowerupRequest( rankId );

    /* If some banks are active, active powerdown is applied */
    if( RankQueueEmpty( rankId ) == false 
        && GetChild()->IsIssuable( powerupRequest ) )
    {
        GetChild()->IssueCommand( powerupRequest );
        rankPowerDown[rankId] = false;
    }
    else
    {
        delete powerupRequest;
    }
}

void MemoryController::HandleLowPower( )
{
    for( ncounter_t rankId = 0; rankId < p->RANKS; rankId++ )
    {
        bool needRefresh = false;
        if( p->UseRefresh )
        {
            for( ncounter_t bankId = 0; bankId < m_refreshBankNum; bankId++ )
            {
                ncounter_t bankGroupHead = bankId * p->BanksPerRefresh;

                if( NeedRefresh( bankGroupHead, rankId ) )
                {
                    needRefresh = true;
                    break;
                }
            }
        }

        /* if some of the banks in this rank need refresh */
        if( needRefresh )
        {
            NVMainRequest *powerupRequest = MakePowerupRequest( rankId );

            /* if the rank is powered down, power it up */
            if( rankPowerDown[rankId] && GetChild()->IsIssuable( powerupRequest ) )
            {
                GetChild()->IssueCommand( powerupRequest );
                rankPowerDown[rankId] = false;
            }
            else
            {
                delete powerupRequest;
            }
        }
        /* else, check whether the rank can be powered down or up */
        else
        {
            if( rankPowerDown[rankId] )
                PowerUp( rankId );
            else
                PowerDown( rankId );
        }
    }
}

Config *MemoryController::GetConfig( )
{
    return (this->config);
}

void MemoryController::SetID( unsigned int id )
{
    this->id = id;
}

unsigned int MemoryController::GetID( )
{
    return this->id;
}
NVMainRequest *MemoryController::MakeComputeRequest( NVMainRequest *triggerRequest )
{
    assert( triggerRequest->type == COMPUTE );

    NVMainRequest *tmp = new NVMainRequest( );

    tmp->type = COMPUTE;
    tmp->issueCycle = GetEventQueue()->GetCurrentCycle();
    tmp->address = triggerRequest->address;
    tmp->owner = this;
    tmp->isBuffer = triggerRequest->isBuffer;
    tmp->Buffer_n = triggerRequest->Buffer_n;
    tmp->rowIntr = triggerRequest->rowIntr;
    tmp->slide = triggerRequest->slide;
    tmp->BufferSize = triggerRequest->BufferSize;
    tmp->C_address1 = triggerRequest->C_address1;
    tmp->C_address2 = triggerRequest->C_address2;

    return tmp;
}
NVMainRequest *MemoryController::MakeReadCycleRequest( NVMainRequest *triggerRequest )
{
    assert( triggerRequest->type == COMPUTE );

    NVMainRequest *rcRequest = new NVMainRequest( );

    rcRequest->type = READCYCLE;
    rcRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    rcRequest->address = triggerRequest->address;
    rcRequest->owner = this;
    rcRequest->isBuffer = triggerRequest->isBuffer;
    rcRequest->Buffer_n = triggerRequest->Buffer_n;
    rcRequest->rowIntr = triggerRequest->rowIntr;
    rcRequest->slide = triggerRequest->slide;
    rcRequest->BufferSize = triggerRequest->BufferSize;
    rcRequest->C_address1 = triggerRequest->C_address1;
    rcRequest->C_address2 = triggerRequest->C_address2;
    rcRequest->isReused = triggerRequest->isReused;

    return rcRequest;
}

NVMainRequest *MemoryController::MakeRealComputeRequest( NVMainRequest *triggerRequest )
{
    assert( triggerRequest->type == COMPUTE );

    NVMainRequest *cRequest = new NVMainRequest( );

    cRequest->type = REALCOMPUTE;
    cRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    cRequest->address = triggerRequest->address;
    cRequest->owner = this;
    cRequest->isBuffer = triggerRequest->isBuffer;
    cRequest->Buffer_n = triggerRequest->Buffer_n;
    cRequest->rowIntr = triggerRequest->rowIntr;
    cRequest->slide = triggerRequest->slide;
    cRequest->BufferSize = triggerRequest->BufferSize;
    cRequest->C_address1 = triggerRequest->C_address1;
    cRequest->C_address2 = triggerRequest->C_address2;

    return cRequest;    
}

NVMainRequest *MemoryController::MakePostReadRequest( NVMainRequest *triggerRequest )
{
    assert( triggerRequest->type == COMPUTE );

    NVMainRequest *prRequest = new NVMainRequest( );

    prRequest->type = POSTREAD;
    prRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    prRequest->address = triggerRequest->address;
    prRequest->owner = this;
    prRequest->isBuffer = triggerRequest->isBuffer;
    prRequest->Buffer_n = triggerRequest->Buffer_n;
    prRequest->rowIntr = triggerRequest->rowIntr;
    prRequest->slide = triggerRequest->slide;
    prRequest->BufferSize = triggerRequest->BufferSize;
    prRequest->C_address1 = triggerRequest->C_address1;
    prRequest->C_address2 = triggerRequest->C_address2;

    return prRequest;    
}

NVMainRequest *MemoryController::MakeWriteCycleRequest( NVMainRequest *triggerRequest )
{
    assert( triggerRequest->type == COMPUTE );

    NVMainRequest *wcRequest = new NVMainRequest( );

    wcRequest->type = WRITECYCLE;
    wcRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    wcRequest->address = triggerRequest->address;
    //wcRequest->address = globalparams.Output_Addr;
    wcRequest->owner = this;
    wcRequest->isBuffer = triggerRequest->isBuffer;
    wcRequest->Buffer_n = triggerRequest->Buffer_n;
    wcRequest->rowIntr = triggerRequest->rowIntr;
    wcRequest->slide = triggerRequest->slide;
    wcRequest->BufferSize = triggerRequest->BufferSize;
    wcRequest->C_address1 = triggerRequest->C_address1;
    wcRequest->C_address2 = triggerRequest->C_address2;

    return wcRequest;    
}

NVMainRequest *MemoryController::MakeCachedRequest( NVMainRequest *triggerRequest )
{
    /* This method should be called on *transaction* queue requests, thus only READ/WRITE possible. */
    assert( triggerRequest->type == READ || triggerRequest->type == WRITE || triggerRequest->type == LOAD_WEIGHT || triggerRequest->type == COMPUTE || triggerRequest->type == TRANSFER);

    NVMainRequest *cachedRequest = new NVMainRequest( );

    *cachedRequest = *triggerRequest;
    cachedRequest->type = (triggerRequest->type == READ ? CACHED_READ : CACHED_WRITE);
    cachedRequest->owner = this;

    return cachedRequest;
}

NVMainRequest *MemoryController::MakeActivateRequest( NVMainRequest *triggerRequest )
{
    NVMainRequest *activateRequest = new NVMainRequest( );

    activateRequest->type = ACTIVATE;
    activateRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    activateRequest->address = triggerRequest->address;
    activateRequest->owner = this;

    return activateRequest;
}

NVMainRequest *MemoryController::MakeActivateRequest( const ncounter_t row,
                                                      const ncounter_t col,
                                                      const ncounter_t bank,
                                                      const ncounter_t rank,
                                                      const ncounter_t subarray )
{
    NVMainRequest *activateRequest = new NVMainRequest( );

    activateRequest->type = ACTIVATE;
    ncounter_t actAddr = GetDecoder( )->ReverseTranslate( row, col, bank, rank, id, subarray );
    activateRequest->address.SetPhysicalAddress( actAddr );
    activateRequest->address.SetTranslatedAddress( row, col, bank, rank, id, subarray );
    activateRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    activateRequest->owner = this;

    return activateRequest;
}

NVMainRequest *MemoryController::MakePrechargeRequest( NVMainRequest *triggerRequest )
{
    NVMainRequest *prechargeRequest = new NVMainRequest( );

    prechargeRequest->type = PRECHARGE;
    prechargeRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    prechargeRequest->address = triggerRequest->address;
    prechargeRequest->owner = this;

    return prechargeRequest;
}

NVMainRequest *MemoryController::MakePrechargeRequest( const ncounter_t row,
                                                       const ncounter_t col,
                                                       const ncounter_t bank,
                                                       const ncounter_t rank,
                                                       const ncounter_t subarray )
{
    NVMainRequest *prechargeRequest = new NVMainRequest( );

    prechargeRequest->type = PRECHARGE;
    ncounter_t preAddr = GetDecoder( )->ReverseTranslate( row, col, bank, rank, id, subarray );
    prechargeRequest->address.SetPhysicalAddress( preAddr );
    prechargeRequest->address.SetTranslatedAddress( row, col, bank, rank, id, subarray );
    prechargeRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    prechargeRequest->owner = this;

    return prechargeRequest;
}

NVMainRequest *MemoryController::MakePrechargeAllRequest( NVMainRequest *triggerRequest )
{
    NVMainRequest *prechargeAllRequest = new NVMainRequest( );

    prechargeAllRequest->type = PRECHARGE_ALL;
    prechargeAllRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    prechargeAllRequest->address = triggerRequest->address;
    prechargeAllRequest->owner = this;

    return prechargeAllRequest;
}

NVMainRequest *MemoryController::MakePrechargeAllRequest( const ncounter_t row,
                                                          const ncounter_t col,
                                                          const ncounter_t bank,
                                                          const ncounter_t rank,
                                                          const ncounter_t subarray )
{
    NVMainRequest *prechargeAllRequest = new NVMainRequest( );

    prechargeAllRequest->type = PRECHARGE_ALL;
    ncounter_t preAddr = GetDecoder( )->ReverseTranslate( row, col, bank, rank, id, subarray );
    prechargeAllRequest->address.SetPhysicalAddress( preAddr );
    prechargeAllRequest->address.SetTranslatedAddress( row, col, bank, rank, id, subarray );
    prechargeAllRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    prechargeAllRequest->owner = this;

    return prechargeAllRequest;
}

NVMainRequest *MemoryController::MakeImplicitPrechargeRequest( NVMainRequest *triggerRequest )
{
    if( triggerRequest->type == READ )
        triggerRequest->type = READ_PRECHARGE;
    else if( triggerRequest->type == WRITE )
        triggerRequest->type = WRITE_PRECHARGE;

    triggerRequest->issueCycle = GetEventQueue()->GetCurrentCycle();

    return triggerRequest;
}

NVMainRequest *MemoryController::MakeRefreshRequest( const ncounter_t row,
                                                     const ncounter_t col,
                                                     const ncounter_t bank,
                                                     const ncounter_t rank,
                                                     const ncounter_t subarray )
{
    NVMainRequest *refreshRequest = new NVMainRequest( );

    refreshRequest->type = REFRESH;
    ncounter_t preAddr = GetDecoder( )->ReverseTranslate( row, col, bank, rank, id, subarray );
    refreshRequest->address.SetPhysicalAddress( preAddr );
    refreshRequest->address.SetTranslatedAddress( row, col, bank, rank, id, subarray );
    refreshRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    refreshRequest->owner = this;

    return refreshRequest;
}

NVMainRequest *MemoryController::MakePowerdownRequest( OpType pdOp,
                                                       const ncounter_t rank )
{
    NVMainRequest *powerdownRequest = new NVMainRequest( );

    powerdownRequest->type = pdOp;
    ncounter_t pdAddr = GetDecoder( )->ReverseTranslate( 0, 0, 0, rank, id, 0 );
    powerdownRequest->address.SetPhysicalAddress( pdAddr );
    powerdownRequest->address.SetTranslatedAddress( 0, 0, 0, rank, id, 0 );
    powerdownRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    powerdownRequest->owner = this;

    return powerdownRequest;
}

NVMainRequest *MemoryController::MakePowerupRequest( const ncounter_t rank )
{
    NVMainRequest *powerupRequest = new NVMainRequest( );

    powerupRequest->type = POWERUP;
    ncounter_t puAddr = GetDecoder( )->ReverseTranslate( 0, 0, 0, rank, id, 0 );
    powerupRequest->address.SetPhysicalAddress( puAddr );
    powerupRequest->address.SetTranslatedAddress( 0, 0, 0, rank, id, 0 );
    powerupRequest->issueCycle = GetEventQueue()->GetCurrentCycle();
    powerupRequest->owner = this;

    return powerupRequest;
}

bool MemoryController::IsLastRequest( std::list<NVMainRequest *>& transactionQueue,
                                      NVMainRequest *request )
{
    bool rv = true;
    
    if( p->ClosePage == 0 )
    {
        rv = false;
    }
    else if( p->ClosePage == 1 )
    {
        ncounter_t mRank, mBank, mRow, mSubArray;
        request->address.GetTranslatedAddress( &mRow, NULL, &mBank, &mRank, NULL, &mSubArray );
        std::list<NVMainRequest *>::iterator it;

        for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
        {
            ncounter_t rank, bank, row, subarray;

            (*it)->address.GetTranslatedAddress( &row, NULL, &bank, &rank, NULL, &subarray );

            /* if a request that has row buffer hit is found, return false */ 
            if( rank == mRank && bank == mBank && row == mRow && subarray == mSubArray )
            {
                rv = false;
                break;
            }
        }
    }

    return rv;
}

bool MemoryController::FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, 
                                           NVMainRequest **starvedRequest )
{
    DummyPredicate pred;

    return FindStarvedRequest( transactionQueue, starvedRequest, pred );
}

bool MemoryController::FindStarvedRequest( std::list<NVMainRequest *>& transactionQueue, 
                                           NVMainRequest **starvedRequest, 
                                           SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *starvedRequest = NULL;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank, row, subarray, col;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( &row, &col, &bank, &rank, NULL, &subarray );
        
        /* By design, mux level can only be a subset of the selected columns. */
        ncounter_t muxLevel = static_cast<ncounter_t>(col / p->RBSize);

        if( activateQueued[rank][bank] 
            && ( !activeSubArray[rank][bank][subarray]          /* The subarray is inactive */
                || effectiveRow[rank][bank][subarray] != row    /* Row buffer miss */
                || effectiveMuxedRow[rank][bank][subarray] != muxLevel )  /* Subset of row buffer is not at the sense amps */
            && !bankNeedRefresh[rank][bank]                     /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]                       /* Don't interrupt refreshes queued on bank group head. */
            && starvationCounter[rank][bank][subarray] 
                >= starvationThreshold                          /* This subarray has reached starvation threshold */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && commandQueues[queueId].empty()                   /* The request queue is empty */
            && pred( (*it) ) )                                  /* User-defined predicate is true */
        {
            *starvedRequest = (*it);
            transactionQueue.erase( it );

            /* Different row buffer management policy has different behavior */ 

            /* 
             * if Relaxed Close-Page row buffer management policy is applied,
             * we check whether there is another request has row buffer hit.
             * if not, this request is the last request and we can close the
             * row.
             */
            if(  IsLastRequest( transactionQueue, (*starvedRequest) ) )
                (*starvedRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

/*
 *  Find any requests that can be serviced without going through a normal activation cycle.
 */
bool MemoryController::FindCachedAddress( std::list<NVMainRequest *>& transactionQueue,
                                              NVMainRequest **accessibleRequest )
{
    DummyPredicate pred;

    return FindCachedAddress( transactionQueue, accessibleRequest, pred );
}

/*
 *  Find any requests that can be serviced without going through a normal activation cycle.
 */
bool MemoryController::FindCachedAddress( std::list<NVMainRequest *>& transactionQueue,
                                              NVMainRequest **accessibleRequest, 
                                              SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *accessibleRequest = NULL;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        ncounter_t queueId = GetCommandQueueId( (*it)->address );
        NVMainRequest *cachedRequest = MakeCachedRequest( (*it) );
        
        if( commandQueues[queueId].empty() 
            && GetChild( )->IsIssuable( cachedRequest )
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && pred( (*it ) ) )
        {
            *accessibleRequest = (*it);
            transactionQueue.erase( it );

            delete cachedRequest;

            rv = true;
            break;
        }

        delete cachedRequest;
    }

    return rv;
}

bool MemoryController::FindWriteStalledRead( std::list<NVMainRequest *>& transactionQueue,
                                             NVMainRequest **hitRequest )
{
    DummyPredicate pred;

    return FindWriteStalledRead( transactionQueue, hitRequest, pred );
}

bool MemoryController::FindWriteStalledRead( std::list<NVMainRequest *>& transactionQueue, 
                                             NVMainRequest **hitRequest, SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *hitRequest = NULL;

    if( !p->WritePausing )
        return false;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        if( (*it)->type != READ )
            continue;

        ncounter_t rank, bank, row, subarray, col;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( &row, &col, &bank, &rank, NULL, &subarray );

        /* Find the requests's SubArray destination. */
        SubArray *writingArray = FindChild( (*it), SubArray );

        /* Assume the memory has no subarrays if we don't find the destination. */
        if( writingArray == NULL )
            return false;

        NVMainRequest *testActivate = MakeActivateRequest( (*it) );
        testActivate->flags |= NVMainRequest::FLAG_PRIORITY; 

        if( !bankNeedRefresh[rank][bank]                 /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]                /* Don't interrupt refreshes queued on bank group head. */
            && writingArray->IsWriting( )                /* There needs to be a write to cancel. */
            && ( GetChild( )->IsIssuable( (*it ) )       /* Check for RB hit pause */
            || GetChild( )->IsIssuable( testActivate ) ) /* See if we can activate to pause. */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && commandQueues[queueId].empty()            /* The request queue is empty */
            && pred( (*it) ) )                           /* User-defined predicate is true */
        {
            if( !writingArray->BetweenWriteIterations( ) && p->pauseMode == PauseMode_Normal )
            {
                delete testActivate;

                /* Stall the scheduler by returning true. */
                rv = true;
                break;
            }

            *hitRequest = (*it);
            transactionQueue.erase( it );

            delete testActivate;

            /* Different row buffer management policy has different behavior */ 

            /* 
             * if Relaxed Close-Page row buffer management policy is applied,
             * we check whether there is another request has row buffer hit.
             * if not, this request is the last request and we can close the
             * row.
             */
            if( IsLastRequest( transactionQueue, (*hitRequest) ) )
                (*hitRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;

            break;
        }

        delete testActivate;
    }

    return rv;
}

bool MemoryController::FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, 
                                         NVMainRequest **hitRequest )
{
    DummyPredicate pred;

    return FindRowBufferHit( transactionQueue, hitRequest, pred );
}

bool MemoryController::FindRowBufferHit( std::list<NVMainRequest *>& transactionQueue, 
                                         NVMainRequest **hitRequest, SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *hitRequest = NULL;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank, row, subarray, col;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( &row, &col, &bank, &rank, NULL, &subarray );

        /* By design, mux level can only be a subset of the selected columns. */
        ncounter_t muxLevel = static_cast<ncounter_t>(col / p->RBSize);

        if( activateQueued[rank][bank]                    /* The bank is active */ 
            && activeSubArray[rank][bank][subarray]       /* The subarray is open */
            && effectiveRow[rank][bank][subarray] == row  /* The effective row is the row of this request */ 
            && effectiveMuxedRow[rank][bank][subarray] == muxLevel  /* Subset of row buffer is currently at the sense amps */
            && !bankNeedRefresh[rank][bank]               /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]                 /* Don't interrupt refreshes queued on bank group head. */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && commandQueues[queueId].empty( )            /* The request queue is empty */
            && pred( (*it) ) )                            /* User-defined predicate is true */
        {
            *hitRequest = (*it);
            transactionQueue.erase( it );

            /* Different row buffer management policy has different behavior */ 

            /* 
             * if Relaxed Close-Page row buffer management policy is applied,
             * we check whether there is another request has row buffer hit.
             * if not, this request is the last request and we can close the
             * row.
             */
            if( IsLastRequest( transactionQueue, (*hitRequest) ) )
                (*hitRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;

            break;
        }
    }

    return rv;
}

bool MemoryController::FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, 
                                               NVMainRequest **oldestRequest )
{
    DummyPredicate pred;

    return FindOldestReadyRequest( transactionQueue, oldestRequest, pred );
}

bool MemoryController::FindOldestReadyRequest( std::list<NVMainRequest *>& transactionQueue, 
                                               NVMainRequest **oldestRequest, 
                                               SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *oldestRequest = NULL;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( activateQueued[rank][bank]         /* The bank is active */ 
            && !bankNeedRefresh[rank][bank]    /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]      /* Don't interrupt refreshes queued on bank group head. */
            && commandQueues[queueId].empty()  /* The request queue is empty */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && pred( (*it) ) )                 /* User-defined predicate is true. */
        {
            *oldestRequest = (*it);
            transactionQueue.erase( it );
            
            /* Different row buffer management policy has different behavior */ 

            /* 
             * if Relaxed Close-Page row buffer management policy is applied,
             * we check whether there is another request has row buffer hit.
             * if not, this request is the last request and we can close the
             * row.
             */
            if( IsLastRequest( transactionQueue, (*oldestRequest) ) )
                (*oldestRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

bool MemoryController::FindComputeRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **computeRequest )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *computeRequest = NULL;
    for( it = transactionQueue.begin();it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( !activateQueued[rank][bank]         /* This bank is inactive */
            && !bankNeedRefresh[rank][bank]     /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]       /* Don't interrupt refreshes queued on bank group head. */
            && commandQueues[queueId].empty()   /* The request queue is empty */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle() )
        {
            *computeRequest = (*it);
            transactionQueue.erase( it );

            if( IsLastRequest( transactionQueue, (*computeRequest) ) )
                (*computeRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

bool MemoryController::FindTransferRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **transferRequest )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *transferRequest = NULL;
    for( it = transactionQueue.begin();it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( commandQueues[queueId].empty()   /* The request queue is empty */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle() )
        {
            *transferRequest = (*it);
            transactionQueue.erase( it );

            if( IsLastRequest( transactionQueue, (*transferRequest) ) )
                (*transferRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

bool MemoryController::FindLoadRequest( std::list<NVMainRequest *>& transactionQueue, NVMainRequest **loadRequest )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *loadRequest = NULL;
    for( it = transactionQueue.begin();it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( commandQueues[queueId].empty()   /* The request queue is empty */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle() )
        {
            *loadRequest = (*it);
            transactionQueue.erase( it );

            if( IsLastRequest( transactionQueue, (*loadRequest) ) )
                (*loadRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

bool MemoryController::FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, 
                                              NVMainRequest **closedRequest )
{
    DummyPredicate pred;

    return FindClosedBankRequest( transactionQueue, closedRequest, pred );
}

bool MemoryController::FindClosedBankRequest( std::list<NVMainRequest *>& transactionQueue, 
                                              NVMainRequest **closedRequest, 
                                              SchedulingPredicate& pred )
{
    bool rv = false;
    std::list<NVMainRequest *>::iterator it;

    *closedRequest = NULL;

    for( it = transactionQueue.begin(); it != transactionQueue.end(); it++ )
    {
        ncounter_t rank, bank;
        ncounter_t queueId = GetCommandQueueId( (*it)->address );

        if( !commandQueues[queueId].empty() ) continue;

        (*it)->address.GetTranslatedAddress( NULL, NULL, &bank, &rank, NULL, NULL );

        if( !activateQueued[rank][bank]         /* This bank is inactive */
            && !bankNeedRefresh[rank][bank]     /* The bank is not waiting for a refresh */
            && !refreshQueued[rank][bank]       /* Don't interrupt refreshes queued on bank group head. */
            && commandQueues[queueId].empty()   /* The request queue is empty */
            && (*it)->arrivalCycle != GetEventQueue()->GetCurrentCycle()
            && pred( (*it) ) )                  /* User defined predicate is true. */
        {
            *closedRequest = (*it);
            transactionQueue.erase( it );
            
            /* Different row buffer management policy has different behavior */ 

            /* 
             * if Relaxed Close-Page row buffer management policy is applied,
             * we check whether there is another request has row buffer hit.
             * if not, this request is the last request and we can close the
             * row.
             */
            if( IsLastRequest( transactionQueue, (*closedRequest) ) )
                (*closedRequest)->flags |= NVMainRequest::FLAG_LAST_REQUEST;

            rv = true;
            break;
        }
    }

    return rv;
}

bool MemoryController::DummyPredicate::operator() ( NVMainRequest* /*request*/ )
{
    return true;
}


/*
 *  NOTE: This function assumes the memory controller uses any predicates when
 *  scheduling. They will not be re-checked here.
 */
bool MemoryController::IssueMemoryCommands( NVMainRequest *req )
{
    bool rv = false;
    ncounter_t rank, bank, row, subarray, col;

    req->address.GetTranslatedAddress( &row, &col, &bank, &rank, NULL, &subarray );

    SubArray *writingArray = FindChild( req, SubArray );

    ncounter_t muxLevel = static_cast<ncounter_t>(col / p->RBSize);
    ncounter_t queueId = GetCommandQueueId( req->address );

    /*
     *  If the request is somehow accessible (e.g., via caching, etc), but the
     *  bank state does not match the memory controller, just issue the request
     *  without updating any internal states.
     */
    FailReason reason;
    NVMainRequest *cachedRequest = MakeCachedRequest( req );

    if( GetChild( )->IsIssuable( cachedRequest, &reason ) )
    {
        /* Differentiate from row-buffer hits. */
        if ( !activateQueued[rank][bank] 
             || !activeSubArray[rank][bank][subarray]
             || effectiveRow[rank][bank][subarray] != row 
             || effectiveMuxedRow[rank][bank][subarray] != muxLevel ) 
        {
            req->issueCycle = GetEventQueue()->GetCurrentCycle();

            // Update starvation ??
            commandQueues[queueId].push_back( req );

            delete cachedRequest;

            return true;
        }
        else
        {
            delete cachedRequest;
        }
    }
    else
    {
        delete cachedRequest;
    }


    if( !activateQueued[rank][bank] && commandQueues[queueId].empty() )
    {  
        /* Any activate will request the starvation counter */
        activateQueued[rank][bank] = true;
        activeSubArray[rank][bank][subarray] = true;
        effectiveRow[rank][bank][subarray] = row;
        effectiveMuxedRow[rank][bank][subarray] = muxLevel;
        starvationCounter[rank][bank][subarray] = 0;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        NVMainRequest *actRequest = MakeActivateRequest( req );
        actRequest->flags |= (writingArray != NULL && writingArray->IsWriting( )) ? NVMainRequest::FLAG_PRIORITY : 0;
        commandQueues[queueId].push_back( actRequest );

        /* Different row buffer management policy has different behavior */ 
        /*
         * There are two possibilities that the request is the last request:
         * 1) ClosePage == 1 and there is no other request having row
         * buffer hit
         * or 2) ClosePage == 2, the request is always the last request
         */
        if( req->flags & NVMainRequest::FLAG_LAST_REQUEST && p->UsePrecharge )
        {
            assert(!(req->type == COMPUTE));
            commandQueues[queueId].push_back( MakeImplicitPrechargeRequest( req ) );
            activeSubArray[rank][bank][subarray] = false;
            effectiveRow[rank][bank][subarray] = p->ROWS;
            effectiveMuxedRow[rank][bank][subarray] = p->ROWS;
            activateQueued[rank][bank] = false;
        }
        else
        {
            activeSubArray[rank][bank][subarray] = false;
            effectiveRow[rank][bank][subarray] = p->ROWS;
            effectiveMuxedRow[rank][bank][subarray] = p->ROWS;
            activateQueued[rank][bank] = false;

            if ( req->type == COMPUTE )
            {
                req->Cycle_n = 0;
                req->Buffer_n = req->BufferSize;
                req->row = 1;
                req->col = 1;
                req->rowIntr = false;
                req->isReused = false;
                //req->slide = globalparams.slide;

                NVMainRequest *rcRequest = MakeReadCycleRequest( req );
                //flags need setting ??
                commandQueues[queueId].push_back( rcRequest );

                NVMainRequest *cRequest = MakeRealComputeRequest( req );
                commandQueues[queueId].push_back( cRequest );
            
                NVMainRequest *prRequest = MakePostReadRequest( req );
                commandQueues[queueId].push_back( prRequest );

                NVMainRequest *wcRequest = MakeWriteCycleRequest( req );
                commandQueues[queueId].push_back( wcRequest );

                req->isBuffer = true;
            }

            commandQueues[queueId].push_back( req );
            //std::cout << "[+] im here？？？" <<" id " << queueId << std::endl;
        }
        //std::cout << "[+] im here" << std::endl;
        rv = true;
    }
    else if( activateQueued[rank][bank] 
            && ( !activeSubArray[rank][bank][subarray] 
                || effectiveRow[rank][bank][subarray] != row 
                || effectiveMuxedRow[rank][bank][subarray] != muxLevel )
            && commandQueues[queueId].empty() )
    {
        /* Any activate will request the starvation counter */
        starvationCounter[rank][bank][subarray] = 0;
        activateQueued[rank][bank] = true;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        if( activeSubArray[rank][bank][subarray] && p->UsePrecharge )
        {
            commandQueues[queueId].push_back( 
                    MakePrechargeRequest( effectiveRow[rank][bank][subarray], 0, bank, rank, subarray ) );
        }

        NVMainRequest *actRequest = MakeActivateRequest( req );
        actRequest->flags |= (writingArray != NULL && writingArray->IsWriting( )) ? NVMainRequest::FLAG_PRIORITY : 0;
        commandQueues[queueId].push_back( actRequest );

        if ( req->type == COMPUTE )
        {
            req->Cycle_n = 0;
            req->Buffer_n = req->BufferSize;
            req->row = 1;
            req->col = 1;
            req->rowIntr = false;
            req->isReused = false;
            //req->slide = globalparams.slide;
            
            NVMainRequest *rcRequest = MakeReadCycleRequest( req );
            //flags need setting ??
            commandQueues[queueId].push_back( rcRequest );

            NVMainRequest *cRequest = MakeRealComputeRequest( req );
            commandQueues[queueId].push_back( cRequest );
            
            NVMainRequest *prRequest = MakePostReadRequest( req );
            commandQueues[queueId].push_back( prRequest );

            NVMainRequest *wcRequest = MakeWriteCycleRequest( req );
            commandQueues[queueId].push_back( wcRequest );

            req->isBuffer = true;
        }

        commandQueues[queueId].push_back( req );
        activeSubArray[rank][bank][subarray] = true;
        effectiveRow[rank][bank][subarray] = row;
        effectiveMuxedRow[rank][bank][subarray] = muxLevel;

        rv = true;
    }
    else if( activateQueued[rank][bank] 
            && activeSubArray[rank][bank][subarray]
            && effectiveRow[rank][bank][subarray] == row 
            && effectiveMuxedRow[rank][bank][subarray] == muxLevel )
    {
        starvationCounter[rank][bank][subarray]++;

        req->issueCycle = GetEventQueue()->GetCurrentCycle();

        /* Different row buffer management policy has different behavior */ 
        /*
         * There are two possibilities that the request is the last request:
         * 1) ClosePage == 1 and there is no other request having row
         * buffer hit
         * or 2) ClosePage == 2, the request is always the last request
         */
        if( req->flags & NVMainRequest::FLAG_LAST_REQUEST && p->UsePrecharge )
        {
            /* if Restricted Close-Page is applied, we should never be here */
            assert( p->ClosePage != 2 );

            assert(!(req->type == COMPUTE));

            commandQueues[queueId].push_back( MakeImplicitPrechargeRequest( req ) );
            activeSubArray[rank][bank][subarray] = false;
            effectiveRow[rank][bank][subarray] = p->ROWS;
            effectiveMuxedRow[rank][bank][subarray] = p->ROWS;

            bool idle = true;
            for( ncounter_t i = 0; i < subArrayNum; i++ )
            {
                if( activeSubArray[rank][bank][i] == true )
                {
                    idle = false;
                    break;
                }
            }

            if( idle )
                activateQueued[rank][bank] = false;
        }
        else
        {
            if ( req->type == COMPUTE )
            {
                req->Cycle_n = 0;
                req->Buffer_n = req->BufferSize;
                req->rowIntr = false;
                req->row = 1;
                req->col = 1;
                req->isReused = false;
                //req->slide = globalparams.slide;
                
                NVMainRequest *rcRequest = MakeReadCycleRequest( req );
                //flags need setting ??
                commandQueues[queueId].push_back( rcRequest );

                NVMainRequest *cRequest = MakeRealComputeRequest( req );
                commandQueues[queueId].push_back( cRequest );
            
                NVMainRequest *prRequest = MakePostReadRequest( req );
                commandQueues[queueId].push_back( prRequest );

                NVMainRequest *wcRequest = MakeWriteCycleRequest( req );
                commandQueues[queueId].push_back( wcRequest );

                req->isBuffer = true;
            }    
            
            commandQueues[queueId].push_back( req );
        }

        rv = true;
    }
    else
    {
        rv = false;
    }

    /* Schedule wake event for memory commands if not scheduled. */
    if( rv == true )
    {
        ScheduleCommandWake( );
    }

    return rv;
}

void MemoryController::CycleCommandQueues( )
{
    //HandleLowPower( );

    /* If a refresh event schedule for this cycle was handled, we are done. */
    if( handledRefresh == GetEventQueue()->GetCurrentCycle() )
    {
        return;
    }

    for( ncounter_t queueIdx = 0; queueIdx < commandQueueCount; queueIdx++ )
    {
        /* 
         * Requests are placed in queues in priority order, so we can simply
         * iterator over all queues.
         */
        ncounter_t queueId = (curQueue + queueIdx) % commandQueueCount;
        //std::cout << "[+] test for queueId " << queueId << std::endl;
        FailReason fail;

        /*
        std::cout << "[+] test wrong" << std::endl;
        if( commandQueues[queueId].empty( ) )
            std::cout << "[+] 1****************" << std::endl;
        else if( !(lastIssueCycle != GetEventQueue()->GetCurrentCycle() ))
            std::cout << "[+] 2************" << std::endl;
        */
        if( !commandQueues[queueId].empty( )
            && lastIssueCycle != GetEventQueue()->GetCurrentCycle()
            && GetChild( )->IsIssuable( commandQueues[queueId].at( 0 ), &fail ) )
        {
            NVMainRequest *queueHead = commandQueues[queueId].at( 0 );

            *debugStream << GetEventQueue()->GetCurrentCycle() << " MemoryController: Issued request type "
                         << queueHead->type << " for address 0x" << std::hex 
                         << queueHead->address.GetPhysicalAddress()
                         << std::dec << " for queue " << queueId << std::endl;
            //std::cout << "[+] test for queueId " << queueId << std::endl;

            if( queueHead->type == COMPUTE )
            {
                if ( queueHead->Buffer_n > 1 )
                {
                    commandQueues[queueId].pop_front();
                    commandQueues[queueId].push_front(MakeComputeRequest( queueHead ));

                    /*
                    queueHead->Cycle_n++;
                    //if( queueHead->Cycle_n = p->DeviceWidth*8 / globalparams.BitWidth )
                    if( queueHead->Cycle_n == 2)
                    {
                        queueHead->address.SetPhysicalAddress( queueHead->address.GetPhysicalAddress() + 1 );
                        NVMainRequest *rcRequest = MakeReadCycleRequest( queueHead );
                        //flags need setting ??
                        commandQueues[queueId].push_back( rcRequest );
                        queueHead->Cycle_n = 0;
                    }
                    */
                    
                    NVMainRequest *cRequest = MakeRealComputeRequest( queueHead );
                    commandQueues[queueId].push_back( cRequest );
            
                    NVMainRequest *prRequest = MakePostReadRequest( queueHead );
                    commandQueues[queueId].push_back( prRequest );

                    NVMainRequest *wcRequest = MakeWriteCycleRequest( queueHead );
                    commandQueues[queueId].push_back( wcRequest );

                    queueHead->Buffer_n--;
                    commandQueues[queueId].push_back(queueHead);
                    queueHead = commandQueues[queueId].at( 0 );
                }    
                else if(queueHead->slide == X)
                {
                    if(!queueHead->ColComplete)
                    {
                        queueHead->isBuffer = false;             
                        commandQueues[queueId].pop_front();
                        commandQueues[queueId].push_front(MakeComputeRequest( queueHead ));

                        queueHead->col = queueHead->col + queueHead->BufferSize / 2;
                        ncounter_t rank, bank, row, subarray, col, channel;
                        queueHead->address.GetTranslatedAddress( &row, &col, &bank, &rank, &channel, &subarray );
                        
                        col = col + queueHead->BufferSize / 2;

                        while (col >=p->COLS)
                        {
                            col = col - p->COLS;
                            row++;
                            if( row == p->ROWS)
                            {
                                std::cout << "[-](CycleCommandQueues) wrong command: load data from different bank " << std::endl;
                                assert(row < p->ROWS);
                            }
                        }

                        /*
                        if ( !queueHead->rowIntr )
                            col = col + queueHead->BufferSize / 2;
                        else
                        {
                            col = col + queueHead->BufferSize / 2 - p->COLS;
                            row++ ;
                            if( row = p->ROWS )
                            {
                                std::cout << "[-](CycleCommandQueues) wrong command: load data from different bank " << std::endl;
                                assert(row < p->ROWS);
                            }
                        }
                        */
                        std::cout << "[+]  now is col " << col << " row " << row << std::endl;
                        queueHead->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
                        
                        queueHead->BufferSize = globalparams.Buffer_n;
                        if(( queueHead->col + queueHead->BufferSize / 2 + globalparams.K_Col - 2) >= globalparams.Input_Col)
                        {
                            queueHead->BufferSize = 2*(globalparams.Input_Row - queueHead->col + 2 - globalparams.K_Col);
                            queueHead->ColComplete = true;
                        }
                        queueHead->Buffer_n = queueHead->BufferSize;
                        std::cout << "[+] buffer_n is " << queueHead->Buffer_n << std::endl;
                        /*
                        if((col + queueHead->Buffer_n / 2 + globalparams.K_Col - 1) >= p->COLS)
                            queueHead->rowIntr = true;
                        else
                            queueHead->rowIntr = false;
                        */
                        
                        queueHead->isReused = true;
                        commandQueues[queueId].push_back(MakeActivateRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeReadCycleRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeRealComputeRequest( queueHead ));
                        commandQueues[queueId].push_back(MakePostReadRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeWriteCycleRequest( queueHead ));

                        queueHead->isBuffer = true;
                        commandQueues[queueId].push_back(queueHead);
                        queueHead = commandQueues[queueId].at( 0 );
                    }
                    else if(!queueHead->RowComplete)
                    {
                        queueHead->ColComplete = false;
                        queueHead->isBuffer = false;
                        queueHead->col = 1;
                        queueHead->row = queueHead->row + 1;
                        
                        ncounter_t rank, bank, row, subarray, col, channel;
                        queueHead->C_address1.GetTranslatedAddress( &row, &col, &bank, &rank, &channel, &subarray );
                        col = col + (queueHead->row - 1)*globalparams.Input_Row;
                        while (col>=p->COLS)
                        {
                            col = col - p->COLS;
                            row++;
                            if( row == p->ROWS)
                            {
                                std::cout << "[-](CycleCommandQueues) wrong command: load data from different bank " << std::endl;
                                assert(row < p->ROWS);
                            }
                        }
                        queueHead->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );

                        commandQueues[queueId].pop_front();
                        commandQueues[queueId].push_front(MakeComputeRequest( queueHead ));
                        queueHead->BufferSize = globalparams.Buffer_n;
                        if(( queueHead->col + queueHead->BufferSize / 2 + globalparams.K_Col - 2) >= globalparams.Input_Col)
                        {
                            queueHead->BufferSize = 2*(globalparams.Input_Row - queueHead->col + 2 - globalparams.K_Col);
                            queueHead->ColComplete = true;
                        }
                        queueHead->Buffer_n = queueHead->BufferSize;
                        std::cout << "[+] buffer is " << queueHead->Buffer_n << std::endl;
                        if(( queueHead->row + globalparams.K_Row - 1 ) >= globalparams.Input_Row)
                        {
                            queueHead->RowComplete = true;
                        }
                        
                        queueHead->isReused = false;
                        commandQueues[queueId].push_back(MakeActivateRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeReadCycleRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeRealComputeRequest( queueHead ));
                        commandQueues[queueId].push_back(MakePostReadRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeWriteCycleRequest( queueHead ));

                        queueHead->isBuffer = true;
                        commandQueues[queueId].push_back(queueHead);
                        queueHead = commandQueues[queueId].at( 0 );
                    }
                    else
                    {
                        assert(queueHead->RowComplete);
                        queueHead->isBuffer = false;
                    }
                    
                }
                else if(queueHead->slide == Y)
                {
                    if(!queueHead->RowComplete)
                    {
                        queueHead->isBuffer = false;
                        commandQueues[queueId].pop_front();
                        commandQueues[queueId].push_front(MakeComputeRequest( queueHead ));

                        queueHead->row++;
                        queueHead->Buffer_n = queueHead->BufferSize;
                        std::cout << "[+] buffer_n is " << queueHead->Buffer_n << std::endl;

                        ncounter_t rank, bank, row, subarray, col, channel;
                        queueHead->C_address1.GetTranslatedAddress( &row, &col, &bank, &rank, &channel, &subarray );
                        std::cout << "[+]  point is col " << queueHead->col << " row " << queueHead->row << std::endl;
                        col = col + (queueHead->row - 1)*globalparams.Input_Col + queueHead->col - 1;
                        while (col >=p->COLS)
                        {
                            col = col - p->COLS;
                            row++;
                            if( row == p->ROWS)
                            {
                                std::cout << "[-](CycleCommandQueues) wrong command: load data from different bank " << std::endl;
                                assert(row < p->ROWS);
                            }
                        }
                        std::cout << "[+]  now is col " << col << " row " << row << std::endl;
                        queueHead->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );

                        if(( queueHead->row + globalparams.K_Row - 1 ) >= globalparams.Input_Row)
                        {
                            queueHead->RowComplete = true;
                        }
                        
                        queueHead->isReused = true;
                        commandQueues[queueId].push_back(MakeActivateRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeReadCycleRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeRealComputeRequest( queueHead ));
                        commandQueues[queueId].push_back(MakePostReadRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeWriteCycleRequest( queueHead ));

                        queueHead->isBuffer = true;
                        commandQueues[queueId].push_back(queueHead);
                        queueHead = commandQueues[queueId].at( 0 );
                    }
                    else if(!queueHead->ColComplete)
                    {
                        queueHead->RowComplete = false;
                        queueHead->isBuffer = false;
                        queueHead->col = queueHead->col + queueHead->BufferSize / 2;
                        queueHead->row = 1;
                        
                        commandQueues[queueId].pop_front();
                        commandQueues[queueId].push_front(MakeComputeRequest( queueHead ));

                        ncounter_t rank, bank, row, subarray, col, channel;
                        queueHead->C_address1.GetTranslatedAddress( &row, &col, &bank, &rank, &channel, &subarray );
                        col = col + (queueHead->row - 1)*globalparams.Input_Row + queueHead->col - 1;
                        while (col>=p->COLS)
                        {
                            col = col - p->COLS;
                            row++;
                            if( row == p->ROWS)
                            {
                                std::cout << "[-](CycleCommandQueues) wrong command: load data from different bank " << std::endl;
                                assert(row < p->ROWS);
                            }
                        }
                        queueHead->address.SetTranslatedAddress( row, col, bank, rank, channel, subarray );
                        std::cout << "[+]  now is col " << col << " row " << row << std::endl;
                        queueHead->BufferSize = globalparams.Buffer_n;
                        if(( queueHead->col + queueHead->BufferSize / 2 + globalparams.K_Col - 2) >= globalparams.Input_Col)
                        {
                            queueHead->BufferSize = 2*(globalparams.Input_Row - queueHead->col + 2 - globalparams.K_Col);
                            queueHead->ColComplete = true;
                        }
                        queueHead->Buffer_n = queueHead->BufferSize;
                        std::cout << "[+] buffer is " << queueHead->Buffer_n << std::endl;

                        queueHead->isReused = false;
                        commandQueues[queueId].push_back(MakeActivateRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeReadCycleRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeRealComputeRequest( queueHead ));
                        commandQueues[queueId].push_back(MakePostReadRequest( queueHead ));
                        commandQueues[queueId].push_back(MakeWriteCycleRequest( queueHead ));

                        queueHead->isBuffer = true;
                        commandQueues[queueId].push_back(queueHead);
                        queueHead = commandQueues[queueId].at( 0 );
                    }
                    else
                    {
                        assert(queueHead->ColComplete);
                        queueHead->isBuffer = false;
                    }
                    
                }
            }

            GetChild( )->IssueCommand( queueHead );

            queueHead->flags |= NVMainRequest::FLAG_ISSUED;

            if( queueHead->type == REFRESH )
                ResetRefreshQueued( queueHead->address.GetBank(),
                                    queueHead->address.GetRank() );

            if( GetEventQueue( )->GetCurrentCycle( ) != lastIssueCycle )
                lastIssueCycle = GetEventQueue( )->GetCurrentCycle( );

            /* Get this cleaned this up. */
            ncycle_t cleanupCycle = GetEventQueue()->GetCurrentCycle() + 1;
            bool cleanupScheduled = GetEventQueue()->FindCallback( this, 
                                        (CallbackPtr)&MemoryController::CleanupCallback,
                                        cleanupCycle, NULL, cleanupPriority );

            if( !cleanupScheduled )
                GetEventQueue( )->InsertCallback( this, 
                                  (CallbackPtr)&MemoryController::CleanupCallback,
                                  cleanupCycle, NULL, cleanupPriority );

            /* If the bank queue will be empty, we can issue another transaction, so wakeup the system. */
            if( commandQueues[queueId].size( ) == 1 )
            {
                /* If there is a transaction for this command queue, wake immediately. */
                if( TransactionAvailable( queueId ) )
                {
                    ncycle_t nextWakeup = GetEventQueue( )->GetCurrentCycle( ) + 1;

                    GetEventQueue( )->InsertEvent( EventCycle, this, nextWakeup, NULL, transactionQueuePriority );
                }
            }

            MoveCurrentQueue( );

            /* we should return since one time only one command can be issued */
            return;
        }
        else if( !commandQueues[queueId].empty( ) )
        {
            NVMainRequest *queueHead = commandQueues[queueId].at( 0 );

            if( ( GetEventQueue()->GetCurrentCycle() - queueHead->issueCycle ) > p->DeadlockTimer )
            {
                ncounter_t row, col, bank, rank, channel, subarray;
                queueHead->address.GetTranslatedAddress( &row, &col, &bank, &rank, &channel, &subarray );
                std::cout << "[+] NVMain Warning: Operation could not be sent to memory after a very long time: "
                          << std::endl; 
                std::cout << "[+]          Address: 0x" << std::hex 
                          << queueHead->address.GetPhysicalAddress( )
                          << std::dec << " @ Bank " << bank << ", Rank " << rank << ", Channel " << channel
                          << " Subarray " << subarray << " Row " << row << " Column " << col
                          << ". Queued time: " << queueHead->arrivalCycle
                          << ". Issue time: " << queueHead->issueCycle
                          << ". Current time: " << GetEventQueue()->GetCurrentCycle() << ". Type: " 
                          << queueHead->type << std::endl;

                // Give the opportunity to attach a debugger here.
#ifndef NDEBUG
                raise( SIGSTOP );
#endif
                GetStats( )->PrintAll( std::cerr );
                exit(1);
            }
        }
    }
}

/*
 * Decode command queue in priority order
 *
 * 0 -- Fixed Scheduling from Rank0 and Bank0
 * 1 -- Rank-first round-robin
 * 2 -- Bank-first round-robin
 */
ncounter_t MemoryController::GetCommandQueueId( NVMAddress addr )
{
    ncounter_t queueId = std::numeric_limits<ncounter_t>::max( );

    if( queueModel == PerRankQueues )
    {
        queueId = addr.GetRank( );
    }
    else if( queueModel == PerBankQueues )
    {
        /* Decode queue id in priority order. */
        if( p->ScheduleScheme == 1 )
        {
            /* Rank-first round-robin */
            queueId = (addr.GetBank( ) * p->RANKS + addr.GetRank( ));
        }
        else if( p->ScheduleScheme == 2 )
        {
            /* Bank-first round-robin. */
            queueId = (addr.GetRank( ) * p->BANKS + addr.GetBank( ));
        }
    }
    else if( queueModel == PerSubArrayQueues )
    {   
        // TODO: ScheduleSchemes? There are 6 possible orderings now.
        queueId = (addr.GetRank( ) * p->BANKS * subArrayNum)
                + (addr.GetBank( ) * subArrayNum) + addr.GetSubArray( );
    }

    assert( queueId < commandQueueCount );

    return queueId;
}

ncycle_t MemoryController::NextIssuable( NVMainRequest * /*request*/ )
{
    /* Determine the next time we need to wakeup. */
    ncycle_t nextWakeup = std::numeric_limits<ncycle_t>::max( );

    /* Check for memory commands to issue. */
    for( ncounter_t rankIdx = 0; rankIdx < p->RANKS; rankIdx++ )
    {
        for( ncounter_t bankIdx = 0; bankIdx < p->BANKS; bankIdx++ )
        {
            ncounter_t queueIdx = GetCommandQueueId( NVMAddress( 0, 0, bankIdx, rankIdx, 0, 0 ) );

            /* Give refresh priority. */
            if( NeedRefresh( bankIdx, rankIdx )
                && IsRefreshBankQueueEmpty( bankIdx, rankIdx ) )
            {
                if( lastIssueCycle != GetEventQueue()->GetCurrentCycle() )
                    HandleRefresh( );
                 else
                     nextWakeup = GetEventQueue()->GetCurrentCycle() + 1;
            }

            if( commandQueues[queueIdx].empty( ) )
                continue;

            NVMainRequest *queueHead = commandQueues[queueIdx].at( 0 );
            //std::cout << "[+] memctr next" << nextWakeup << std::endl;
            nextWakeup = MIN( nextWakeup, GetChild( )->NextIssuable( queueHead ) );
        }
    }

    //std::cout << "[+] memctr next" << nextWakeup << std::endl;
    if( nextWakeup <= GetEventQueue( )->GetCurrentCycle( ) )
        nextWakeup = GetEventQueue( )->GetCurrentCycle( ) + 1;
    //std::cout << "[+] memctr next" << nextWakeup << std::endl;
    return nextWakeup;
}

/*
 * RankQueueEmpty() check all command queues in the given rank to see whether
 * they are empty, return true if all queues are empty
 */
bool MemoryController::RankQueueEmpty( const ncounter_t& rankId )
{
    bool rv = true;

    for( ncounter_t i = 0; i < p->BANKS; i++ )
    {
        ncounter_t queueId = GetCommandQueueId( NVMAddress( 0, 0, i, rankId, 0, 0 ) );
        if( commandQueues[queueId].empty( ) == false )
        {
            rv = false;
            break;
        }
    }

    return rv;
}

/*
 * Returns true if the command queue is empty or will be empty the next cycle
 */
bool MemoryController::EffectivelyEmpty( const ncounter_t& queueId )
{
    assert(queueId < commandQueueCount);

    bool effectivelyEmpty = (commandQueues[queueId].size( ) == 1)
                         && (WasIssued(commandQueues[queueId].at(0)));

    return (commandQueues[queueId].empty() || effectivelyEmpty);
}

/* 
 * MoveCurrentQueue() increment curQueue
 */
void MemoryController::MoveCurrentQueue( )
{
    /* if fixed scheduling is used, we do nothing */
    if( p->ScheduleScheme != 0 )
    {
        curQueue++;
        if( curQueue > commandQueueCount )
        {
            curQueue = 0;
        }
    }
}

void MemoryController::CalculateStats( )
{
    /* Sync all the child modules to the same cycle before calculating stats. */
    ncycle_t syncCycles = GetEventQueue( )->GetCurrentCycle( ) - lastCommandWake;
    GetChild( )->Cycle( syncCycles );

    simulation_cycles = GetEventQueue()->GetCurrentCycle();

    GetChild( )->CalculateStats( );
    GetDecoder( )->CalculateStats( );
}

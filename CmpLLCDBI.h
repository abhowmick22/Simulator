// -----------------------------------------------------------------------------
// File: CmpLLCDBI.h
// Description:
//    Implements a last-level cache
// This cache stores the dirty bit information in a separate DBI
// The DBI is derived from the GenericTagStore
// -----------------------------------------------------------------------------

/*
It is the job of LLC to generate writebacks with good row buffer locality that are to be sent to the
memory controller so that it can make use of this locality to avoid turnaround time ( and write recovery latency) in the channel 
(aggressive write back)
Right now, we assume that  the memory controller does this correctly, if it does not, we will have to look into 
behaviour of memory controller
*/

#ifndef __CMP_LLC_DBI_H__
#define __CMP_LLC_DBI_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "Types.h"
#include "GenericTagStore.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------

#include <iostream>
#include <bitset>
#include <fstream>
#include <string>

// -----------------------------------------------------------------------------
// Few defines
// -----------------------------------------------------------------------------

#define NUMBER_OF_ROWS 128
#define BLOCKS_PER_ROW 128
#define NUMBER_OF_BANKS 8

// -----------------------------------------------------------------------------
// Class: CmpLLCDBI
// Description:
//    Baseline lastlevel cache with DBI.
// -----------------------------------------------------------------------------

class CmpLLCDBI : public MemoryComponent {

protected:

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------

  uint32 _size;					// this is the size of in kB of the LLC
  uint32 _blockSize;
  uint32 _associativity;
  string _policy;
  string _dbipolicy;
  uint32 _policyVal;
  uint32 _dbiPolicyVal;

  uint32 _tagStoreLatency;
  uint32 _dataStoreLatency;

  uint32 _dbiSize;

  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------

  struct TagEntry {
    //bool dirty;
    addr_t vcla;
    addr_t pcla;
    uint32 appID;
    TagEntry() { //dirty = false; 
    }
  };

  struct DBIEntry {
    bitset <BLOCKS_PER_ROW> dirtyBits;
    // pointer to a clean request for this current row ?   
    DBIEntry() { 
    dirtyBits.reset(); 
    }
  };


  // tag store
  uint32 _numSets;
  generic_tagstore_t <addr_t, TagEntry> _tags;
  generic_tagstore_t <addr_t, DBIEntry> _dbi;
  policy_value_t _pval;
  policy_value_t _dbipval;

  // dbi
  uint32 _numdbiEntries;

  // per processor hit/miss counters
  vector <uint32> _hits;
  vector <uint32> _misses;

  // -------------------------------------------------------------------------
  // Output file
  // -------------------------------------------------------------------------


  // -------------------------------------------------------------------------
  // Declare Counters
  // -------------------------------------------------------------------------

  NEW_COUNTER(accesses);
  NEW_COUNTER(reads);
  NEW_COUNTER(writebacks);
  NEW_COUNTER(misses);
  NEW_COUNTER(evictions);
  NEW_COUNTER(dirty_evictions);
  NEW_COUNTER(dbievictions);

  NEW_COUNTER(dbi_eviction_writebacks);
  NEW_COUNTER(tagstore_eviction_writebacks);
  NEW_COUNTER(dbi_misses);
  NEW_COUNTER(dbi_hits);
// The last two are dicey


public:

  // -------------------------------------------------------------------------
  // Constructor. It cannot take any arguments
  // By default, dbi size is equal to the cache size
  // -------------------------------------------------------------------------

  CmpLLCDBI() {
    _size = 1024;
    _blockSize = 64;
    _associativity = 16;
    _tagStoreLatency = 6;
    _dataStoreLatency = 15;
    _policy = "lru";
    _dbipolicy = "lru";
    _policyVal = 0;
    _dbiPolicyVal = 0;
    _dbiSize = 0;			// to be used later for size
  }


  // -------------------------------------------------------------------------
  // Virtual functions to be implemented by the components
  // -------------------------------------------------------------------------

  // -------------------------------------------------------------------------
  // Function to add a parameter to the component
  // -------------------------------------------------------------------------

  void AddParameter(string pname, string pvalue) {
      
    CMP_PARAMETER_BEGIN

      // Add the list of parameters to the component here
      CMP_PARAMETER_UINT("size", _size)
      CMP_PARAMETER_UINT("block-size", _blockSize)
      CMP_PARAMETER_UINT("associativity", _associativity)
      CMP_PARAMETER_STRING("policy", _policy)
      CMP_PARAMETER_STRING("dbi-policy", _dbipolicy)
      CMP_PARAMETER_UINT("policy-value", _policyVal)
      CMP_PARAMETER_UINT("dbi-policy-value", _dbiPolicyVal)
      CMP_PARAMETER_UINT("tag-store-latency", _tagStoreLatency)
      CMP_PARAMETER_UINT("data-store-latency", _dataStoreLatency)
      CMP_PARAMETER_UINT("dbi-size", _dbiSize)
      
    CMP_PARAMETER_END
  }


  // -------------------------------------------------------------------------
  // Function to initialize statistics
  // -------------------------------------------------------------------------

  void InitializeStatistics() {

    INITIALIZE_COUNTER(accesses, "Total Accesses");
    INITIALIZE_COUNTER(reads, "Read Accesses");
    INITIALIZE_COUNTER(writebacks, "Writeback Accesses");
    INITIALIZE_COUNTER(misses, "Total Misses");
    INITIALIZE_COUNTER(evictions, "Evictions");
    INITIALIZE_COUNTER(dirty_evictions, "Dirty Evictions");
    INITIALIZE_COUNTER(dbievictions, "DBI Evictions");
    INITIALIZE_COUNTER(dbi_eviction_writebacks, "DBI Eviction Writebacks");
    INITIALIZE_COUNTER(tagstore_eviction_writebacks, "Tagstore Eviction Writebacks");
    INITIALIZE_COUNTER(dbi_misses, "DBI Accesses");
    INITIALIZE_COUNTER(dbi_hits, "DBI Hits");
  }


  // -------------------------------------------------------------------------
  // Function called when simulation starts
  // -------------------------------------------------------------------------

  void StartSimulation() {

    _numSets = (_size * 1024) / (_blockSize * _associativity);

     _numdbiEntries = _dbiSize;

    _tags.SetTagStoreParameters(_numSets, _associativity, _policy);
    _dbi.SetTagStoreParameters(1, _numdbiEntries, _dbipolicy);

    switch (_policyVal) {
    case 0: _pval = POLICY_HIGH; break;
    case 1: _pval = POLICY_BIMODAL; break;
    case 2: _pval = POLICY_LOW; break;
    }

    switch (_dbiPolicyVal) {
    case 0: _dbipval = POLICY_HIGH; break;
    case 1: _dbipval = POLICY_BIMODAL; break;
    case 2: _dbipval = POLICY_LOW; break;
    }
                           
    _hits.resize(_numCPUs, 0);
    _misses.resize(_numCPUs, 0);
  }


  // -------------------------------------------------------------------------
  // Function called at a heart beat. Argument indicates cycles elapsed after
  // previous heartbeat
  // -------------------------------------------------------------------------

  void HeartBeat(cycles_t hbCount) {
  }


protected:

  // -------------------------------------------------------------------------
  // Function to process a request. Return value indicates number of busy
  // cycles for the component.
  // -------------------------------------------------------------------------


  cycles_t ProcessRequest(MemoryRequest *request) {

    // update stats
    INCREMENT(accesses);

    table_t <addr_t, TagEntry>::entry tagentry;
    table_t <addr_t, DBIEntry>::entry dbientry;
    cycles_t latency;

    // NO WRITES (Complete or partial)
    if (request -> type == MemoryRequest::WRITE || 
        request -> type == MemoryRequest::PARTIALWRITE) {
      fprintf(stderr, "LLC annot handle direct writes (yet)\n");
      exit(0);
    }

    // compute the cache block tag
    addr_t ctag = VADDR(request) / _blockSize;

    // compute the cache block logical row, this is used to key into dbi, like ctag is used for tagstores
    addr_t logicalRow = ctag / BLOCKS_PER_ROW;	

    // check if its a read or write back
    switch (request -> type) {

      // READ request
    case MemoryRequest::READ:
    case MemoryRequest::READ_FOR_WRITE:
    case MemoryRequest::PREFETCH:

      INCREMENT(reads);
          
      tagentry = _tags.read(ctag);

      if (tagentry.valid) {
        request -> serviced = true;
        request -> AddLatency(_tagStoreLatency + _dataStoreLatency);

        // update per processor counters
        _hits[request -> cpuID] ++;
      }
      else {
        INCREMENT(misses);
        request -> AddLatency(_tagStoreLatency);

        _misses[request -> cpuID] ++;
      }
          
      return _tagStoreLatency;


      // WRITEBACK request
    case MemoryRequest::WRITEBACK:

      INCREMENT(writebacks);

      if (_tags.lookup(ctag))
        {

	// this one sets the dirty bit alongwith doing the replacement policy
        if(_dbi.lookup(logicalRow)){	_dbi[logicalRow].dirtyBits.set(ctag % BLOCKS_PER_ROW);	
	// Not all clean blocks are guaranteed to have their dirty bit info in the DBI
	INCREMENT(dbi_hits);
	// dummy read to update with replacement policy
        _dbi.read(logicalRow);
        }

	else{
        table_t <addr_t, DBIEntry>::entry dbientry;    

	HANDLE_DBI_INSERTION(ctag, request, dbientry, true);
// this will handle dbi insertion when tagstore entry is not present
        }
	        
      }

      else
        INSERT_BLOCK(ctag, true, request);	

      request -> serviced = true;
      return _tagStoreLatency;
    }
  }


  // -------------------------------------------------------------------------
  // Function to process the return of a request. Return value indicates
  // number of busy cycles for the component.
  // -------------------------------------------------------------------------
    
  cycles_t ProcessReturn(MemoryRequest *request) { 

    // if its a writeback from this component, delete it
    if (request -> iniType == MemoryRequest::COMPONENT &&
        request -> iniPtr == this) {
      request -> destroy = true;
      return 0;
    }

    // get the cache block tag
    addr_t ctag = VADDR(request) / _blockSize;

    // if the block is already present, return
    if (_tags.lookup(ctag))
      return 0;

    INSERT_BLOCK(ctag, false, request);

    return 0;
  }


  // -------------------------------------------------------------------------
  // Function to insert a block into the cache
  // Should check for DBI evictions also
  // -------------------------------------------------------------------------

  void INSERT_BLOCK(addr_t ctag, bool dirty, MemoryRequest *request) {

    table_t <addr_t, TagEntry>::entry tagentry;
    table_t <addr_t, DBIEntry>::entry dbientry;
    
    bool DBIevictedEntry;			// indicates if dbientry is evicted or was already present, true if evicted

    addr_t logicalRow = ctag / BLOCKS_PER_ROW;	

    DBIevictedEntry = !(_dbi.lookup(logicalRow));

/*
The following FSM has been implemented :
1. First insert into _dbi.
2. If returned dbientry was valid and evicted, generate writebacks for all the entries corresponding to dirty bits in the 
   evicted dbientry. (nothing to be done when dbientry is valid and already present) Also, delete the tagstore entries 
   corresponding to these dirty bits. 
3. Now insert into _tags.
4. If returned tagentry is valid and dirty (evicted or already present), then generate writeback. 
*/

// if returned entry is due to free list in table, then bool valid will be false

// How to differentiate between evicted and already present, use evictedEntry and lookup


if(dirty)	HANDLE_DBI_INSERTION(ctag, request, dbientry, DBIevictedEntry);
     

// 3. and 4.

tagentry = _tags.insert(ctag, TagEntry(), _pval);

    _tags[ctag].vcla = BLOCK_ADDRESS(VADDR(request), _blockSize);
    _tags[ctag].pcla = BLOCK_ADDRESS(PADDR(request), _blockSize);
    _tags[ctag].appID = request -> cpuID;		
    //_dbi.read(logicalRow).value.dirtyBits[ctag % BLOCKS_PER_ROW] = dirty; // already done above    

    if (tagentry.valid) {
    // this will let in entries that were evicted, tagentry is always evicted

      INCREMENT(evictions);		

     bool specialCase = false;
     if(dirty)	specialCase = (((dbientry.key == (tagentry.key) / BLOCKS_PER_ROW)&&(dbientry.value.dirtyBits[(tagentry.key) % BLOCKS_PER_ROW])))?true:false;


      if (((_dbi.lookup((tagentry.key) / BLOCKS_PER_ROW))&&(_dbi[(tagentry.key) / BLOCKS_PER_ROW].dirtyBits[(tagentry.key) % BLOCKS_PER_ROW])) || specialCase) { 
    // this will let in entries that were dirty, specialCase leads to extrememly rare dirty evictions
  
        INCREMENT(dirty_evictions);

        // We should check if the dbientry is still present in the DBI, only then clean the corresponding bit
	if(_dbi.lookup((tagentry.key) / BLOCKS_PER_ROW))	
        //_dbi.read(logicalRow).value.dirtyBits.reset(ctag % BLOCKS_PER_ROW);	// no need for replacement policy here also
	_dbi[(tagentry.key) / BLOCKS_PER_ROW].dirtyBits.reset((tagentry.key) % BLOCKS_PER_ROW);
        // This is a cleaning operation, clean entry in DBI if present

	// ********   also invalidate row if it was the last set bit
        if(!(_dbi[(tagentry.key) / BLOCKS_PER_ROW].dirtyBits.any()))	_dbi.invalidate((tagentry.key) / BLOCKS_PER_ROW);

        MemoryRequest *writeback =
          new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
                            MemoryRequest::WRITEBACK, request -> cmpID, 
                            tagentry.value.vcla, tagentry.value.pcla, _blockSize,
                            request -> currentCycle);
        // Generating writebacks when entry was already present and dirty, is it necessary (for memory consistency perhaps?)
	INCREMENT(tagstore_eviction_writebacks);
        writeback -> icount = request -> icount;
        writeback -> ip = request -> ip;
        SendToNextComponent(writeback);
      } 

    }
  }

void HANDLE_DBI_INSERTION(addr_t ctag, MemoryRequest *request, table_t <addr_t, DBIEntry>::entry &dbientry, bool DBIevictedEntry){
 
    addr_t logicalRow = ctag / BLOCKS_PER_ROW;	

// 1.
    dbientry = _dbi.insert(logicalRow, DBIEntry(), _dbipval);
    _dbi[logicalRow].dirtyBits.set(ctag % BLOCKS_PER_ROW);
    
    INCREMENT(dbi_misses);

// 2. 
    if(DBIevictedEntry && dbientry.valid){
    INCREMENT(dbievictions);    
    
    // generate writebacks for all dirty blocks in the row
      for(uint64 i=0;i<BLOCKS_PER_ROW;i++){
           
          if(dbientry.value.dirtyBits[i]){ 
          addr_t discardtag = (dbientry.key * BLOCKS_PER_ROW) + i;
       
	  // now check if the corresponding block is present in the tagstore
	  // if not, no need to generate writebacks
	  if(_tags.lookup(discardtag)){
          
	  // we have to generate writebacks and remove tagstore entries too
	  // if we do not remove the tagstore entries, we may have cases where tagentry is there but dbientry is not
          
	  table_t <addr_t, TagEntry>::entry discardentry;
	  discardentry = _tags.invalidate(discardtag);
          
          MemoryRequest *writeback =
          new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
                            MemoryRequest::WRITEBACK, request -> cmpID, 
                            discardentry.value.vcla, discardentry.value.pcla, _blockSize,
                            request -> currentCycle);
          
	  INCREMENT(dbi_eviction_writebacks);
          writeback -> icount = request -> icount;
          writeback -> ip = request -> ip;
          SendToNextComponent(writeback);
          }
        }
      }
    }
  }

};

#endif // __CMP_LLC_DBI_H__

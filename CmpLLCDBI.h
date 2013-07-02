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
    bitset <NUMBER_OF_ROWS> dirtyBits;
    // pointer to a clean request for this current row ?
    DBIEntry() { dirtyBits.reset(); 
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
  // Declare Counters
  // -------------------------------------------------------------------------

  NEW_COUNTER(accesses);
  NEW_COUNTER(reads);
  NEW_COUNTER(writebacks);
  NEW_COUNTER(misses);
  NEW_COUNTER(evictions);
  NEW_COUNTER(dirty_evictions);


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
  }


  // -------------------------------------------------------------------------
  // Function called when simulation starts
  // -------------------------------------------------------------------------

  void StartSimulation() {

    // compute number of sets
    _numSets = (_size * 1024) / (_blockSize * _associativity);

    // number of dbi entries
    // the default dbi storage size is the nbr of bits for dirty info in a normal cache
    // _numdbiEntries = (_size * 1024) / (_blockSize * BLOCKS_PER_ROW);

    _numdbiEntries = 1000000;			// enough space for all rows in a 8 GB memory

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
      dbientry = _dbi.read(logicalRow);

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
        //_tags[ctag].dirty = true;
	//_dbi.setDirty(ctag);
        _dbi[logicalRow].dirtyBits.set(ctag % BLOCKS_PER_ROW);			// set corr bit in appr entry
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
  // Should define a handler (replacement policy) for the case when DBI is full 
  // -------------------------------------------------------------------------

  void INSERT_BLOCK(addr_t ctag, bool dirty, MemoryRequest *request) {

    table_t <addr_t, TagEntry>::entry tagentry;
    table_t <addr_t, DBIEntry>::entry dbientry;

    addr_t logicalRow = ctag / BLOCKS_PER_ROW;

    // insert the block into the cache
    tagentry = _tags.insert(ctag, TagEntry(), _pval);
    _tags[ctag].vcla = BLOCK_ADDRESS(VADDR(request), _blockSize);
    _tags[ctag].pcla = BLOCK_ADDRESS(PADDR(request), _blockSize);
    //_tags[ctag].dirty = dirty;
    _dbi[logicalRow].dirtyBits[ctag % BLOCKS_PER_ROW] = dirty;
    //if(_dbi.testIfPresent(ctag))	cout << "This is a dbi hit for block " << ctag << endl;
    	
    _tags[ctag].appID = request -> cpuID;

    // if the evicted tag entry is valid
    if (tagentry.valid) {
      INCREMENT(evictions);

      if (_dbi[logicalRow].dirtyBits[ctag % BLOCKS_PER_ROW]) { 
        INCREMENT(dirty_evictions);


        _dbi[logicalRow].dirtyBits.reset(ctag % BLOCKS_PER_ROW);	
        // This is a cleaning operation, clean entry in DBI, this should be there (gives correct results)
        MemoryRequest *writeback =
          new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
                            MemoryRequest::WRITEBACK, request -> cmpID, 
                            tagentry.value.vcla, tagentry.value.pcla, _blockSize,
                            request -> currentCycle);


        writeback -> icount = request -> icount;
        writeback -> ip = request -> ip;
        SendToNextComponent(writeback);
      }
    }
  }

};

#endif // __CMP_LLC_DBI_H__

// -----------------------------------------------------------------------------
// File: CmpDCP.h
// Description:
//    Implements a last-level cache with Decouple Caching and
//    Prefetching and prefetch monitors
// -----------------------------------------------------------------------------

#ifndef __CMP_DCP_H__
#define __CMP_DCP_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "Types.h"
#include "GenericTagStore.h"

#include "VictimTagStore.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------

#define SET_DUEL_PRIME 443

// -----------------------------------------------------------------------------
// Class: CmpDCP
// Description:
//    Last-level cache with decoupled caching and prefetching.
//
// The changes to the baseline last-level cache are marked with a
// comment DCP CHANGE.
// -----------------------------------------------------------------------------

class CmpDCP : public MemoryComponent {

protected:

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------

  uint32 _size;
  uint32 _blockSize;
  uint32 _associativity;
  string _policy;

  uint32 _tagStoreLatency;
  uint32 _dataStoreLatency;

  // DCP parameters
  bool _prefetchRequestPromote;
  bool _reusePrediction;
  bool _demandReusePrediction;
  bool _accuracyPrediction;

  uint32 _accuracyTableSize; // same as the prefetch table size
  uint32 _prefetchDistance;
  uint32 _accuracyCounterMax;

  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------
 
  enum PrefetchState {
    NOT_PREFETCHED,
    PREFETCHED_UNUSED,
    PREFETCHED_USED,
    PREFETCHED_REUSED,
  };
  
  struct TagEntry {
    bool dirty;
    addr_t vcla;
    addr_t pcla;
    uint32 appID;

    // prefetch related info
    PrefetchState prefState;
    uint32 prefID;
    bool lowPriority;

    // miss counter information
    uint32 prefetchMiss;
    uint32 useMiss;
    
    // cycle information
    cycles_t prefetchCycle;
    cycles_t useCycle;
    
    TagEntry() {
      dirty = false;
      lowPriority = false;
      prefState = NOT_PREFETCHED;
    }
   };

  // tag store
  uint32 _numSets;
  generic_tagstore_t <addr_t, TagEntry> _tags;

  // D-EAF reuse predictor
  struct SetEntry {
    bool leader;
    bool eaf;
    SetEntry() { leader = false; }
  };
  evicted_address_filter_t _eaf;
  vector <SetEntry> _duelInfo;
  saturating_counter _psel;
  uint32 _pselThreshold;
  

  // accuracy predictor
  struct AccuracyEntry {
    saturating_counter counter;
    generic_tagstore_t <addr_t, bool> ipEAF;
  };

  vector <AccuracyEntry> _accuracyTable;

  vector <uint32> _missCounter;


  // -------------------------------------------------------------------------
  // Declare Counters
  // -------------------------------------------------------------------------

  NEW_COUNTER(accesses);
  NEW_COUNTER(reads);
  NEW_COUNTER(writebacks);
  NEW_COUNTER(misses);
  NEW_COUNTER(evictions);
  NEW_COUNTER(dirty_evictions);

  NEW_COUNTER(prefetches);
  NEW_COUNTER(prefetch_misses);

  NEW_COUNTER(predicted_accurate);
  NEW_COUNTER(accurate_predicted_inaccurate);
  NEW_COUNTER(inaccurate_predicted_accurate);
  
  NEW_COUNTER(unused_prefetches);
  NEW_COUNTER(used_prefetches);
  NEW_COUNTER(unreused_prefetches);
  NEW_COUNTER(reused_prefetches);

  NEW_COUNTER(prefetch_use_cycle);
  NEW_COUNTER(prefetch_use_miss);

  NEW_COUNTER(prefetch_lifetime_cycle);
  NEW_COUNTER(prefetch_lifetime_miss);

  NEW_COUNTER(eaf_hits);
  
public:

  // -------------------------------------------------------------------------
  // Constructor. It cannot take any arguments
  // -------------------------------------------------------------------------

  CmpDCP() {
    _size = 1024;
    _blockSize = 64;
    _associativity = 16;
    _tagStoreLatency = 6;
    _dataStoreLatency = 15;
    _policy = "lru";
    _prefetchRequestPromote = true;
    _reusePrediction = false;
    _demandReusePrediction = false;
    _accuracyPrediction = false;
    _accuracyTableSize = 16;
    _prefetchDistance = 24;
    _accuracyCounterMax = 16;
    _pselThreshold = 1024;
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
      CMP_PARAMETER_UINT("tag-store-latency", _tagStoreLatency)
      CMP_PARAMETER_UINT("data-store-latency", _dataStoreLatency)

      CMP_PARAMETER_BOOLEAN("prefetch-request-promote", _prefetchRequestPromote)
      CMP_PARAMETER_BOOLEAN("reuse-prediction", _reusePrediction)
      CMP_PARAMETER_BOOLEAN("demand-reuse-prediction", _demandReusePrediction)
      CMP_PARAMETER_BOOLEAN("accuracy-prediction", _accuracyPrediction)
      
      CMP_PARAMETER_UINT("accuracy-table-size", _accuracyTableSize)
      CMP_PARAMETER_UINT("prefetch-distance", _prefetchDistance)
      CMP_PARAMETER_UINT("accuracy-counter-max", _accuracyCounterMax)

    CMP_PARAMETER_END
  }


  // -------------------------------------------------------------------------
  // Function to initialize statistics
  // -------------------------------------------------------------------------

  void InitializeStatistics() {

    INITIALIZE_COUNTER(accesses, "Total Accesses")
    INITIALIZE_COUNTER(reads, "Read Accesses")
    INITIALIZE_COUNTER(writebacks, "Writeback Accesses")
    INITIALIZE_COUNTER(misses, "Total Misses")
    INITIALIZE_COUNTER(evictions, "Evictions")
    INITIALIZE_COUNTER(dirty_evictions, "Dirty Evictions")

    INITIALIZE_COUNTER(prefetches, "Total prefetches")
    INITIALIZE_COUNTER(prefetch_misses, "Prefetch misses")
      
    INITIALIZE_COUNTER(predicted_accurate, "Prefetches predicted to be accurate")
    INITIALIZE_COUNTER(accurate_predicted_inaccurate, "Incorrect accuracy predictions")
    INITIALIZE_COUNTER(inaccurate_predicted_accurate, "Incorrect accuracy predictions")

    INITIALIZE_COUNTER(unused_prefetches, "Unused prefetches")
    INITIALIZE_COUNTER(used_prefetches, "Used prefetches")
    INITIALIZE_COUNTER(unreused_prefetches, "Unreused prefetches")
    INITIALIZE_COUNTER(reused_prefetches, "Reused prefetches")

    INITIALIZE_COUNTER(prefetch_use_cycle, "Prefetch-to-use Cycles")
    INITIALIZE_COUNTER(prefetch_use_miss, "Prefetch-to-use Misses")

    INITIALIZE_COUNTER(prefetch_lifetime_cycle, "Prefetch-lifetime Cycles")
    INITIALIZE_COUNTER(prefetch_lifetime_miss, "Prefetch-lifetime Misses")

    INITIALIZE_COUNTER(eaf_hits, "EAF hits")
  }


  // -------------------------------------------------------------------------
  // Function called when simulation starts
  // -------------------------------------------------------------------------

  void StartSimulation() {

    // compute number of sets
    _numSets = (_size * 1024) / (_blockSize * _associativity);
    _tags.SetTagStoreParameters(_numSets, _associativity, _policy);
    _missCounter.resize(_numSets, 0);

    // initialize the reuse predictor
    if (_reusePrediction || _demandReusePrediction) {
      _eaf.initialize(_numSets * _associativity);
      _psel = saturating_counter(_pselThreshold, _pselThreshold / 2);
      // dueling sets
      _duelInfo.resize(_numSets);
      cyclic_pointer current(_numSets, 0);
      for (int i = 0; i < 32; i ++) {
        _duelInfo[current].leader = true;
        _duelInfo[current].eaf = true;
        current.add(SET_DUEL_PRIME);
        _duelInfo[current].leader = true;
        _duelInfo[current].eaf = false;
        current.add(SET_DUEL_PRIME);
      }
    }

    // check if an accuracy predictor is needed
    if (_accuracyPrediction) {
      _accuracyTable.resize(_accuracyTableSize);
      for (int i = 0; i < _accuracyTableSize; i ++) {
        _accuracyTable[i].counter.set_max(_accuracyCounterMax);
        _accuracyTable[i].ipEAF.SetTagStoreParameters(_prefetchDistance, 1, "fifo");
      }
    }
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

    cycles_t latency;

    // NO WRITES (Complete or partial)
    if (request -> type == MemoryRequest::WRITE || 
        request -> type == MemoryRequest::PARTIALWRITE) {
      fprintf(stderr, "LLC cannot handle direct writes (yet)\n");
      exit(0);
    }

    // compute the cache block tag
    addr_t ctag = VADDR(request) / _blockSize;
    uint32 index = _tags.index(ctag);

    // check if its a read or write back
    switch (request -> type) {

    // READ request
    case MemoryRequest::READ:
    case MemoryRequest::READ_FOR_WRITE:

      INCREMENT(reads);
          
      if (_tags.lookup(ctag)) {

        request -> serviced = true;
        request -> AddLatency(_tagStoreLatency + _dataStoreLatency);

        // DCP CHANGE: Update state only for demand-fetched or
        // already used blocks. For prefetched blocks, depromote
        // to low priority. Code moved into switch case.
        
        // read to update replacement policy
        // _tags.read(ctag);
        
        // read to update state
        TagEntry &tagentry = _tags[ctag];
        policy_value_t priority;
        
        // check the prefetched state
        switch (tagentry.prefState) {
        case PREFETCHED_UNUSED:
          // update state
          tagentry.prefState = PREFETCHED_USED;
          tagentry.useMiss = _missCounter[index];
          tagentry.useCycle = request -> currentCycle;

          // update accuracy
          if (_accuracyPrediction) {
            _accuracyTable[tagentry.prefID].counter.increment();
          }

          // DCP CHANGE: Depromote to low priority
          // DCP-RP CHANGE: Check if we need to use reuse predictor
          priority = POLICY_LOW;
          if (_reusePrediction) {
            policy_value_t eafPriority =
              _eaf.test(ctag) ? POLICY_HIGH : POLICY_BIMODAL;
            SetEntry sentry = _duelInfo[_tags.index(ctag)];
            if ((sentry.leader && sentry.eaf) || (_psel > _pselThreshold / 2))
              priority = eafPriority; 
            else
              priority = POLICY_HIGH;
          }
          _tags.read(ctag, priority);


          // update counters
          INCREMENT(used_prefetches);
          ADD_TO_COUNTER(prefetch_use_cycle,
                         tagentry.useCycle - tagentry.prefetchCycle);
          ADD_TO_COUNTER(prefetch_use_miss,
                         tagentry.useMiss - tagentry.prefetchMiss);
          break;
          
        case PREFETCHED_USED:
          _tags.read(ctag, POLICY_HIGH);
          tagentry.prefState = PREFETCHED_REUSED;
          INCREMENT(reused_prefetches);
          break;
          
        case NOT_PREFETCHED:
        case PREFETCHED_REUSED:
          _tags.read(ctag, POLICY_HIGH);
          // do nothing
          break;
        }
        
      }
      else {

        // if reuse prediction is used, update predictor
        if (_reusePrediction || _demandReusePrediction) {
          SetEntry sentry = _duelInfo[_tags.index(ctag)];
          if (sentry.leader) {
            if (sentry.eaf)
              _psel.decrement();
            else
              _psel.increment();
          }
        }

        // check ipEAF if necessary
        if (_accuracyPrediction) {
          if (request -> d_prefetched) {
            AccuracyEntry &accEntry = _accuracyTable[request -> d_prefID];
            if (accEntry.ipEAF.lookup(ctag)) {
              accEntry.ipEAF.invalidate(ctag);
              accEntry.counter.increment();
              INCREMENT(accurate_predicted_inaccurate);
            }
          }
        }
        
        INCREMENT(misses);
        request -> AddLatency(_tagStoreLatency);
        _missCounter[index] ++;
      }
          
      return _tagStoreLatency;

    case MemoryRequest::PREFETCH:

      INCREMENT(reads);
          
      if (_tags.lookup(ctag)) {
        request -> serviced = true;
        request -> AddLatency(_tagStoreLatency + _dataStoreLatency);

        // read to update replacement policy
        // if we shoule promote on a prefetch request hit?
        // else do nothing
        if (_prefetchRequestPromote)
          _tags.read(ctag, POLICY_HIGH);
      }
      else {
        INCREMENT(prefetch_misses);
        request -> AddLatency(_tagStoreLatency);
        _missCounter[index] ++;
      }
          
      return _tagStoreLatency;

      
    // WRITEBACK request
    case MemoryRequest::WRITEBACK:

      INCREMENT(writebacks);

      if (_tags.lookup(ctag))
        _tags[ctag].dirty = true;
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
  // -------------------------------------------------------------------------

  void INSERT_BLOCK(addr_t ctag, bool dirty, MemoryRequest *request) {

    table_t <addr_t, TagEntry>::entry tagentry;
    policy_value_t priority = POLICY_HIGH;

    // if there is demand reuse prediction
    if (_demandReusePrediction &&
        request -> type != MemoryRequest::PREFETCH) {
      policy_value_t eafPriority = _eaf.test(ctag) ? POLICY_HIGH : POLICY_BIMODAL;
      SetEntry sentry = _duelInfo[_tags.index(ctag)];
      if ((sentry.leader && sentry.eaf) || (_psel > _pselThreshold / 2)) {
        priority = eafPriority; 
      }
      else {
        priority = POLICY_HIGH;
      }
    }


    // if there is accuracy prediction
    if (_accuracyPrediction &&
        request -> type == MemoryRequest::PREFETCH) {
      if (_accuracyTable[request -> prefetcherID].counter >
          (_accuracyCounterMax / 2)) {
        priority = POLICY_HIGH;
        INCREMENT(predicted_accurate);
      }
      else {
        priority = POLICY_LOW;
      }
    }      

    // insert the block into the cache
    tagentry = _tags.insert(ctag, TagEntry(), priority);
    _tags[ctag].vcla = BLOCK_ADDRESS(VADDR(request), _blockSize);
    _tags[ctag].pcla = BLOCK_ADDRESS(PADDR(request), _blockSize);
    _tags[ctag].dirty = dirty;
    _tags[ctag].appID = request -> cpuID;
    _tags[ctag].prefState = NOT_PREFETCHED;

    uint32 index = _tags.index(ctag);

    // Handle prefetch
    if (request -> type == MemoryRequest::PREFETCH) {
      _tags[ctag].prefState = PREFETCHED_UNUSED;
      _tags[ctag].prefID = request -> prefetcherID;
      _tags[ctag].prefetchCycle = request -> currentCycle;
      _tags[ctag].prefetchMiss = _missCounter[index];
      if (priority == POLICY_LOW) {
        _tags[ctag].lowPriority = true;
      }

      INCREMENT(prefetches);
    }

    // if the evicted tag entry is valid
    if (tagentry.valid) {
      INCREMENT(evictions);

      // insert into eaf it its not an unused prefetch
      if (_reusePrediction) {
        if (tagentry.value.prefState != PREFETCHED_UNUSED)
          _eaf.insert(tagentry.key);
      }
      
      // check prefetched state
      switch (tagentry.value.prefState) {
      case PREFETCHED_UNUSED:
        INCREMENT(unused_prefetches);
        ADD_TO_COUNTER(prefetch_lifetime_cycle,
                       request -> currentCycle - tagentry.value.prefetchCycle);
        ADD_TO_COUNTER(prefetch_lifetime_miss,
                       _missCounter[index] - tagentry.value.prefetchMiss);

        // if accuracy prediction is enabled, update the accuracy table
        if (_accuracyPrediction) {
          AccuracyEntry &accEntry = _accuracyTable[tagentry.value.prefID];
          if (tagentry.value.lowPriority) {
            if (accEntry.ipEAF.insert(tagentry.key, true).valid)
              accEntry.counter.decrement();
          }
          else {
            accEntry.counter.decrement();
            INCREMENT(inaccurate_predicted_accurate);
          }
        }
        
        break;
        
      case PREFETCHED_USED:
        INCREMENT(unreused_prefetches);
        ADD_TO_COUNTER(prefetch_lifetime_cycle,
                       request -> currentCycle - tagentry.value.prefetchCycle);
        ADD_TO_COUNTER(prefetch_lifetime_miss,
                       _missCounter[index] - tagentry.value.prefetchMiss);
        break;
        
      case PREFETCHED_REUSED:
        ADD_TO_COUNTER(prefetch_lifetime_cycle,
                       tagentry.value.useCycle - tagentry.value.prefetchCycle);
        ADD_TO_COUNTER(prefetch_lifetime_miss,
                       tagentry.value.useMiss - tagentry.value.prefetchMiss);
        break;
        
      case NOT_PREFETCHED:
        // do nothing
        break;
      }

      if (tagentry.value.dirty) {
        INCREMENT(dirty_evictions);
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

#endif // __CMP_DCP_H__

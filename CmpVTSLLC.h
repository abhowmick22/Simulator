// -----------------------------------------------------------------------------
// File: CmpVTSLLC.h
// Description:
//    Implements a last-level cache
// -----------------------------------------------------------------------------

#ifndef __CMP_VTS_LLC_H__
#define __CMP_VTS_LLC_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "Types.h"
#include "GenericTagStore.h"
#include "VictimTags.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Class: CmpVTSLLC
// Description:
//    victim tag based implementation lastlevel cache.
// -----------------------------------------------------------------------------

class CmpVTSLLC : public MemoryComponent {

  protected:

    // -------------------------------------------------------------------------
    // Parameters
    // -------------------------------------------------------------------------

    uint32 _size;
    uint32 _blockSize;
    uint32 _associativity;

    uint32 _tagStoreLatency;
    uint32 _dataStoreLatency;

    uint32 _numHistorySets;
    uint32 _maxSetLength;

    uint32 _highSet;

    bool _allowBypass;
    bool _allowAlwaysHigh;
    bool _allowAlwaysLow;

    uint32 _highThreshold;
    uint32 _highThreshold2;
    uint32 _lowCap;
    bool _scale;

    double _movingAverage;

    double _alpha;

    // -------------------------------------------------------------------------
    // Private members
    // -------------------------------------------------------------------------

    struct TagEntry {
      bool valid;
      addr_t tag;
      bool dirty;
      addr_t vcla;
      addr_t pcla;
      uint32 appID;
      bool reused;
      saturating_counter replInfo;

      TagEntry():replInfo(7,0) { 
        valid = false; 
        dirty = false; 
        reused = false;
      }
    };

    // tag store
    uint32 _numSets;
    uint32 _numBlocks;
    vector <vector <TagEntry> > _tags;

    // policy related
    vector <bool> _alwaysHigh;
    vector <bool> _alwaysLow;
    vector <saturating_counter> _highCount;
    vector <saturating_counter> _highCount2;
    vector <cyclic_pointer> _setProb;


    // counters to keep track of occupancy
    vector <uint32> _occupancy;

    // victim tag store
    vector <VictimTagStore> _victims;
    vector <int32> _reusedBlocks;
    vector <uint32> _uselessBlocks;

    // per processor hit/miss counters
    vector <uint64> _hits;
    vector <uint64> _misses;

    // per processor victim hit/miss counters
    vector <vector <uint64> > _victimHits;
    vector <uint64> _victimMisses;
    

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
    // -------------------------------------------------------------------------

    CmpVTSLLC() {
      _size = 1024;
      _blockSize = 64;
      _associativity = 16;
      _tagStoreLatency = 6;
      _dataStoreLatency = 15;
      _highSet = 1;
      _allowBypass = false;
      _allowAlwaysHigh = false;
      _allowAlwaysLow = false;
      _highThreshold = 2;
      _highThreshold2 = 2;
      _lowCap = 1;
      _scale = false;
      _movingAverage = 0;
      _alpha = 0;
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

      CMP_PARAMETER_UINT("tag-store-latency", _tagStoreLatency)
      CMP_PARAMETER_UINT("data-store-latency", _dataStoreLatency)

      CMP_PARAMETER_UINT("num-history-sets", _numHistorySets)
      CMP_PARAMETER_UINT("max-set-length", _maxSetLength)

      CMP_PARAMETER_UINT("high-set", _highSet)

      CMP_PARAMETER_BOOLEAN("allow-bypass", _allowBypass)
      CMP_PARAMETER_BOOLEAN("allow-always-high", _allowAlwaysHigh)
      CMP_PARAMETER_BOOLEAN("allow-always-low", _allowAlwaysLow)
      CMP_PARAMETER_BOOLEAN("scale", _scale)

      CMP_PARAMETER_UINT("high-threshold", _highThreshold)
      CMP_PARAMETER_UINT("low-cap", _lowCap)

      CMP_PARAMETER_DOUBLE("moving-average", _movingAverage)
      CMP_PARAMETER_DOUBLE("alpha", _alpha)
 
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
    }


    // -------------------------------------------------------------------------
    // Function called when simulation starts
    // -------------------------------------------------------------------------

    void StartSimulation() {

      // compute number of sets
      _numSets = (_size * 1024) / (_blockSize * _associativity);
      _numBlocks = _numSets * _associativity;
      _tags.resize(_numSets, vector <TagEntry> (_associativity));
      _occupancy.resize(_numCPUs, 0);

      // set initial policy
      _setProb.resize(_numSets, cyclic_pointer(64,0));
      _alwaysHigh.resize(_numCPUs, false);
      _alwaysLow.resize(_numCPUs, false);
      _highCount.resize(_numCPUs, saturating_counter (_highThreshold, 
            _highThreshold));
      _highCount2.resize(_numCPUs, saturating_counter (_highThreshold2, 
            _highThreshold2));

      if (_numCPUs > 1) {
        // create the occupancy log file
        NEW_LOG_FILE("occupancy", "occupancy");
      }

      // victim tag store
      _victims.resize(_numCPUs, VictimTagStore(_numHistorySets, _maxSetLength));

      // for each processor create a victim log file
      for (uint32 i = 0; i < _numCPUs; i ++) {
        char fileName[25];
        sprintf(fileName, "victim-%u", i);
        NEW_LOG_FILE(fileName, fileName);
        sprintf(fileName, "info-%u", i);
        NEW_LOG_FILE(fileName, fileName);
        sprintf(fileName, "victim-hits-%u", i);
        NEW_LOG_FILE(fileName, fileName);
      }

      // initialize per processor counters
      _hits.resize(_numCPUs, 0);
      _misses.resize(_numCPUs, 0);
      _victimHits.resize(_numCPUs, vector <uint64> (_numHistorySets, 0));
      _victimMisses.resize(_numCPUs, 0);

      _reusedBlocks.resize(_numCPUs, 0);
      _uselessBlocks.resize(_numCPUs, 0);
    }


    // -------------------------------------------------------------------------
    // Function to compute the percentage value
    // -------------------------------------------------------------------------

    double PERCENT(uint64 value, uint64 total) {
      return (double)(value * 100) / (total + 1);
    }


    // -------------------------------------------------------------------------
    // Function called at a heart beat. Argument indicates cycles elapsed after
    // previous heartbeat
    // -------------------------------------------------------------------------

    void HeartBeat(cycles_t hbCount) {

      // check if always high policy is needed
      if (_allowAlwaysHigh) {
        for (uint32 i = 0; i < _numCPUs; i ++) {

          uint64 recentHits = _victimHits[i][0] + _victimHits[i][1];
          uint64 totalAccesses = _hits[i] + _misses[i];
          uint64 totalVTSHits = _misses[i] - _victimMisses[i];
          if ((PERCENT(recentHits, totalVTSHits) > 98) &&
              PERCENT(_victimMisses[i], totalAccesses) > 5) {
            _highCount[i].decrement();
          }
          else if (PERCENT(_hits[i], totalAccesses) < 5 ||
              totalVTSHits > _hits[i]) {
            _highCount[i].increment();
          }

          if ((recentHits * _occupancy[i]) > 
              ((1 + _alpha) * _hits[i] * _associativity * _numSets)) {
            _highCount2[i].decrement();
          }
          else if (PERCENT(_hits[i], totalAccesses) < 5 ||
              totalVTSHits > _hits[i]) {
            _highCount2[i].increment();
          }

          if (_highCount[i] == 0 || _highCount2[i] == 0) {
            _alwaysHigh[i] = true;
            _alwaysLow[i] = false;
          }
          else if (_highCount[i] == _highThreshold || 
              _highCount2[i] == _highThreshold2) {
            _alwaysHigh[i] = false;
          }

        }        
      }

      // check if always low policy is allowed
      if (_allowAlwaysLow) {

        for (uint32 i = 0; i < _numCPUs; i ++) {

          uint64 totalAccesses = _hits[i] + _misses[i];
          uint64 recentHits = _victimHits[i][0] + _victimHits[i][1];

          // exit condition
          if (_alwaysLow[i]) {
            if (recentHits * 100 > totalAccesses * _lowCap) {
              _alwaysLow[i] = false;
            }
          }

          // entry condition
          else {
            if ((recentHits + _hits[i]) * 100 < (totalAccesses * _lowCap)) {
              _alwaysLow[i] = true;
              _alwaysHigh[i] = false;
            }
          }

        }
      }

      for (uint32 i = 0; i < _numCPUs; i ++) {
        char filename[25];
        sprintf(filename, "info-%u", i);
        LOG(filename, "%u %u %u %u\n", (uint32)_alwaysHigh[i], (uint32)_alwaysLow[i],
            (uint32)(_highCount[i]), (uint32)(_highCount2[i]));
      }


      // if there are more than one apps, then print occupancy
      if (_numCPUs > 1) {
        LOG_W("occupancy", "%llu ", _currentCycle);
        for (uint32 i = 0; i < _numCPUs; i ++)
          LOG_W("occupancy", "%u ", _occupancy[i]);
        LOG_W("occupancy", "\n");
      }

      // log the victim information for each block and reset information
      for (uint32 i = 0; i < _numCPUs; i ++) {
        char victimFileName[15];
        sprintf(victimFileName, "victim-%u", i);

        // hb count
        LOG(victimFileName, "%llu ", hbCount);
        // hits
        LOG(victimFileName, "%llu ", _hits[i]);
        _hits[i] *= _movingAverage;
        // misses
        LOG(victimFileName, "%llu ", _misses[i]);
        _misses[i] *= _movingAverage;
        // victim Hits
        for (uint32 j = 0; j < _numHistorySets; j ++) {
          LOG(victimFileName, "%llu ", _victimHits[i][j]);
          _victimHits[i][j] *= _movingAverage;
        }

        LOG(victimFileName, "%llu\n", _victimMisses[i]);
        _victimMisses[i] *= _movingAverage;
        _uselessBlocks[i] = 0;
        _reusedBlocks[i] = 0;
      }

      // clear all reused bits in all the blocks
      for (uint32 i = 0; i < _numSets; i ++) {
        for (uint32 j = 0; j < _associativity; j ++) {
          _tags[i][j].reused = false;
        }
      }

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
      addr_t ctag = PADDR(request) / _blockSize;

      // check if its a read or write back
      switch (request -> type) {

        // READ request
        case MemoryRequest::READ:

          INCREMENT(reads);
          
          if (READ_BLOCK(ctag)) {
            request -> serviced = true;
            request -> AddLatency(_tagStoreLatency + _dataStoreLatency);

            char hitslogfile[20];
            sprintf(hitslogfile, "victim-hits-%u", request -> cpuID);
            LOG_W(hitslogfile, "0 %llx\n", ctag);

          }
          else {
            INCREMENT(misses);
            request -> AddLatency(_tagStoreLatency);

            // update per processor counter
            _misses[request -> cpuID] ++;

            // check if the block is a victim hit
            pair <uint32,uint32> victimSetID = 
              _victims[request -> cpuID].LookUp(ctag);

            request -> victimSetID = victimSetID.first;
            if (victimSetID.first < _numHistorySets) {
              _victimHits[request -> cpuID][victimSetID.first] ++;
            }
            else
              _victimMisses[request -> cpuID] ++;

            char hitslogfile[20];
            sprintf(hitslogfile, "victim-hits-%u", request -> cpuID);
            LOG_W(hitslogfile, "%u %llx\n", victimSetID.first + 1, ctag);

          }
          
          return _tagStoreLatency;


        // WRITEBACK request
        case MemoryRequest::WRITEBACK:

          INCREMENT(writebacks);

          // insert writebacks with the lowest priority
          request -> victimSetID = _numHistorySets;

          if (!MARK_DIRTY(ctag))
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
      addr_t ctag = PADDR(request) / _blockSize;

      // check if the block is already present
      if (LOOK_UP(ctag))
        return 0;

      INSERT_BLOCK(ctag, false, request);

      return 0;
    }


    // -------------------------------------------------------------------------
    // Index of a block in the tag store
    // -------------------------------------------------------------------------

    uint32 INDEX(addr_t ctag) {
      return ctag % _numSets;
    }


    // -------------------------------------------------------------------------
    // Look up a particular block address in the tag store
    // -------------------------------------------------------------------------
    
    bool LOOK_UP(addr_t ctag) {

      uint32 index = INDEX(ctag);

      // for each way
      for (uint32 i = 0; i < _associativity; i ++) {
        if (_tags[index][i].valid) {
          if (_tags[index][i].tag == ctag)
            return true;
        }
      }

      return false;
    }


    // -------------------------------------------------------------------------
    // Function to mark a block as dirty
    // -------------------------------------------------------------------------

    bool MARK_DIRTY(addr_t ctag) {

      uint32 index = INDEX(ctag);

      // for each way
      for (uint32 i = 0; i < _associativity; i ++) {
        if (_tags[index][i].valid) {
          if (_tags[index][i].tag == ctag) {
            _tags[index][i].dirty = true;
            return true;
          }
        }
      }

      return false;
    }


    // -------------------------------------------------------------------------
    // Function to read a block present in the cache
    // -------------------------------------------------------------------------
    
    bool READ_BLOCK(addr_t ctag) {

      uint32 index = INDEX(ctag);

      // for each way
      for (uint32 i = 0; i < _associativity; i ++) {
        if (_tags[index][i].valid) {
          if (_tags[index][i].tag == ctag) {
            _tags[index][i].replInfo.increment();
            if (!_tags[index][i].reused) {
              _reusedBlocks[_tags[index][i].appID] ++;
            }
            _tags[index][i].reused = true;
            _hits[_tags[index][i].appID] ++;
            return true;
          }
        }
      }

      return false;
    }
    

    // -------------------------------------------------------------------------
    // Function to insert a block into the cache
    // -------------------------------------------------------------------------

    void INSERT_BLOCK(addr_t ctag, bool dirty, MemoryRequest *request) {

      // get the index of the block
      uint32 index = INDEX(ctag);

      uint32 way;
      bool flag = true;
      bool bypassBlock = false;
      TagEntry replaced;
      bool setBimodalLow = false;

      // check if there is a free way to insert the block
      for (uint32 i = 0; i < _associativity; i ++) {
        if (_tags[index][i].valid == false) {
          way = i;
          flag = false;
          break;
        }
      }

      uint32 priority;

      // determine the priority of the block that is going to be inserted
      if (_alwaysHigh[request -> cpuID]) {
        priority = 4;
      }
      else if (request -> victimSetID < _highSet) {
        priority = 1;
      }
      else if (_alwaysLow[request -> cpuID]) {
        priority = 0;
      }
      else {
        if (_setProb[index] == 0) {
          priority = 1;
        }
        else {
          priority = 0;
          setBimodalLow = true;
        }

        _setProb[index].increment();
      }


      // if there is no invalid block check if we need to bypass
      // else find a replacement
      if (flag) {

        // bypass block if enabled and 0 priority
        if (_allowBypass && priority == 0) {
          bypassBlock = true;
        }
        
        // else find a replacement
        else {
          while (flag) {
            for (uint32 i = 0; i < _associativity; i ++) {
              if (_tags[index][i].replInfo == 0) {
                way = i;
                flag = false;
                break;
              }
            }

            if (flag) {
              for (uint32 i = 0; i < _associativity; i ++)
                _tags[index][i].replInfo.decrement();
            }
          }

          replaced = _tags[index][way];
          _tags[index][way].valid = false;
        }
      }

      // if the block is bypassed
      if (bypassBlock) {
        _victims[request -> cpuID].Insert(ctag, 0);
        INCREMENT(evictions);
        return;
      }


      assert(_tags[index][way].valid == false);

      _tags[index][way].valid = true;
      _tags[index][way].tag = ctag;
      _tags[index][way].dirty = dirty;
      _tags[index][way].appID = request -> cpuID;
      _tags[index][way].reused = false;
      _tags[index][way].vcla = BLOCK_ADDRESS(VADDR(request), _blockSize);
      _tags[index][way].pcla = BLOCK_ADDRESS(PADDR(request), _blockSize);
      _tags[index][way].replInfo.set(priority);

      // increment that apps occupancy
      _occupancy[request -> cpuID] ++;

      // if the evicted tag entry is valid
      if (replaced.valid) {

        // update counters
        _occupancy[replaced.appID] --;
        INCREMENT(evictions);

        // insert the evicted block into the victim tag store
        _victims[replaced.appID].Insert(replaced.tag, 0);
        if (replaced.reused) 
          _reusedBlocks[replaced.appID] --;
        else 
          _uselessBlocks[replaced.appID] ++;


        if (replaced.dirty) {
          INCREMENT(dirty_evictions);
          MemoryRequest *writeback = new MemoryRequest(
              MemoryRequest::COMPONENT, request -> cpuID, this,
              MemoryRequest::WRITEBACK, request -> cmpID, 
              replaced.vcla, replaced.pcla, _blockSize,
              request -> currentCycle);
          writeback -> icount = request -> icount;
          writeback -> ip = request -> ip;
          SendToNextComponent(writeback);
        }
      }

    }
};

#endif // __CMP_VTS_LLC_H__

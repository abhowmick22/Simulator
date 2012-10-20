// -----------------------------------------------------------------------------
// File: CmpStreamPrefetcher.h
// Description:
//
// -----------------------------------------------------------------------------

#ifndef __CMP_STREAM_PREFETCHER_H__
#define __CMP_STREAM_PREFETCHER_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "GenericTable.h"
#include "Types.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------

#include <cstdlib>

// -----------------------------------------------------------------------------
// Class: CmpStreamPrefetcher
// Description:
// This class implements a stream prefetcher. Similar to the IBM
// Power prefetchers. Imported primarily from the stream
// prefetcher in scarab/ringo
// -----------------------------------------------------------------------------

class CmpStreamPrefetcher : public MemoryComponent {

protected:

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------

  uint32 _blockSize;
  bool _prefetchOnWrite;
  
  uint32 _tableSize;
  string _tablePolicy;
  uint32 _numTrains;
  uint32 _trainDistance;
  uint32 _distance;
  uint32 _degree;


  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------

  enum StreamDirection {
    FORWARD = 1,
    BACKWARD = -1,
    NONE = 0
  };
  
  struct StreamEntry {
    // the miss address that allocated the stream entry and the
    // instruction pointer that caused the miss
    addr_t allocMissAddress;
    addr_t ip;

    // start and end pointers of the stream
    addr_t sp;
    addr_t ep;

    // is the prefetcher trained
    int trainHits;
    bool trained;
    StreamDirection direction;
  };
  
  // Prefetcher table
  generic_table_t <uint32, StreamEntry> _streamTable;

  // Running index, primarily to reuse the generic table
  // implementation
  uint32 _runningIndex;

  // Frequently used values
  addr_t _trainAddrDistance;
  addr_t _prefetchAddrDistance;
  
  // -------------------------------------------------------------------------
  // Declare Counters
  // -------------------------------------------------------------------------

  // No counters for now. May need later.

public:

  // -------------------------------------------------------------------------
  // Constructor. It cannot take any arguments
  // -------------------------------------------------------------------------

  CmpStreamPrefetcher() {
    _blockSize = 64;
    _prefetchOnWrite = false;

    _tableSize = 16;
    _tablePolicy = "lru";
    _trainDistance = 16;
    _numTrains = 2;
    _distance = 24;
    _degree = 4;
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
      CMP_PARAMETER_UINT("block-size", _blockSize)
      CMP_PARAMETER_BOOLEAN("prefetch-on-write", _prefetchOnWrite)

      CMP_PARAMETER_UINT("table-size", _tableSize)
      CMP_PARAMETER_STRING("table-policy", _tablePolicy)
      CMP_PARAMETER_UINT("train-distance", _trainDistance)
      CMP_PARAMETER_UINT("num-trains", _numTrains)
      CMP_PARAMETER_UINT("distance", _distance)
      CMP_PARAMETER_UINT("degree", _degree)

    CMP_PARAMETER_END
 }


  // -------------------------------------------------------------------------
  // Function to initialize statistics
  // -------------------------------------------------------------------------

  void InitializeStatistics() {
  }


  // -------------------------------------------------------------------------
  // Function called when simulation starts
  // -------------------------------------------------------------------------

  void StartSimulation() {
    _streamTable.SetTableParameters(_tableSize, _tablePolicy);
    _runningIndex = 0;

    _trainAddrDistance = _trainDistance * _blockSize;
    _prefetchAddrDistance = _distance * _blockSize;
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

    if (request -> type == MemoryRequest::WRITE ||
        request -> type == MemoryRequest::WRITEBACK ||
        request -> type == MemoryRequest::PREFETCH) {
      // do nothing
      return 0;
    }

    if (!_prefetchOnWrite &&
        (request -> type == MemoryRequest::READ_FOR_WRITE)) {
      // do nothing
      return 0;
    }

    addr_t vcla = VBLOCK_ADDRESS(request, _blockSize);
    addr_t pcla = PBLOCK_ADDRESS(request, _blockSize);
    
    table_t <uint32, StreamEntry>::entry row;

    bool hit = false;
    uint32 key;
    
    // Check if there is a stream entry matching the address
    for (uint32 i = 0; i < _tableSize; i ++) {

      // get row i from the stream table
      row = _streamTable.entry_at_index(i);

      // if row is invalid, continue
      if (!row.valid) continue;

      // get the stream entry information
      StreamEntry entry = row.value;

      // if entry is in the training phase
      if (!row.trained) {
        if (llabs(entry.allocMissAddress - vcla) < _trainAddrDistance) {
          // HIT! entry within training scope
          hit = true;
          key = row.key;
          break;
        }
      }

      // not training phase
      else {
        if (entry.sp <= vcla && entry.ep >= vcla) {
          // HIT! entry within monitor scope
          hit = true;
          key = row.key;
          break;
        }
      }
    }


    // If there is a stream entry, then update the entry based on
    // the current phase and issue prefetches if necessary
    if (hit) {
      // dummy read to update replacement state
      _streamTable.read(key);
      
      // real read to modify stream entry state
      StreamEntry &entry = _streamTable[key];

      // entry not trained yet
      if (!entry.trained) {
        // forward direction
        if (entry.allocMissAddress < vcla) {
          switch (entry.direction) {
          case FORWARD:
            // same direction.
            entry.trainHits ++;
            if (vcla > entry.ep) entry.ep = vcla;
            break;
          case BACKWARD:
          case NONE:
            // new direction
            entry.trainHits = 1;
            entry.direction = FORWARD;
            entry.ep = vcla;
            break;
          }
        }

        // backward direction
        else {
          switch (entry.direction) {
          case BACKWARD:
            // same direction.
            entry.trainHits ++;
            if (vcla < entry.ep) entry.ep = vcla;
            break;
          case FORWARD:
          case NONE:
            // new direction
            entry.trainHits = 1;
            entry.direction = BACKWARD;
            entry.ep = vcla;
            break;
          }
        }

        // Upgrade to trained?
        if (entry.trainHits >= _numTrains)
          entry.trained = true;
      }

      // entry trained
      if (entry.trained) {
        // Issue prefetches
      }
    }
    
    // If there is no stream entry, allocate a new stream entry
    else {
      // Create a new stream entry
      StreamEntry entry;
      entry.allocMissAddress = vcla;
      entry.ip = request -> ip;
      entry.sp = vcla;
      entry.ep = vcla;
      entry.trainHits = 0;
      entry.trained = false;
      entry.direction = NONE;
      _streamTable.insert(_runningIndex, entry);
      _runningIndex ++;
    }

    for (int i = 0; i < _degree; i ++) {
      vcla += _blockSize;
      pcla += _blockSize;
      MemoryRequest *prefetch =
        new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
                          MemoryRequest::PREFETCH, request -> cmpID, 
                          vcla, pcla, _blockSize, request -> currentCycle);
      prefetch -> icount = request -> icount;
      prefetch -> ip = request -> ip;
      SendToNextComponent(prefetch);
    }
    
    return 0; 
  }


  // -------------------------------------------------------------------------
  // Function to process the return of a request. Return value indicates
  // number of busy cycles for the component.
  // -------------------------------------------------------------------------
    
  cycles_t ProcessReturn(MemoryRequest *request) {

    // if its a prefetch from this component, delete it
    if (request -> iniType == MemoryRequest::COMPONENT &&
        request -> iniPtr == this) {
      request -> destroy = true;
    }
    
    return 0; 
  }

};

#endif // __CMP_STREAM_PREFETCHER_H__

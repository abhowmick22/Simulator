// -----------------------------------------------------------------------------
// File: CmpStridePrefetcher.h
// Description:
//
// -----------------------------------------------------------------------------

#ifndef __CMP_STRIDE_PREFETCHER_H__
#define __CMP_STRIDE_PREFETCHER_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "Types.h"
#include "GenericTable.h"


// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Class: CmpStridePrefetcher
// Description:
// This class implements a simple stride prefetcher. The number
// of strides to prefetch can be configured
// -----------------------------------------------------------------------------
using namespace std;

class CmpStridePrefetcher : public MemoryComponent {

protected:

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------

  uint32 _degree;
  uint32 _blockSize;
  bool _prefetchOnWrite;

  uint32 _tableSize;
  string _tablePolicy;
  uint32 _numTrains;
  uint32 _trainDistance;
  uin32 _distance;
  
  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------

  // Stride table entry
  
  // last seen address
  // last stride
  // trained?
  
  struct StrideEntry{

	//last recorded request address
  addr_t vaddr;
  addr_t paddr;

	//last sent prefetch
	addr_t vpref;
	addr_t ppref;

	//stride length
	int stride;

	//number of strides ahead
	int stridesAhead;

	//training state
	int trainHits;
	bool trained;
    
  };

  generic_table_t<addr_t, StrideEntry> _strideTable;

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

  CmpNextLinePrefetcher() {
    _degree = 4;
    _prefetchOnWrite = false;
    _blockSize = 64;
		
		_tableSize = 16;
		_tablePolicy = "lru";

		_numTrains = 2;
		_distance = 24;
		_trainDistance = 16;
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
    CMP_PARAMETER_UINT("degree", _degree)
    CMP_PARAMETER_UINT("block-size", _blockSize)
		CMP_PARAMETER_BOOLEAN("prefetch-on-write", _prefetchOnWrite)	  
	  CMP_PARAMETER_UINT("table-size", _tableSize)
	  CMP_PARAMETER_STRING("table_policy", _tablePolicy)
	  CMP_PARAMETER_UINT("train-distance", _trainDistance)
	  CMP_PARAMETER_UINT("num-trains", _numTrains)
	  CMP_PARAMETER_UINT("distance", _distance)

	  //TODO: add parameter for size of stride buffer
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
	  _strideTable.SetTableParameters(_tableSize, _tablePolicy);

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

		addr_t vcla = VBLOCK_ADDRESS(request, __blockSize);
	  addr_t pcla = PBLOCK_ADDRESS(request, __blockSize);

		//if no IP entry found, add vcla and pcla, return.
		if(stride_table.get(request->ip) == NULL)
		{
			StrideEntry newEntry;
	  	newEntry.vaddr = VBLOCK_ADDRESS(request, _blockSize); 
	  	newEntry.paddr = PBLOCK_ADDRESS(request, _blockSize);
		  newEntry.trained = false;
		  newEntry.trainHits = 0;
			newEntry.stride = 0;

			//insert the new entry into the table
		  _strideTable.insert(request->ip, newEntry);
	
	    //no information about the stream, therefore no prefetch
		  return 0;
		}
		
		//dummy read to update replacement state
		_strideTable.read(request->ip);

		//actual read entry
		StrideEntry &entry = _strideTable[request->ip];
		
		//delta in virtual and physical blocks 
		v_delta = vcla - entry.vaddr;
		p_delta = pcla - entry.paddr;
		
		//if IP entry found, but not trained OR stride not accurate
		if(entry.trained == false || v_delta != entry.stride )  {
	
			//if current stride is the same as the last stride, 
			//increment the trainhits
			if(v_delta == entry.stride) {
				entry.trainHits++;
				entry.trained == false;
			}
	
			//if the stride is different from the last stride, save current stride
			//and reset the trainHits to 0;
			else {
				entry.stride = v_delta;
				entry.trainHits = 0;
				entry.trained == false;
			}

			//update the "last seen" pointer
			entry.vaddr = vcla;
			entry.paddr = pcla;

			//update the "last pref" pointer - no prefs yet, but keep current
			entry.vpref = vcla;
			entry.ppref = pcla;

			//if pattern seen numTrains many times, time to set the trained bit
			if(entry.trainHits >= _numTrains) {
				entry.trained = true;
			}
		}
	
	
		//If IP entry found, is trained, AND current stride is consistent:
		//issue degree-many prefetches that are "stride" blocks away
		if(entry.trained == true) {
		  
			//if stride is 0, no point prefetching...
		  //TODO: think about this case more carefully. maybe if a block gets
		  //evicted from L1 we should remove it from the hashtable?
		  if(v_delta == 0 || p_delta == 0) {
				return 0;
		 	}

			//update the "last seen" pointer
			entry.vaddr = vcla;
			entry.paddr = pcla;
	
		  //figure out how many prefetches to send
			addr_t maxAddress = entry.vaddr + (_prefetchAddrDistance + _blockSize);
			int maxPrefetches = (maxAddress - entry.vpref)/_blockSize;
			int numPrefetches = (maxPrefetches > _degree) ? _degree : maxPrefetches;

			//if stride is > 0, issue prefetches for the next degree-many
		  //cache blocks, each a stride distance apart, upto a max distance away
   	  for (int i = 0; i < numPrefetches; i ++) {

				//virtual and physical addresses of next prefetch
        entry.vpref += _blockSize * v_delta;
        entry.ppref += _blockSize * p_delta;

				//a generated memory request
        MemoryRequest *prefetch =
          new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
				  MemoryRequest::PREFETCH, request -> cmpID, 
				  entry.vpref, entry.ppref, _blockSize, request -> currentCycle);
        prefetch -> icount = request -> icount;
        prefetch -> ip = request -> ip;

				//send the prefetch request downstream
        SendToNextComponent(prefetch);
      }
		
		  return 0; 
		}
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

#endif // __CMP_NEXT_LINE_PREFETCHER_H__

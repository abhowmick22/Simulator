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
#include <map>

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
  
  
  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------

  // Stride table entry
  
  // last seen address
  // last stride
  // trained?
  
  typedef struct blk_addr_t {
    addr_t vaddr;
    addr_t paddr;
    
  } blk_addr_t;

  map<addr_t, blk_addr_t> stride_table;

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

    // Prefetch the next "degree" cachelines
    // TODO: take care of across page prefetching!
	addr_t vcla;
	addr_t pcla;
	addr_t v_delta;
	addr_t p_delta;

	//if no IP entry found, add vlcla and pcla, return.
	if(stride_table.count(request->ip) == 0)
	{
	  stride_table[request->ip].vaddr = VBLOCK_ADDRESS(request, _blockSize); 
	  stride_table[request->ip].paddr = PBLOCK_ADDRESS(request, _blockSize);
      //no information about the stream, therefore no prefetch
	  return 0;
	}

	//If IP entry found, get the "stride" between current miss and last miss
	//issue degree-many prefetches that are "stride" blocks away
	else {
	  //load addresses of current prefetch
	  vcla = VBLOCK_ADDRESS(request, __blockSize);
	  pcla = VBLOCK_ADDRESS(request, __blockSize);

	  //delta in virtual and physical blocks 
	  v_delta = vcla - stride_table[request->ip].vaddr;
	  p_delta = pcla - stride_table[request->ip].paddr;

	  //if stride is 0, no point prefetching...
	  //however, if stride is 0, that means the block is no longer in the 
	  //L1 cache. maybe just make a mem request for the block?
	  //(both delta values are the same because cache block size is the same)
	  //TODO: think about this case more carefully. maybe if a block gets
	  //evicted from L1 we should remove it from the hashtable?
	  if(v_delta == 0 || p_delta == 0) {
		 MemoryRequest *prefetch =
          new MemoryRequest(MemoryRequest::COMPONENT, request -> cpuID, this,
				  MemoryRequest::PREFETCH, request -> cmpID, 
				  vcla, pcla, _blockSize, request -> currentCycle);
        prefetch -> icount = request -> icount;
        prefetch -> ip = request -> ip;
        SendToNextComponent(prefetch);
		return 0;
	  }

	  //if stride is > 0, issue prefetches for the next degree-many
	  //cache blocks, each a stride distance apart
   	  for (int i = 0; i < _degree; i ++) {
        vcla += _blockSize * v_delta;
        pcla += _blockSize * p_delta;
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

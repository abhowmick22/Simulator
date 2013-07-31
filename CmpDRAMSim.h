// TODO : Check all interactions with queue, check the timing of _currentCycle
// Check if mem will accept transactions
// Check for boundary cases in the new additions

// -----------------------------------------------------------------------------
// File: CmpDRAMSim.h
// Description:
//    Defines an interface to the detailed DRAMSim model. 
// (policies on when to expose write buffer writes to channel etc)
// -----------------------------------------------------------------------------


// Important info of the dram device
// DDR2_micron_16M_8b_x8_sg3E
//NUM_RANKS=1
//NUM_BANKS=8
//NUM_ROWS=16384 
//NUM_COLS=1024
//DEVICE_WIDTH=8
//tCK=3.0
//
// Thus for us, _numBanks= 8
// _rowSize = DEVICE_WIDTH*NUM_COLS = 8192
// ---------------------------------------------------------

#ifndef __CMP_DRAMSIM_H__
#define __CMP_DRAMSIM_H__

// the transaction size, or burst length, 32 bytes for DDR2, and 64 bytes for DDR3
#define TRANS_SIZE 32
#define ALIGN_ADDRESS(addr, bytes) (addr & ~(((unsigned long)bytes) - 1L))
#define tCK 3.0

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "MemoryComponent.h"
#include "Types.h"
#include <DRAMSim.h>

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------
#include <string>
#include <stdint.h>
#include <list>
#include <iostream>
#include <fstream>

using namespace DRAMSim;

// -----------------------------------------------------------------------------
// Class: CmpDRAMSim
// Description:
//    Detailed DRAM memory system model, fully configurable
//    Implements FCFS with drain-when-full.
// -----------------------------------------------------------------------------

class CmpDRAMSim : public MemoryComponent {

protected:

  // -------------------------------------------------------------------------
  // Parameters
  // -------------------------------------------------------------------------

// These need to be made equal to the DRAM device file
  uint32 _numBanks;
  uint32 _rowSize;

  string _schedAlgo;

  /*
  uint32 _rowHitLatency;
  uint32 _rowConflictLatency;
  uint32 _readToWriteLatency;
  uint32 _writeToReadLatency;
  uint32 _channelDelay;
  */

  uint32 _numWriteBufferEntries;
  uint32 _busProcessorRatio;
  
  uint32 _dummy;

  // -------------------------------------------------------------------------
  // Private members
  // -------------------------------------------------------------------------

  // memory request queue
  list <MemoryRequest *> _readQ;
  list <MemoryRequest *> _writeQ;

  // last operation type
  MemoryRequest::Type _lastOp;

  // open row in each bank
  vector <addr_t> _openRow;

  // scheduling algorithm
  MemoryRequest * (CmpDRAMSim::*NextRequest)();

  // for scheduling algorithms
  bool _drain;
  list <MemoryRequest *> _writeRowHits;
  list <MemoryRequest *> _readRowHits;

  // for requests
  uint64_t physicalAddress;

  // DRAM time
  cycles_t DRAMtime;

  // output file
  ofstream OutFile;

  // number of requests pending
  unsigned pendingRequests;


  // -------------------------------------------------------------------------
  // Declare counters
  // -------------------------------------------------------------------------

  NEW_COUNTER(accesses);
  NEW_COUNTER(reads);
  NEW_COUNTER(writes);

  
  NEW_COUNTER(Writerowhits);
  NEW_COUNTER(Readrowhits);
  NEW_COUNTER(rowconflicts);
/*
  NEW_COUNTER(readtowrites);
  NEW_COUNTER(writetoreads);
  

NEW_COUNTER(readandprefetch_cycles);
NEW_COUNTER(writeback_cycles);
*/
public:

  // -------------------------------------------------------------------------
  // Constructor. It cannot take any arguments
  // -------------------------------------------------------------------------

  CmpDRAMSim() {

    _numBanks = 8;
    _rowSize = 8192;				// default to the DDR2_micron_16M_8b_x8_sg3E    
    _numWriteBufferEntries = 64;
    _busProcessorRatio = 8;
    _schedAlgo = "fcfs";
  }


  // -------------------------------------------------------------------------
  // Virtual functions to be implemented by the components
  // -------------------------------------------------------------------------

  // DRAM component
  MultiChannelMemorySystem *mem ;
						

  // ------------------------------------------------------------------------
  // Callback functions for DRAMSim
  // -------------------------------------------------------------------------


	void read_complete(unsigned id, uint64_t address, uint64_t clock_cycle)
	{
    // printf("[Callback] read complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
    // callback function will set the corresponding request as serviced, add latency to request's latency, send the 
    // request to 
    // its next component, also update the _currentCycle of the simulator
    // For this request needs to have an id, which will determine which entry to free when callback fires, we use addr and order 
    // of the _queue vector for this

	//OutFile << endl << "read callback fired for request of addr : " << address << " at DRAM time : " << clock_cycle*_busProcessorRatio << endl;
	//OutFile << " SIM time : " << *_simulatorCycle << endl;
	//OutFile << " DRAM time : " << DRAMtime << endl;


	vector <MemoryRequest *> temp; 
        MemoryRequest *tempReq;		// All because I can't iterate over a priority_queue
	unsigned sizequeue = _queue.size();
	addr_t myaddr;
	myaddr = address;

                for (unsigned i=0;i<sizequeue;i++){
		if(!((myaddr==ALIGN_ADDRESS(_queue.top() -> virtualAddress, TRANS_SIZE))&&(_queue.top()->s_f_d)))
		{
		tempReq = _queue.top();
                _queue.pop();	
		temp.push_back(tempReq);
		}

		else{
			tempReq = _queue.top();
                	_queue.pop();
			unsigned sizetemp = temp.size();
			for(unsigned i=0;i<sizetemp;i++)
			{
			_queue.push(temp.back());
			temp.pop_back();
			}

			tempReq -> serviced = true;
			tempReq -> s_f_d = false;
			//OutFile << "the request was sent to dramsim at " << tempReq -> dramIssueCycle << " with an address " << ALIGN_ADDRESS(tempReq -> virtualAddress, TRANS_SIZE) << endl;
			//OutFile << "The latency of this request is " << (clock_cycle*_busProcessorRatio) - (tempReq -> dramIssueCycle) << endl;

			// this is a placeholder
			// _currentCycle is not really critical
		      
			cycles_t now = max((cycles_t)(clock_cycle*_busProcessorRatio), _currentCycle);
			_currentCycle = now;

			pendingRequests--;
			//OutFile << "number of pending requests is " << pendingRequests << endl << endl;

			tempReq -> AddLatency((clock_cycle*_busProcessorRatio) - (tempReq -> currentCycle));
                        //SendToNextComponent(tempReq);
			// alternative is, do all the tasks of SendToNextComponent except processpendingrequests in AddRequest
			tempReq -> cmpID --;
			((*_hier)[tempReq -> cpuID])[tempReq -> cmpID] -> SimpleAddRequest(tempReq);
			return;
                	}
                }
		OutFile << "DAFUQ Happened ? read cb\n";

	}

	void write_complete(unsigned id, uint64_t address, uint64_t clock_cycle)
	{
    // printf("[Callback] write complete: %d 0x%lx cycle=%lu\n", id, address, clock_cycle);
    // callback function will set the corresponding request as serviced, add latency to request's latency, send the 
    // request to 
    // its next component, free the entry in pending request list, also update the _currentCycle of the simulator
    // For this request needs to have an id, which will determine which entry to free when callback fires, we use addr and order 
    // of the pendingRequests list for this
	//OutFile << endl << "write callback fired for request of addr : " << address << " at DRAM time : " << clock_cycle*_busProcessorRatio << endl;
	//OutFile << " SIM time : " << *_simulatorCycle << endl;
	//OutFile << " DRAM time : " << DRAMtime << endl << endl;


	vector <MemoryRequest *> temp; 
        MemoryRequest *tempReq;		// All because I can't iterate over a priority_queue
	unsigned sizequeue = _queue.size();
	addr_t myaddr;
	myaddr = address;

                for (unsigned i=0;i<sizequeue;i++){
		if(!((myaddr==ALIGN_ADDRESS(_queue.top() -> virtualAddress, TRANS_SIZE))&&(_queue.top()->s_f_d)))
		{
		tempReq = _queue.top();
                _queue.pop();	
		temp.push_back(tempReq);
		}

		else{
			tempReq = _queue.top();
                	_queue.pop();
			unsigned sizetemp = temp.size();
			for(unsigned i=0;i<sizetemp;i++)
			{
			_queue.push(temp.back());
			temp.pop_back();
			}

			tempReq -> serviced = true;
			tempReq -> s_f_d = false;
			//OutFile << "the request was sent to dramsim at " << tempReq -> dramIssueCycle << " with an address " << ALIGN_ADDRESS(tempReq -> virtualAddress, TRANS_SIZE) << endl;
			//OutFile << "The latency of this request is " << (clock_cycle*_busProcessorRatio) - (tempReq -> dramIssueCycle) << endl;

			// this is a placeholder
			// _currentCycle is not really critical
		      
			cycles_t now = max((cycles_t)(clock_cycle*_busProcessorRatio), _currentCycle);
			_currentCycle = now;

			pendingRequests--;
			//OutFile << "number of pending requests is " << pendingRequests << endl << endl;

			tempReq -> AddLatency((clock_cycle*_busProcessorRatio) - (tempReq -> currentCycle));
                        //SendToNextComponent(tempReq);
			tempReq -> cmpID --;
			((*_hier)[tempReq -> cpuID])[tempReq -> cmpID] -> SimpleAddRequest(tempReq);
			return;
                	}
                }
		//OutFile << "DAFUQ Happened ? read cb\n";


	}

	/* This currently does nothing */
	void power_callback(double a, double b, double c, double d)
	{
	//	printf("power callback: %0.3f, %0.3f, %0.3f, %0.3f\n",a,b,c,d);
	}




  // -------------------------------------------------------------------------
  // Function to add a parameter to the component
  // -------------------------------------------------------------------------

  void AddParameter(string pname, string pvalue) {
      
    CMP_PARAMETER_BEGIN

      // Add the list of parameters to the component here
      CMP_PARAMETER_UINT("num-banks", _numBanks)
      CMP_PARAMETER_UINT("row-size", _rowSize)
      CMP_PARAMETER_UINT("num-write-buffer-entries", _numWriteBufferEntries)
      CMP_PARAMETER_STRING("scheduling-algo", _schedAlgo)
      CMP_PARAMETER_UINT("bus-processor-ratio", _busProcessorRatio)

/*
      CMP_PARAMETER_UINT("row-hit-latency", _rowHitLatency)
      CMP_PARAMETER_UINT("row-conflict-latency", _rowConflictLatency)
      CMP_PARAMETER_UINT("read-to-write-latency", _readToWriteLatency)
      CMP_PARAMETER_UINT("write-to-read-latency", _readToWriteLatency)
        
      CMP_PARAMETER_UINT("channel-delay", _channelDelay)
*/

      CMP_PARAMETER_UINT("stall-count", _dummy)
      CMP_PARAMETER_UINT("cmp-stall-count", _dummy)

      CMP_PARAMETER_END
      }


  // -------------------------------------------------------------------------
  // Function to initialize statistics
  // -------------------------------------------------------------------------

  void InitializeStatistics() {

    INITIALIZE_COUNTER(accesses, "Total Accesses");
    INITIALIZE_COUNTER(reads, "Read Accesses");
    INITIALIZE_COUNTER(writes, "Write Accesses");


    INITIALIZE_COUNTER(Writerowhits, "Write Row Buffer Hits");
    INITIALIZE_COUNTER(Readrowhits, "Read Row Buffer Hits");
    INITIALIZE_COUNTER(rowconflicts, "Row Buffer Conflicts");
/*
    INITIALIZE_COUNTER(readtowrites, "Read to Write Switches");
    INITIALIZE_COUNTER(writetoreads, "Write to Read Switches");

  INITIALIZE_COUNTER(readandprefetch_cycles, "Number of cycles for Read requests")
  INITIALIZE_COUNTER(writeback_cycles, "Number of cycles for writeback requests")
*/
  }


  // -------------------------------------------------------------------------
  // Function called when simulation starts
  // -------------------------------------------------------------------------

  void StartSimulation() {
    _openRow.resize(_numBanks, 0);
    NextRequest = GetSchedulingAlgorithmFunction(_schedAlgo);
    _drain = false;
    _lastOp = MemoryRequest::READ;
    pendingRequests = 0;

/*
    _rowHitLatency *= _busProcessorRatio;
    _rowConflictLatency *= _busProcessorRatio;
    _readToWriteLatency *= _busProcessorRatio;
    _writeToReadLatency *= _busProcessorRatio;
    _channelDelay *= _busProcessorRatio;
*/
	mem = getMemorySystemInstance("ini/DDR2_micron_16M_8b_x8_sg3E.ini", "system.ini", "/home/abhishek/DRAMSim2", "MyResults", 1024);
	uint64_t procSpeed = 2666666667;
	//8 * 1000000000 / tCK;	// Here 8 is the busprocessorratio
        mem->setCPUClockSpeed(procSpeed);
	DRAMtime = *_simulatorCycle;

	typedef DRAMSim::Callback <CmpDRAMSim, void, uint, uint64_t, uint64_t> dramsim_callback_t;
	TransactionCompleteCB *read_cb = new dramsim_callback_t(this, &CmpDRAMSim::read_complete);
	TransactionCompleteCB *write_cb = new dramsim_callback_t(this, &CmpDRAMSim::write_complete);
 
	mem->RegisterCallbacks(read_cb, write_cb, NULL);
	OutFile.open("output");
  }


  // -------------------------------------------------------------------------
  // Function called at a heart beat. Argument indicates cycles elapsed after
  // previous heartbeat
  // -------------------------------------------------------------------------
    
  void HeartBeat(cycles_t hbCount) {
  }


  void EndSimulation() {
    DUMP_STATISTICS;
    CLOSE_ALL_LOGS;
    OutFile.close();
  }


protected:

  // -------------------------------------------------------------------------
  // Function to get the scheduling algorithm function
  // -------------------------------------------------------------------------

  MemoryRequest * (CmpDRAMSim::*GetSchedulingAlgorithmFunction(
                                                                        string algo)) () {
    if (algo.compare("fcfs")) return &CmpDRAMSim::FCFS;
  }


  // -------------------------------------------------------------------------
  // Function to process a request. Return value indicates number of busy
  // cycles for the component.
  // -------------------------------------------------------------------------

  cycles_t ProcessRequest(MemoryRequest *request) {

    INCREMENT(accesses);
    bool isWrite;
    uint64_t addr;

    switch (request -> type) {
  
    case MemoryRequest::READ: case MemoryRequest::READ_FOR_WRITE: case MemoryRequest::PREFETCH:
     INCREMENT(reads);
     addr = ALIGN_ADDRESS(request -> virtualAddress, TRANS_SIZE);
     isWrite = false;
     break;

    case MemoryRequest::WRITEBACK:
     INCREMENT(writes);
     addr = ALIGN_ADDRESS(request -> virtualAddress, TRANS_SIZE);
     isWrite = true;
     break;
 
    case MemoryRequest::WRITE:
    case MemoryRequest::PARTIALWRITE:
      fprintf(stderr, "Memory controller cannot get a write\n");
      exit(0);    
    }

    // Get the row address of the request
    addr_t logicalRow = (request -> virtualAddress) / _rowSize;
    uint32 bankIndex = logicalRow % _numBanks;
    uint32 rowID = logicalRow / _numBanks;

    // check if the access is a row hit or conflict, to satisfy the scheduler
    if (_openRow[bankIndex] == rowID) {
      if((request -> type == MemoryRequest::READ)||(request -> type == MemoryRequest::READ_FOR_WRITE)||(request -> type == MemoryRequest::PREFETCH)){ INCREMENT(Readrowhits);}
      
      else INCREMENT(Writerowhits);
    }

    else {
      INCREMENT(rowconflicts);
      _openRow[bankIndex] = rowID;
    }


    // Can't really measure the turnarounds   

    bool accepted = mem->addTransaction(isWrite, addr);
    // request -> currentCycle is already set to _currentCycle of component
    if(accepted){request -> s_f_d = true;
    if(addr == 139779289939584) cout << "sent this at cycle " << *_simulatorCycle<<endl;
    pendingRequests++;
    request -> dramIssueCycle = request -> currentCycle;
    //OutFile << "number of pending requests is " << pendingRequests << endl;
    //OutFile << "Transaction with addr : " << addr << " and type " << request->type << " added to dramsim at sim time : " << *_simulatorCycle << " and dram time : " << DRAMtime << endl;
    //OutFile << " The currentCycle of the transaction added is " << request -> dramIssueCycle << endl;
    //OutFile << "The icount of the transaction added is " << request -> icount << " and its issue cycle is "<< request -> issueCycle << endl << endl;
	}
    else OutFile << "DRAMSim rejection occured " << endl;
    this -> AddRequest(request);		// add accepted or non-accepted request to queue of Memory Controller

    	// Do we really need this ?
    /*
    cycles_t prevDRAMtime = DRAMtime;
    for(cycles_t i=0;i<((*_simulatorCycle)-prevDRAMtime);i++){
    mem->update();						// update the DRAM time
    DRAMtime++;
    }
    */

    return 0;
      
    
    

/*
    cycles_t latency = 0;
    cycles_t turnAround = 0;

    // determine if there is a switch penalty
    switch (request -> type) {

    case MemoryRequest::READ: case MemoryRequest::READ_FOR_WRITE: case MemoryRequest::PREFETCH:
      INCREMENT(reads);
      if (_lastOp == MemoryRequest::WRITEBACK) {
        INCREMENT(writetoreads);
        latency += _writeToReadLatency;
        turnAround = _writeToReadLatency;
      }
      break;

    case MemoryRequest::WRITEBACK:
      INCREMENT(writes);
      if ((_lastOp == MemoryRequest::READ) || (_lastOp == MemoryRequest::READ_FOR_WRITE) || (_lastOp == MemoryRequest::PREFETCH)) {
        INCREMENT(readtowrites);
        latency += _readToWriteLatency;
        turnAround = _readToWriteLatency;
      }
      break;

    case MemoryRequest::WRITE:
    case MemoryRequest::PARTIALWRITE:
      fprintf(stderr, "Memory controller cannot get a write\n");
      exit(0);          
    }

    _lastOp = request -> type;

      
    // Get the row address of the request
    addr_t logicalRow = (request -> virtualAddress) / _rowSize;
    uint32 bankIndex = logicalRow % _numBanks;
    uint32 rowID = logicalRow / _numBanks;

    // check if the access is a row hit or conflict
    if (_openRow[bankIndex] == rowID) {
      if((request -> type == MemoryRequest::READ)||(request -> type == MemoryRequest::READ_FOR_WRITE)||(request -> type == MemoryRequest::PREFETCH)){ 
        INCREMENT(Readrowhits);
      }
      else 
        INCREMENT(Writerowhits);
      latency += _rowHitLatency;
    }

    else {
      INCREMENT(rowconflicts);
      latency += _rowConflictLatency;
	// this line is for removing write row conflict penalty
	//if(request -> type == MemoryRequest::WRITEBACK)	latency = latency - _rowConflictLatency + _rowHitLatency;
      _openRow[bankIndex] = rowID;
    }

    request -> AddLatency(latency);
    request -> serviced = true;
    if((request -> type == MemoryRequest::READ)||(request -> type == MemoryRequest::READ_FOR_WRITE)||(request -> type == MemoryRequest::PREFETCH))	readandprefetch_cycles += (_channelDelay + turnAround);
    else writeback_cycles += (_channelDelay + turnAround);
    return _channelDelay + turnAround;
*/

  }


  // -------------------------------------------------------------------------
  // Function to process the return of a request. Return value indicates
  // number of busy cycles for the component.
  // -------------------------------------------------------------------------
    
  cycles_t ProcessReturn(MemoryRequest *request) { 
    return 0; 
  }


  // -------------------------------------------------------------------------
  // Overriding process pending requests. To do batch processing
  // -------------------------------------------------------------------------


  void ProcessPendingRequests() {

    
    while(DRAMtime < *_simulatorCycle){
    DRAMtime++;						// update the DRAM time
	mem->update();						
    /*
    if(DRAMtime == 2608205){
     cout << "Break here \n";
     }
    */
    }

	//OutFile << " SIM time : " << *_simulatorCycle << endl;
	//OutFile << " DRAM time : " << DRAMtime << endl << endl;

    // if processing return
    if (_processing)	return;
    _processing = true;

    // if the request queue is empty return
    if (CheckifEmpty(_queue) && _readQ.empty() && _writeQ.empty() 
        && _readRowHits.empty() && _writeRowHits.empty()) {
      _processing = false;
      return;
    }

    MemoryRequest *request;

    // take all the requests in the queue till the simulator cycle and add
    // them to the read or write queue.
    if (!CheckifEmpty(_queue)) {		// this checks if there are non-s_f_d requests in the queue

      //cout << "Before processing : " << _queue.size() << endl;
      vector <MemoryRequest *> temp;

      while(_queue.top()->s_f_d){	// Coz I don't want to pull out s_f_d requests from the queue
	temp.push_back(_queue.top());
	_queue.pop();
	}
      	
      request = _queue.top();		// I am bound to get a non-s_f_d request sometime

      while (request -> currentCycle <= (*_simulatorCycle)) {
        _queue.pop();
        unsigned sizetemp = temp.size();
	for(unsigned i=0;i<sizetemp;i++){_queue.push(temp.back());temp.pop_back();}	// rectify the queue
	
        // if the request is already serviced
        if (request -> serviced) {
          cycles_t busyCycles = ProcessReturn(request);
	  _currentCycle += busyCycles;

          SendToNextComponent(request);
        }

        // else add the request to the corresponding queue 

        else {

          switch (request -> type) {
          case MemoryRequest::READ: case MemoryRequest::READ_FOR_WRITE: case MemoryRequest::PREFETCH: 
            _readQ.push_back(request);
            break;

          case MemoryRequest::WRITEBACK:
            _writeQ.push_back(request);
            break;

          case MemoryRequest::WRITE:
          case MemoryRequest::PARTIALWRITE:
            printf("Memory controller cannot receive a direct write\n");
            exit(0);
          }
        }
      

        if (CheckifEmpty(_queue))				// If no non-s_f_d requests present
	{request = NULL;
          break;}

	while(_queue.top()->s_f_d){				// Coz I don't want to pull out s_f_d requests from the queue
	temp.push_back(_queue.top());
	_queue.pop();
	}
        request = _queue.top();
      }

	if(request!=NULL){	
	_queue.push(request);
	unsigned sizetemp = temp.size();
	for(unsigned i=0;i<sizetemp;i++){_queue.push(temp.back());temp.pop_back();}	// rectify the queue
   	}
      
 
    }

    // process requests until there are none or the component time 
    // exceeds simulator time
    while (_currentCycle <= (*_simulatorCycle)) {

	// after some point, _currentCycle exceeds *_simulatorCycle, then we can no longer issue requests

	// I am altering behaviour now, after every ProcessRequest, _currentCycle does not increase, and doesn't become equal to 
        // *_simulatorCycle, _currentCycle can increase through the callback

    // Why do we this condition here ? 

      // get the next request to schedule
      // TODO: request = (*this.*NextRequest)();
      request = FRFCFSDrainWhenFull();

      if (request == NULL)
        break;

      // process the request
      cycles_t now = max(request -> currentCycle, _currentCycle);
      cycles_t _prevCycle = _currentCycle;
      _currentCycle = now;
      
      request -> currentCycle = now;
      ProcessRequest(request);
      //cycles_t busyCycles = ProcessRequest(request);


	// I will not get back busyCycles immediately, it has gone to a transaction queue
      /*
      for(int i=0;i<busyCycles;i++){
	_currentCycle++;
	//mem->update();
	}
      */

      //SendToNextComponent(request);
    }

    _processing = false;
  }

  // ------------------------------------------------------------------------
  // Function to check if queue is devoid of non-s_f_d requests
  // -----------------------------------------------------------------------
  bool CheckifEmpty(RequestPriorityQueue _queue){
	if(_queue.empty())	return true;
	else{
	while(!_queue.empty()){
	if(!(_queue.top()->s_f_d))	return false;
	_queue.pop();
	}
	return true;
	}
  }

  // -------------------------------------------------------------------------
  // Function to check if a request is a row buffer hit
  // -------------------------------------------------------------------------

  bool IsRowBufferHit(MemoryRequest *request) {
    addr_t logicalRow = (request -> virtualAddress) / _rowSize;
    uint32 bankIndex = logicalRow % _numBanks;
    addr_t rowID = logicalRow / _numBanks;
    // if logical row of two requests are same, then they have to be in same row buffer
    if (_openRow[bankIndex] == rowID)
      return true;
    return false;
  }

#include "MemorySchedulers.h"

};

#endif // __CMP_MEMORY_CONTROLLER_H__

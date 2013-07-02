// -----------------------------------------------------------------------------
// File: DBI.h
// Description:
//    Defines a dirty bit index. 
//    It is a vector that stores the dirty bit information for each cache line 
//    according to the DRAM row they would occupy in memory, no restriction on size
// -----------------------------------------------------------------------------

#ifndef __DBI_H__
#define __DBI_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "Types.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------

#include <vector>
#include <list>
#include <map>
#include <cassert>
#include <bitset>
#include <utility>
#include <iostream>

// -----------------------------------------------------------------------------
// Some defintions -> DRAM Parameters
// -----------------------------------------------------------------------------

#define BLOCKS_PER_ROW 128
#define NUMBER_OF_BANKS 8

// -----------------------------------------------------------------------------
// Class: DBI
// Description:
//    Defines an abstract Dirty Bit Index
// -----------------------------------------------------------------------------

class dbi_t {

public:

  // -------------------------------------------------------------------------
  // Entry structure. 
  // -------------------------------------------------------------------------

  struct entry {
    bitset<BLOCKS_PER_ROW> dirtyBits;		// every ctag (block address gets an entry)
    // Constructor
    entry() {	
      dirtyBits.reset();				        // start off with an all zero bitvector
    }    
  };


  // -------------------------------------------------------------------------
  // Size of the dbi, this should be set equal to the size of the cache for now
  // -------------------------------------------------------------------------

/* Not needed
  uint32 _size;					
*/

protected:

  // -------------------------------------------------------------------------
  // pair of bank index and row Id, this acts as key for the dbi entries
  // -------------------------------------------------------------------------

/* Not needed
  typedef pair <uint32,uint32> dbiId;
*/				

  // -------------------------------------------------------------------------
  // vector of dbi entries, with ctag as the index and entry as the value
  // -------------------------------------------------------------------------

  map <addr_t, entry> _dbi;				

  // -------------------------------------------------------------------------
  // Number of entries present in the DBI, slots currently occupied
  // -------------------------------------------------------------------------

/* Not needed
  uint32 _nbrEntries;
*/

  // -------------------------------------------------------------------------
  // Size of the DRAM row size in terms of number of blocks
  // -------------------------------------------------------------------------

  uint32 _rowSizeinBlocks;

  // -------------------------------------------------------------------------
  // Number of banks in DRAM
  // -------------------------------------------------------------------------

  uint32 _numBanks;


public : 

  // -------------------------------------------------------------------------
  // Function to set a dirty bit
  // -------------------------------------------------------------------------

void setDirty(addr_t ctag){				// ctag is the block address
    
   addr_t logicalRow = ctag / _rowSizeinBlocks;		// gives the logical row
   uint32 rowOffset = ctag % _rowSizeinBlocks;

   _dbi[logicalRow].dirtyBits.set(rowOffset);		// sets the corresponding block as dirty

  }

  // -------------------------------------------------------------------------
  // Function to clean a dirty bit
  // -------------------------------------------------------------------------

void cleanDirty(addr_t ctag){				// ctag is the block address
    
   addr_t logicalRow = ctag / _rowSizeinBlocks;		// gives the logical row
   uint32 rowOffset = ctag % _rowSizeinBlocks;

   _dbi[logicalRow].dirtyBits.reset(rowOffset);		// sets the corresponding block as dirty

  }


  // -------------------------------------------------------------------------
  // Function to test if a bit is dirty
  // -------------------------------------------------------------------------

bool isDirty(addr_t ctag){

   addr_t logicalRow = ctag / _rowSizeinBlocks;
   uint32 rowOffset = ctag % _rowSizeinBlocks;

   return _dbi[logicalRow].dirtyBits.test(rowOffset);	// return true if corresponding block is dirty

  }

  // -------------------------------------------------------------------------
  // Function to insert a new entry into the DBI, corresponding to a block addr
  // It inserts a new entry 
  // Every time we enter a cache block in the tagstore of the LLC, we do an 
  // InsertEntry in the DBI as well, this will just make a new entry if a corr entry
  // was not defined or point to an existing entry
  // Returns true if element insertion was successful, false if not
  // -------------------------------------------------------------------------


void InsertEntry(addr_t ctag, bool dirty){
   addr_t logicalRow = ctag / _rowSizeinBlocks;
   uint32 rowOffset = ctag % _rowSizeinBlocks;
   entry newEntry;

   if(_dbi.find(logicalRow)==_dbi.end())	_dbi[logicalRow] = newEntry;	//insert new entry when no element is present

   if(dirty)	_dbi[logicalRow].dirtyBits.set(rowOffset);
   
  }


  // -------------------------------------------------------------------------
  // Function to remove an entry from the DBI, not gonna be needed in the 
  // first implementation
  // Should probably take DRAM row address as an argument
  // -------------------------------------------------------------------------

// TODO : Check this Function

/*
void DeleteEntry(addr_t rowId){
  _dbi.erase(rowId);						// Remove the corresponding entry
  _nbrEntries = _dbi.size();					// update number of elements in DBI

  }
*/
  // -------------------------------------------------------------------------
  // Function to test if an entry corresponding to a particular block address
  // is present in the DBI, should return a  bool 
  // This may be helpful, say before going for a DeleteEntry operation
  // -------------------------------------------------------------------------

bool testIfPresent(addr_t ctag){
  addr_t logicalRow = ctag / _rowSizeinBlocks;
  return (_dbi.find(logicalRow)==_dbi.end())?false:true;

  }


  // -------------------------------------------------------------------------
  // DBI constructor
  // -------------------------------------------------------------------------

  dbi_t() {
    // initialize members    
    _rowSizeinBlocks = BLOCKS_PER_ROW; 						// A default value for now
    _numBanks = NUMBER_OF_BANKS;						// A default value for now
  }


  // -------------------------------------------------------------------------
  // Function to return a count of number of entries in the DBI
  // -------------------------------------------------------------------------

/* Not needed
  uint32 count() {
    return _size - _nbrEntries;
  }
*/

  // -------------------------------------------------------------------------
  // Function to test if the DBI is full
  // -------------------------------------------------------------------------

/* Not needed
  bool isFull() {
    return (_nbrEntries == _size);
  }
*/

  // -----------------------------------------------------------------------------
  // Replace an old entry in the DBI with a new entry
  // entry corresponding to rowIdOld is to be replaced by that corr. to rowIdNew
  // dirty indicates if new entry has 
  // -----------------------------------------------------------------------------

// TODO : Check this Function

/*
  void replaceEntry(addr_t rowIdOld, addr_t rowIdNew) {
     DeleteEntry(rowIdOld);
     entry newEntry;
     _dbi.insert(make_pair(rowIdNew, newEntry));
     _nbrEntries = _dbi.size();
  }
*/
};
#endif // __DBI_H__



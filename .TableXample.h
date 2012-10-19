// -----------------------------------------------------------------------------
// File: TableXample.h
// Description:
//    Extends the table class with xample replacement policy
// -----------------------------------------------------------------------------

#ifndef __TABLE_XAMPLE_H__
#define __TABLE_XAMPLE_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "Types.h"
#include "Table.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Class: xample_table_t
// Description:
//    Extends the table class with xample replacement policy.
// -----------------------------------------------------------------------------

KVTemplate class xample_table_t : public TableClass {

  protected:

    // members from the table class
    TableClass::_size;


    // -------------------------------------------------------------------------
    // Implementing virtual functions from the base table class
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Function to update the replacement policy
    // -------------------------------------------------------------------------

    void UpdateReplacementPolicy(uint32 index, TableOp op) {

      switch(op) {
        
        case TABLE_INSERT:
          break;

        case TABLE_READ:
          break;

        case TABLE_UPDATE:
          break;

        case TABLE_REPLACE:
          break;

        case TABLE_INVALIDATE:
          break;
      }
    }


    // -------------------------------------------------------------------------
    // Function to return a replacement index
    // -------------------------------------------------------------------------

    uint32 GetReplacementIndex() {
    }


  public:

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    xample_table_t(uint32 size) : TableClass(size) {
    }
};

#endif // __TABLE_XAMPLE_H__

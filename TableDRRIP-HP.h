// -----------------------------------------------------------------------------
// File: TableDRRIP.h
// Description:
//    Extends the table class with drrip replacement policy
// -----------------------------------------------------------------------------

#ifndef __TABLE_DRRIP_HP_H__
#define __TABLE_DRRIP_HP_H__

// -----------------------------------------------------------------------------
// Module includes
// -----------------------------------------------------------------------------

#include "Types.h"
#include "Table.h"

// -----------------------------------------------------------------------------
// Standard includes
// -----------------------------------------------------------------------------

#include <vector>

// -----------------------------------------------------------------------------
// Class: drrip_hp_table_t
// Description:
//    Extends the table class with drrip replacement policy.
// -----------------------------------------------------------------------------

KVTemplate class drrip_hp_table_t : public TableClass {

  protected:

    // members from the table class
    TableClass::_size;

    // list of rrpvs
    vector <saturating_counter> _rrpv;

    // max value
    uint32 _max;

    // BRRIP counter
    cyclic_pointer _brripCounter;

    // -------------------------------------------------------------------------
    // Implementing virtual functions from the base table class
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Function to update the replacement policy
    // -------------------------------------------------------------------------

    void UpdateReplacementPolicy(uint32 index, TableOp op, policy_value_t pval) {

      switch(op) {
        
        case TABLE_INSERT:
          // SRRIP
          if (pval == 0) {
            _rrpv[index].set(1);
          }

          // BRRIP
          else if (pval == 1) {
            if (_brripCounter == 0)
              _rrpv[index].set(1);
            else 
              _rrpv[index].set(0);
          }

          // LRRIP
          else if (pval == 2) {
            _rrpv[index].set(0);
          }
          break;

        case TABLE_READ:
          _rrpv[index].set(_max);
          break;

        case TABLE_UPDATE:
          _rrpv[index].set(_max);
          break;

        case TABLE_REPLACE:
          // SRRIP
          if (pval == 0) {
            _rrpv[index].set(1);
          }

          // BRRIP
          else if (pval == 1) {
            if (_brripCounter == 0)
              _rrpv[index].set(1);
            else 
              _rrpv[index].set(0);
          }

          // LRRIP
          else if (pval == 2) {
            _rrpv[index].set(0);
          }
          break;

        case TABLE_INVALIDATE:
          _rrpv[index].set(0);
          break;
      }
    }


    // -------------------------------------------------------------------------
    // Function to return a replacement index
    // -------------------------------------------------------------------------

    uint32 GetReplacementIndex() {
      _brripCounter.increment();
      while (1) {
        for (uint32 i = 0; i < _size; i ++) {
          if (_rrpv[i] == 0)
            return i;
        }

        for (uint32 i = 0; i < _size; i ++) {
          _rrpv[i].decrement();
        }
      }
    }


  public:

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    drrip_hp_table_t(uint32 size) : TableClass(size), _brripCounter(64) {
      _max = 7;
      _rrpv.resize(size, saturating_counter(_max));
    }
};

#endif // __TABLE_DRRIP_HP_H__

/*  @file
 *  cellset : interface to operate on memory cells (addr, size) pairs
 *  
 */

#ifndef __CELLSET_HXX
#define __CELLSET_HXX

#include <list>

struct Cell
{
public:
  unsigned  addr;
  unsigned  size;
  void     *data;
  Cell (unsigned paddr, unsigned psize, void *d) 
  { addr = paddr; size = psize; data = d; }
};

class CellSet 
{
  std::list<Cell> cells;
public:  
  std::list<Cell>& Cells () { return cells; }
  void Clear () { cells.clear (); }
  void Insert (unsigned, unsigned, void *x);
  void Insert (CellSet &);
  bool SubtractIfIntersecting (CellSet &, std::list<void *> &);
  bool IsEmpty () { return cells.empty (); }
  void Print ();
};

#endif

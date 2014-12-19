
#include <iostream>
#include "cellset.hxx"
using namespace std;

inline int
maxi (unsigned x, unsigned y)
{
  return x > y? x : y;
}

void
CellSet::Insert (unsigned start, unsigned size, void *x)
{
  list<Cell>::iterator iter;
  list<Cell>::iterator piter = cells.end ();

  for (iter = cells.begin (); iter != cells.end (); iter++)
    {
      if (start < iter->addr)
	break;
      piter = iter;
    }
  
  if (piter != cells.end () && 
      (start <  piter->addr + piter->size))
    {
      piter->size = maxi (start + size - piter->addr, piter->size);      
    }
  else
    {
      Cell newC (start, size, x);
      piter = cells.insert (iter, newC);
    }

  while (iter != cells.end () &&
	 iter->addr < piter->addr + piter->size)
    {
      list<Cell>::iterator tmp = iter;
      piter->size = maxi (iter->addr + iter->size - piter->addr, 
			    piter->size);
      iter++;
      cells.erase (tmp);
    }
}

void
CellSet::Insert (CellSet& cellSet1)
{
  list<Cell> &cells1 = cellSet1.Cells ();
  list<Cell>::iterator iter;
  for (iter = cells1.begin (); iter != cells1.end (); iter++)
    {
      Insert (iter->addr, iter->size, iter->data);
    }
}

void
CellSet::Print ()
{
  list<Cell>::iterator iter = cells.begin ();
  
  cout << "{" ;
  while (iter != cells.end ()) 
    {
      cout << " (" << (void *) iter->addr << "," << iter->size << ")";
      iter++;
    }
  cout << " }\n";   
}


// @brief: this <- this \ cellSet 1; cellSet1 <- (this ^ cellSet1)
bool
CellSet::SubtractIfIntersecting (CellSet& cellSet1, list<void *> &dlist)
{
  unsigned start;
  unsigned size;
  bool i1intersects;
  list<Cell> &cells1 = cellSet1.Cells ();
  list<Cell>::iterator iter, iter1, tmp;

  iter = cells.begin ();  iter1 = cells1.begin ();

  i1intersects = false;
  while (iter != cells.end () && iter1 != cells1.end ())
    {
      if ( iter->addr < iter1->addr && 
	   iter1->addr + iter1->size < iter->addr + iter->size)   
	// *iter1 proper subset of *iter
	{
	  start = iter->addr;  size = iter->size;
	  
	  iter->addr = iter1->addr + iter1->size;
	  iter->size = start + size - iter->addr;
	  
	  Cell newC (start, iter1->addr - start, iter->data);
	  cells.insert (iter, newC);
	  dlist.push_front (iter->data);
	  i1intersects = true;
	}
      else if (iter1->addr <= iter->addr && 
	       iter->addr + iter->size <= iter1->addr + iter1->size) 
	// *iter subset of *iter1
	{
	  tmp = iter; iter++;
	  dlist.push_front (tmp->data);
	  cells.erase (tmp);
	  i1intersects = true;
	}
      else if (iter->addr < iter1->addr + iter1->size && 
	       iter1->addr + iter1->size < iter->addr + iter->size)
	{
	  start = iter->addr; size = iter->size;
	  iter->addr = iter1->addr + iter1->size;
	  iter->size =  start + size - iter->addr;
	  dlist.push_front (iter->data);
	  i1intersects = true;
	}
      else if (iter->addr < iter1->addr && 
	       iter1->addr < iter->addr + iter->size)
	{
	  start = iter->addr; size = iter->size;
	  iter->size = iter1->addr - iter->addr;
	  dlist.push_front (iter->data);
	  i1intersects = true;
	}
      else if (iter->addr < iter1->addr)
	iter++;
      else 
	{
	  tmp = iter1;
	  iter1++;
	  if (!i1intersects)
	    cells1.erase (tmp);
	  i1intersects = false;
	}
    }
  

  while (iter1 != cells1.end ()) 
    {
      if (!i1intersects)
	{
	  tmp = iter1;
	  iter1++;
	  cells1.erase (tmp);
	}
      else
	iter1++;
      i1intersects = false;
    }
    
  return !(cells1.empty ());
}

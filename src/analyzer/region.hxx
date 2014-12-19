#ifndef __REGION_H
#define __REGION_H

#include <set>
#include <iostream>
#include <ext/hash_map>


#include "diablo.hxx"

#include <boost/dynamic_bitset.hpp>

// boost c++ bitset declaration
typedef boost::dynamic_bitset<> BitVector;
typedef BitVector PathVector;

// forward declaration
class Region;
typedef Graph<Region> RegionGraph;

struct Triple
{
  Region *D;
  Region *U;
  Variable *V;  
  Triple (Region *d, Region *u, Variable *v)
  { D = d; U = u; V = v;}
};

struct TripleEqual
{
  bool operator() (const Triple &X, const Triple &Y) const
  {
    return (X.D == Y.D) && (X.U == Y.U) && (X.V == Y.V);
  }
};

struct TripleHashFcn
{
  size_t operator() (const Triple &T) const
  {
    return ((size_t) T.D + (size_t) T.U + (size_t) T.V);
  }
};  

typedef __gnu_cxx::hash_map<Triple, PathVector, TripleHashFcn, TripleEqual> 
        DefUsePathVector;

enum {REGION_ACYCLIC, REGION_LOOP, REGION_FUNCTION};

class Region: public Node
{         
  int type;
  int numPaths;    

  PathVector *rBPV; 
  // null if region is a basic region
  RegionGraph*  rDAG;   
  DefUsePathVector rDUPV; 
   
  // basic-region contains the corr. block
  // summary-region contains the corr. header block
  BasicBlock *entryBlock;
    
  // set of exit nodes - has no successor in this region
  std::set<Region *>  exitRegions;

  // set of target nodes which are not in this region
  std::set<Node *>  exitTargets;

  std::set<Variable*> upwardExpUses;
  std::set<Variable*> downwardExpDefs;

public:
  Region (int t = REGION_ACYCLIC) 
  { rDAG = NULL; rBPV = NULL; type = t;}

  int Type () { return type;}
  void EntryBlock (BasicBlock *BB) { entryBlock = BB; }
  BasicBlock *EntryBlock ()        { return entryBlock; }
  
  // ~Region () { if(rBPV) delete rBPV; rBPV = NULL; }
  
  int& NumPaths () { return numPaths; }

  const std::set<Node *>&  ExitTargets()  { return exitTargets; }
  const std::set<Region *>& ExitRegions() { return exitRegions; }

  const std::set<Variable *>& UpwardExpUses ()   { return upwardExpUses; }
  const std::set<Variable *>& DownwardExpDefs () { return downwardExpDefs; }

  RegionGraph*& DAG () { return rDAG;}
  PathVector*& BPV ()  { return rBPV;}
  DefUsePathVector &DUPV () { return rDUPV; }

  void AddExitRegion (Region *R)     { exitRegions.insert (R); }
  void RemoveExitRegion (Region *R)  { exitRegions.erase (R); }
  void AddExitTarget (Node *N)       { exitTargets.insert (N); }
  void AddExitTarget (BasicBlock *B) { exitTargets.insert ((Node *) B); }

  bool IsBasicRegion ()   { return (rDAG == NULL); }
  bool IsSummaryRegion () { return (rDAG != NULL); }

  Address StartAddress () { return BBL_CADDRESS (entryBlock); }

  void ComputeUpwardExpUses   ();
  void ComputeDownwardExpDefs ();
  void ComputeBlockPathVectors ();     
  void ComputeDefUsePathVectors ();
};

typedef void (*ProcessRegionHook)(Region *, int);

void ProcessRegionHierarchy (Region *, ProcessRegionHook, int = 0);
Region *RegionHierarchy (Function *F);

void ComputeDefUsePathVectors (RegionGraph *RG);
void ComputeBlockPathVectors  (RegionGraph *RG);

#define FOREACH_NODEPTR_IN_NODESET(NP, S)				\
  for (set<Node*>::iterator NP = S.begin(); NP != S.end(); NP++)

#endif

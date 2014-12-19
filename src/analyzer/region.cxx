
#include <map>
#include "diablo.hxx"
#include "region.hxx"
#include "loop.hxx"
using namespace std;

LoopList FunctionOutermostLoops;

static  map<Node*, Node*> enclosingRegion;

inline void 
EncloseRegionIn (Region *innerR, Region *outerR)
{
  enclosingRegion[(Node *) innerR] = (Node *) outerR;
}

inline void
EncloseRegionIn (BasicBlock *innerB, Region *outerR)
{
  enclosingRegion[(Node *) innerB] = (Node *) outerR;
}

Region *
OutermostEnclosingRegion (Node *node)
{
  Region *I, *O;
  // @OPTIMIZE make this iterative (tail recursion?)
  if ((I = (Region *) enclosingRegion[node]) == NULL)
    return NULL;
  if ((O = (Region *) OutermostEnclosingRegion(I)) == NULL)
    return I;  
  // @OPTIMIZE: EnclosingRegion[node] = O;
  return O;
}

//! @return basic region corr. to BB
Region *
BasicRegion (BasicBlock *BB)
{
  Region *basicRegion = new Region();
  
  basicRegion->EntryBlock (BB);

  FOREACH_SUCC_EDGE_IN_BB (succE,BB)     
    basicRegion->AddExitTarget (succE->TargetIntraProc ());

  basicRegion->ComputeDownwardExpDefs (); 
  basicRegion->ComputeUpwardExpUses ();
  EncloseRegionIn (BB, basicRegion);

  return basicRegion;
}

//! @brief add edges to DAG of region with only nodes & exitTargets initialized
inline void
AddEdgesToRegionDAG (Region *summaryRegion)
{
  Region *entryRegion = 
    OutermostEnclosingRegion ((Node *) summaryRegion->EntryBlock ());  
  
  FOREACH_NODE_IN_GRAPH (N, summaryRegion->DAG()) 
    {
      Region *R = (Region *) N;
      if (R->ExitTargets().empty ())
	summaryRegion->AddExitRegion (R);

      FOREACH_NODEPTR_IN_NODESET (nPtr, R->ExitTargets ()) 
	{
	  Node *exitTarget = *nPtr;
	  Region *S = OutermostEnclosingRegion (exitTarget);
	  if (S == R) 
	    continue;
	  else if (S == entryRegion)
	    summaryRegion->AddExitRegion (R);
	  else if (S != NULL && summaryRegion->DAG()->Contains (S))
	    summaryRegion->DAG()->AddEdge (R, S);
	  else 
	    {
	      summaryRegion->AddExitTarget (S? S: exitTarget);
	      summaryRegion->AddExitRegion (R);
	    }	  
	}
    }
  return;
}


#define PATH_SIZE_LIMIT 256
void EnumerateRegionPaths (Region *summaryRegion);

bool VariableIntersect (set<Variable*> &, const set<Variable*> &, 
			 const set<Variable*> &);

//! 
void 
SummarizeRegion (Region *summaryRegion)
{
  //EnumerateRegionPaths (summaryRegion);  
  summaryRegion->ComputeBlockPathVectors ();
  summaryRegion->ComputeDefUsePathVectors ();

  summaryRegion->ComputeDownwardExpDefs (); 
  summaryRegion->ComputeUpwardExpUses   (); 

  if (summaryRegion->Type () == REGION_LOOP)
    {
      Region *entryRegion = OutermostEnclosingRegion 
	((Node *) summaryRegion->EntryBlock ());
      set<Variable*> vars;
      set<Variable*>::iterator siter;
      VariableIntersect (vars, summaryRegion->DownwardExpDefs (), 
			 summaryRegion->UpwardExpUses ());
      for (siter = vars.begin (); siter != vars.end (); siter++)
	{
	  PathVector P (entryRegion->NumPaths ());
	  Triple T (summaryRegion, summaryRegion, *siter);
	  P.set ();
	  // @TOFIX improve accuracy
	  (summaryRegion->DUPV())[T] = P;
	}
    }	   

  list<Region*> &nodesInTorder = summaryRegion->DAG()->TopologicalSort ();
  for (list<Region *>::iterator N = nodesInTorder.begin();
       N != nodesInTorder.end(); N++)
    {
      Region *R = *N;
      EncloseRegionIn (R, summaryRegion);
    }
  return;
}

void 
dfsTopologicalSort (Region *R, list<Region *> &order) 
{
  NodeMark2 ((Node *) R);
  // @TODO: use only diablo abstraction (too many details exposed)
  FOREACH_SUCC_EDGE_IN_NODE (succE, R)
    if (!NodeIsMarked2 (EDGE_TAIL (succE)))
      dfsTopologicalSort ((Region*)EDGE_TAIL (succE), order);
  
  order.push_front (R);
}

//! @brief find no of paths from header through nodes that are dominated by it
int
PathsInRegionAt (Region *header, set<Region*> &dominated)
{
  list<Region *>  members, order;
  list<Region *>::iterator elm;
  set<Region *>::iterator selm;

  NodeMarkInit2 ();  
  dfsTopologicalSort (header, members);
  // @TODO: use only diablo abstraction (too many details exposed)
  for (elm = members.begin (); elm != members.end (); elm++) 
    {
      bool is_dominated = true;
      Region *currR = *elm;

      if (currR != header)           
	FOREACH_PRED_EDGE_IN_NODE (predE, currR)
	  if (dominated.find ((Region *) EDGE_HEAD (predE)) == 
	      dominated.end ()) 
	    {
	      is_dominated = false;
	      break;
	    }

      if (is_dominated) 
	{
	  // @OPTIMIZE: NodeMark ((Node *) currR);
	  order.push_front (currR);
	  dominated.insert (currR);
	}
    }

  for (elm = order.begin (); elm != order.end (); elm++) 
    {
      bool nonExitRegion = false;
      Region *currR = *elm;
      currR->NumPaths () = 0;
      FOREACH_SUCC_EDGE_IN_NODE (succE, currR) 
	if (dominated.find ((Region *) EDGE_TAIL (succE)) != dominated.end ())
	  {
	    currR->NumPaths () += ((Region *) EDGE_TAIL (succE))->NumPaths ();
	    nonExitRegion = true;    
	  }
    
      if (nonExitRegion == false)
	currR->NumPaths ()++;
    }  
  return header->NumPaths ();
}
  
pair<Region*, set<Region *>*>* 
FindRegionToSummarize (Region *R)
{
  int maxPaths = 1;
  list<Region*> stack;
  static set<Region *> regionSet;
  static pair<Region*, set<Region *>*> retPair;

  retPair.first = NULL;  retPair.second = NULL;
  // @TODO: use only diablo abstraction (too many details exposed)
  NodeMarkInit ();
  stack.push_front (R);
  while (!stack.empty ()) 
    {
      int nPaths;
      set<Region*> tmpSet;
      Region *currRegion = stack.front (); stack.pop_front ();

      NodeMark ((Node *) currRegion);
      if( currRegion->StartAddress () == 0x805a5c3)
	printf ("?");

      if (currRegion != R) 
	{
	  nPaths = PathsInRegionAt (currRegion, tmpSet);

	  if (nPaths > maxPaths && nPaths < PATH_SIZE_LIMIT)
	    maxPaths = nPaths, regionSet = tmpSet, retPair.first = currRegion;
	}
    

      FOREACH_SUCC_EDGE_IN_NODE (succE, currRegion)
	if (!NodeIsMarked ((Node *) EDGE_TAIL(succE)))
	  stack.push_front ((Region *) EDGE_TAIL (succE));
    }

  //  assert (retPair.first != NULL);
  retPair.second = &regionSet;
  return &retPair;
}
  

void
BlockSummaryRegion (pair<Region *, set<Region *>*> *newRegion, 
		    Region *outerRegion)
{
  bool exitRegion;
  list<Edge *> edgeList;
  list<Edge *>::iterator lelm;
  set<Region *>::iterator selm;  
  Region *summaryRegion = new Region ();
  set<Region *> *regionSet = newRegion->second;

  summaryRegion->DAG() = (new RegionGraph());
  summaryRegion->EntryBlock (newRegion->first->EntryBlock ());

  exitRegion = false;
  for (selm = regionSet->begin(); selm != regionSet->end(); selm++) 
    {
      Region *R = (Region *) *selm;
      outerRegion->DAG()->UnlinkNode (R);

      if (outerRegion->ExitRegions().find (R) 
	  != outerRegion->ExitRegions().end ()) 
	{
	  exitRegion = true;
	  summaryRegion->AddExitRegion (R);
	  outerRegion->RemoveExitRegion (R);            
	}
      summaryRegion->DAG()->AddNode (R);
    }
  
  outerRegion->DAG()->AddNode (summaryRegion);
  if (exitRegion)
    outerRegion->AddExitRegion (summaryRegion);

  FOREACH_EDGE_IN_GRAPH (E, outerRegion->DAG ())
    edgeList.push_front (E);

  for (lelm = edgeList.begin(); lelm != edgeList.end(); lelm++) 
    {
      Region *headRegion = (Region *) EDGE_HEAD (*lelm);
      Region *tailRegion = (Region *) EDGE_TAIL (*lelm);
      bool headInRegion = summaryRegion->DAG()->Contains (headRegion);
      bool tailInRegion = summaryRegion->DAG()->Contains (tailRegion);
      
      if (headInRegion && tailInRegion) 
	{
	  outerRegion->DAG()->UnlinkEdge (*lelm);
	  summaryRegion->DAG()->AddEdge (headRegion, tailRegion);
	}
      else if (headInRegion) 
	{
	  outerRegion->DAG()->UnlinkEdge (*lelm);
	  outerRegion->DAG()->AddEdge (summaryRegion, tailRegion);
	  summaryRegion->AddExitRegion (headRegion);
	  summaryRegion->AddExitTarget (tailRegion);
	}
      else if (tailInRegion) 
	{
	  outerRegion->DAG()->UnlinkEdge (*lelm); 
	  outerRegion->DAG()->AddEdge (headRegion, summaryRegion);
	}
    }

  SummarizeRegion (summaryRegion);
  EncloseRegionIn (summaryRegion, outerRegion);
  
  return;
}

void
EnumerateRegionPaths (Region *summaryRegion)
{  
  bool withInSizeLimit;
  map <Region*,bool> cantSimplify;
  list<Region *>::reverse_iterator N;
  
  do 
    {
      Region *R;
      withInSizeLimit = true;
      list <Region *>&  nodesInTorder =	
	summaryRegion->DAG()->TopologicalSort ();
      
      for (N = nodesInTorder.rbegin (); N != nodesInTorder.rend (); N++) 
	{
	  R = *N;
	  R->NumPaths () = 0;
	}
    
      for (N = nodesInTorder.rbegin (); N != nodesInTorder.rend (); N++) 
	{
	  R = *N;
	  R->NumPaths() = 0;     
	  FOREACH_SUCC_EDGE_IN_NODE (SE, ((Node *) R)) 
	    {
	      Region *S = (Region *) EDGE_TAIL (SE);
	      if (S->NumPaths() == 0)
		printf ("Loop in Region: %p!\n", (void *) S->StartAddress());
	      R->NumPaths() += S->NumPaths();
	    }

	  if (summaryRegion->ExitRegions().find (R) != 
	      summaryRegion->ExitRegions().end ()) 
	    {
	      R->NumPaths() ++; 
	    }
	
	  if (!cantSimplify[R] && R->NumPaths() > PATH_SIZE_LIMIT) 
	    {	  
	      withInSizeLimit = false; 
	      break;
	    }
	}

    if (!withInSizeLimit) 
      {
	pair<Region *, set<Region*>*>* P = FindRegionToSummarize (R);
	if (P->first) 
	  {	
	    BlockSummaryRegion (P, summaryRegion);     
	    summaryRegion->DAG()->Dirtied ();
	  }
	else
	  cantSimplify[R] = true;
      }
    }  while (!withInSizeLimit);  
}

//! @return summary region for the  loop
Region*
LoopSummaryRegion (Loop *loop)
{
  Region *entryRegion;
  LoopList nestedLoops;
  Region *summaryRegion = new Region (REGION_LOOP);

  nestedLoops = LOOP_CHILDS (loop);
  summaryRegion->EntryBlock ((BasicBlock *) LOOP_HEADER (loop)); 
  summaryRegion->DAG() = (new RegionGraph());

  // add nodes (summary regions) to region DAG 
  FOREACH_LOOP_IN_LOOPLIST (L, nestedLoops) 
    summaryRegion->DAG()->AddNode (LoopSummaryRegion (L));

  // add nodes (basic regions) to region DAG
  LOOP_ITERATOR_INIT (loop,loopit);
  FOREACH_BB_IN_LOOP (BB, loopit)
    if (enclosingRegion[(Node *)BB] == NULL && BB->StartAddress ()) 
      summaryRegion->DAG()->AddNode (BasicRegion (BB));         
  LOOP_ITERATOR_END (loop,loopit); 

  // add edges between nodes of region DAG
  AddEdgesToRegionDAG (summaryRegion);
  
  SummarizeRegion (summaryRegion);
  
  return summaryRegion;  
}

//! @return function region summary
Region*
RegionHierarchy (Function *F)
{
  Region *summaryRegion = new Region(REGION_FUNCTION);
  LoopList nestedLoops;

  enclosingRegion.clear (); 

  nestedLoops =  DetectLoopNesting (F);
  summaryRegion->EntryBlock (F->EntryBlock ());
  summaryRegion->DAG() = new RegionGraph ();  

  // add nodes (summary regions) to region DAG
  FOREACH_LOOP_IN_LOOPLIST (L, nestedLoops) 
    summaryRegion->DAG()->AddNode (LoopSummaryRegion (L)); 

  // add nodes (basic regions) to region DAG
  FOREACH_BB_IN_FUNCTION (BB, F)
    if (enclosingRegion[(Node *)BB] == NULL && BB->StartAddress ()) 
      summaryRegion->DAG()->AddNode (BasicRegion (BB));  

  // add edges between nodes of region DAG
  AddEdgesToRegionDAG (summaryRegion);
  
  SummarizeRegion (summaryRegion);   
  return summaryRegion;
}

//! @brief apply processRegion function to each summary region in post-order
void
ProcessRegionHierarchy (Region *summaryRegion, 
			ProcessRegionHook processRegion, int nestingDepth)
{
  FOREACH_NODE_IN_GRAPH (N, summaryRegion->DAG ()) 
    {
      Region *R = (Region *) N;
      if (R->IsSummaryRegion ())
	ProcessRegionHierarchy (R, processRegion, nestingDepth + 1);
    }
  processRegion (summaryRegion, nestingDepth);
  return;
}

void
Region::ComputeDownwardExpDefs ()
{
  if (IsBasicRegion ())
    {
      FOREACH_INS_IN_ORDER_IN_BB (I, entryBlock) 
	{
	  list <Variable*> &Defs = I->VarDefs();
	  downwardExpDefs.insert (Defs.begin (), Defs.end ());  
	}
      Instruction *I = entryBlock->LastInstruction ();
      if (I && I->IsCall ())
	downwardExpDefs.insert (&VarStar);
    }
  else 
    {
      FOREACH_NODE_IN_GRAPH (N, DAG()) 
	{
	  Region *R = (Region *) N;
	  downwardExpDefs.insert 
	    (R->downwardExpDefs.begin (), R->downwardExpDefs.end ());
	}   
    }
  return;
}

void
Region::ComputeUpwardExpUses ()
{
  // TOFIX: ACCURACY (would it help?)
  if (IsBasicRegion ())
    {
      FOREACH_INS_IN_REV_ORDER_IN_BB (I, entryBlock) 
	{
	  list <Variable*> &Defs = I->VarDefs();
	  list <Variable*> &Uses = I->VarUses();

	  for (list<Variable*>::iterator id = Defs.begin ();
	       id != Defs.end (); id++)
	    {
	      set<Variable *>::iterator s = upwardExpUses.find (*id);
	      if (s != upwardExpUses.end ())
		upwardExpUses.erase (s);
	    }	      
	  upwardExpUses.insert (Uses.begin (), Uses.end ());    
	}
    }
  else
    {
      FOREACH_NODE_IN_GRAPH (N, DAG()) 
	{
	  Region *R = (Region *) N;
	  upwardExpUses.insert 
	    (R->upwardExpUses.begin (), R->upwardExpUses.end ());  
	}
    }
  return;
}

// return a bitvector 000...PV[P, P + C]...000
static PathVector*
ProjectPV (PathVector* PV, PathVector *projPV, int N, int P, int C)
{
  int i, j;
  
  for (int i = 0; i < N; i++)
    if ((*PV)[i])
      break;

  while (C && i+P < N)
    {
      (*projPV)[i+P] = (*PV)[i+P];
      C--;
      i++;
    }

  return projPV;
}

void
Region::ComputeBlockPathVectors ()
{
  EnumerateRegionPaths (this);
  Region *entryRegion = OutermostEnclosingRegion ((Node *) entryBlock);
  int numPaths = entryRegion->NumPaths();      
  list<Region*>::iterator M;
  list <Region *>&  nodesInTorder = DAG()->TopologicalSort ();
  
  for (M = nodesInTorder.begin (); M != nodesInTorder.end (); M++)
    {
      Region *R = (Region *) *M;
      R->BPV() = new PathVector (numPaths);
      if (R->BPV ()->size () != numPaths)
	cerr << R->BPV()->size () << " " << numPaths << endl;
      if (R == entryRegion)
	entryRegion->BPV()->set ();
    }  
  for (M = nodesInTorder.begin (); M != nodesInTorder.end (); M++)
    {
      int sPos = 0;
      Region *R = (Region *) *M;
      FOREACH_SUCC_EDGE_IN_NODE (SE, (Node *)R) 
	{
	  Region *S = (Region *) EDGE_TAIL (SE);
	  PathVector PV (numPaths);
	    ProjectPV 
	      (R->BPV(), &PV, numPaths, sPos, S->NumPaths());    
	  if (S->BPV())
	    *S->BPV() |= PV;      
	  sPos += S->NumPaths();
	}
    }    
  return;
}

// @brief: intersects X & Y, result in S. 
// @TODO: fix intersection code (shd be cell-based intersection)
bool
VariableIntersect (set<Variable *> &S,
		   const set<Variable *> &X, const set<Variable *> &Y)
{
  bool DynVarY = false;
  set<Variable *>::iterator siterX, siterY;
  
  for (siterX = X.begin (); siterX != X.end (); siterX++)
    for (siterY = Y.begin (); siterY != Y.end (); siterY++)
      {
	if ((*siterX)->type == DynVar)
	  S.insert (*siterX);
	else if((*siterY)->type == DynVar) 
	  {
	    DynVarY = true;
	    S.insert (*siterY);
	  }
	else if (*siterX == *siterY)
	  S.insert (*siterX);
      }
  return DynVarY;
}

/* DUPV [D,U,V] = Vector */
void Region::ComputeDefUsePathVectors ()
{

  list <Region *>::iterator N, M;
  set<Variable *>::iterator siter;
  Region *entryRegion = OutermostEnclosingRegion ((Node *) entryBlock);
  
  list <Region *> nodesInTorder = DAG ()->TopologicalSort (); 

  for (N = nodesInTorder.begin (); N != nodesInTorder.end (); N++)
    {
      Region *D = (Region *) *N;    
      for (M = N, M++; M != nodesInTorder.end (); M++)
	{
	  Region *U = (Region *) *M;      
	  PathVector P = *D->BPV () & *U->BPV ();  	   
	  if (!P.none ()) 
	    {
	      set<Variable *> vars1;
	      VariableIntersect (vars1, 
				 D->DownwardExpDefs (), U->UpwardExpUses ());
	      for (siter = vars1.begin (); siter != vars1.end (); siter++)
		{
		  Variable *V = *siter;
		  Triple T (D, U, V);
		  rDUPV[T] = P;
		}     	      
	    }
	}
    }
  
  for (N = nodesInTorder.begin () ; N != nodesInTorder.end (); N++)
    {
      Region *D1 = (Region *) *N;
      for (M = N , M++; M != nodesInTorder.end (); M++)
	{
	  Region *D2 = (Region *) *M;     		   
	  PathVector P = *D1->BPV () & *D2->BPV ();
	  if (!P.none ()) 
	    {	  
	      set<Variable *> vars2;
	      bool DynVarU = VariableIntersect (vars2, 
				 D1->DownwardExpDefs (), 
				 D2->DownwardExpDefs ());
	      for (siter = vars2.begin (); siter != vars2.end (); siter++)
		{
		  // DynVar - does may not kill reaching definitions
		  if (DynVarU)
		    continue;
		  FOREACH_NODE_IN_GRAPH (N, DAG ()) 
		    {
		      Region *U = (Region *) N;	 		      
		      if (U == D1 || U == D2)
			continue;
		      PathVector P2 = *D2->BPV () & *U->BPV ();
		      // path D2->U is present
		      if (!P2.none ())
			{
			  Triple T (D1, U, *siter);
			  // kill alongs paths through D2
			  if (rDUPV[T].size ())
			    rDUPV[T] -= *D2->BPV ();
			}
		    } 

		} // for
	    } //  if
	} // for
    } // for
  
  return;
}


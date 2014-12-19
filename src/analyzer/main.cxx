
/*! @file
 *  Main functions of the def-use analyzer for binary code.
 */

#include "diablo.hxx"
#include "region.hxx"
#include "loop.hxx"

#include <string>
#include <fstream>
#include <sstream>
using namespace std;

//! @brief dump loop nesting hierarchy into a output file stream
void DumpLoopNesting (Loop *eLoop, ofstream& loopDump, int depth)
{

 FOREACH_LOOP_IN_LOOPLIST (L, LOOP_CHILDS(eLoop)) 
   DumpLoopNesting (L, loopDump, depth + 1);
 
 loopDump << "[" << depth << ":" <<  eLoop << "]";
 loopDump << "-(" << (void *) BBL_CADDRESS(LOOP_HEADER (eLoop)) << ")";

 LOOP_ITERATOR_INIT (eLoop,loopit);
 FOREACH_BB_IN_LOOP (BB, loopit)
   loopDump << "-" << (void *) BB->StartAddress() << ":" << BB;

 LOOP_ITERATOR_END (eLoop,loopit);
 loopDump << endl;

 return;
}

//! @brief dump loop nesting hierarchy for function
void DumpLoopNesting (Function *F)
{
  ofstream loopDump;
  stringstream fileName;

  fileName << "." << F->Name() << ".loops";   
  loopDump.open (fileName.str().c_str());
 
  LoopList outerLoops = DetectLoopNesting (F);
  FOREACH_LOOP_IN_LOOPLIST (L, outerLoops) 
    DumpLoopNesting (L, loopDump, 1);    

  return;
}

//! @brief dump region graph of a summary region into a file
void DumpRegionGraph (Region* R, int nestingDepth)
{
  ofstream  graphDump;
  stringstream fileName;
  Function *F = R->EntryBlock()->EnclFunction ();
 
  fileName << "." << F->Name() << "." << nestingDepth << "." <<
    (void *) R->StartAddress () << ".vcg";  
  graphDump.open (fileName.str().c_str());

  graphDump << "graph: { title:\"" << (void *) R->StartAddress() << "\"\n";

  FOREACH_NODE_IN_GRAPH(N, R->DAG ()) {
      Region *S = (Region *) N;
      graphDump << 
	"node: { title: " "\"" << (void *) S->StartAddress () << "\"\n }\n"; 
  }

  FOREACH_EDGE_IN_GRAPH (E, R->DAG ()) {
      Region *S = (Region *) EDGE_HEAD (E);
      Region *T = (Region *) EDGE_TAIL (E);
      graphDump << "edge: { sourcename: \"" << (void *) S->StartAddress() << 
	"\" targetname: \"" << (void *) T->StartAddress() <<  "\"}\n";
  }
  graphDump << "}\n";

  return;
}

//! @brief dump summary region def-use variables into a file
void DumpRegionVariables (Region *R, int nestingDepth)
{
  ofstream  varDump;
  stringstream fileName;
  Function *F = R->EntryBlock()->EnclFunction ();
 
  fileName << "." << F->Name() << "." << nestingDepth << "." <<
    (void *) R->StartAddress () << ".var"; 
 
  varDump.open (fileName.str().c_str());

  FOREACH_NODE_IN_GRAPH (N, R->DAG()) {
    Region *R = (Region *) N;
    //if (R->IsBasicRegion ()) 
    {
	varDump << "BasicBlock Address: " 
		<< hex << R->StartAddress () << endl;
      
	set<Variable *>::iterator I;
      
	varDump << "Defs:" ;
	for (I = R->DownwardExpDefs().begin();
	     I != R->DownwardExpDefs().end(); I++)
	  varDump << "(" << (*I)->type << "," << (*I)->addrID << ","	\
		  << (*I)->size << ")";
	varDump << endl;
      
	varDump << "Uses:" ;
	for (I = R->UpwardExpUses().begin();
	     I != R->UpwardExpUses().end();
	     I++)
	  varDump << "(" << (*I)->type << "," << (*I)->addrID << ","  \
		  << (*I)->size << ")";
	varDump << endl;
	
	varDump << endl;     
    }
  }  
  return;
}   

//! @brief dump block-path-vectors of a summary region into a file
void DumpBlockPathVectors (Region *R, int nestingDepth)
{ 
  ofstream bpvDump; 
  stringstream fileName; 
  Function *F = R->EntryBlock()->EnclFunction ();

 
  fileName << "." << F->Name() << "." << nestingDepth << "." <<
    (void *) R->StartAddress () << ".bpv"; 
   
  bpvDump.open (fileName.str ().c_str());

  list<Region*> &nodesInTorder = R->DAG()->TopologicalSort ();
  for (list<Region*>::reverse_iterator N = nodesInTorder.rbegin(); 
       N != nodesInTorder.rend(); N++)
  {
    Region *S = (Region *) *N;
    bpvDump <<  hex << S->StartAddress() << ":" <<  dec <<  S->NumPaths ()
	    << ":";
    if (S->BPV())
      bpvDump << *S->BPV() << endl;  
  }
  
  return;
}
void DumpDefUsePathVectors (Region *R, int nestingDepth)
{
  ofstream dupvDump; 
  stringstream fileName; 
  Function *F = R->EntryBlock()->EnclFunction ();
 
  fileName << "." << F->Name() << "." << nestingDepth << "." <<
    (void *) R->StartAddress () << ".dupv"; 
   
  dupvDump.open (fileName.str ().c_str());

  DefUsePathVector::iterator i;

  for (i = R->DUPV().begin (); i != R->DUPV().end (); i++)
    if (!i->second.none())
    {
      dupvDump << "(" << hex << i->first.D->StartAddress () 
	       << "." << dec << nestingDepth + 
	(i->first.D == i->first.U? 0: 1)
	       << "," << hex << i->first.U->StartAddress ()
	       << "." << dec << nestingDepth +
	(i->first.D == i->first.U? 0: 1)
	       << ", [" << dec << i->first.V->type 
	       <<    "," << hex << i->first.V->addrID 
	       <<    "," << dec << i->first.V->size 
	       <<    "]): " ;
      dupvDump << i->second << endl;
    }
  return;
}


//! @brief cleanup summary region structures
void Cleanup (Region *R, int D)
{
  // @TODO: fix memory leaks!
  list<Region*> &nodesInTorder = R->DAG()->TopologicalSort ();
  for (list<Region*>::reverse_iterator N = nodesInTorder.rbegin(); 
       N != nodesInTorder.rend(); N++)
  {
    Region *S = (Region *) *N;
    if (S->BPV())
      delete S->BPV ();   
    S->BPV() = NULL;
    S->DUPV().clear ();
  }
  //  delete R->DAG();
  //  R->DAG () = NULL;
}

//! @brief calls set of summary region process functions
void ProcessHooks (Region *R, int nestingDepth)
{
  DumpRegionGraph (R, nestingDepth);
  DumpRegionVariables (R, nestingDepth);
  DumpBlockPathVectors (R, nestingDepth);
  DumpDefUsePathVectors (R, nestingDepth);
  Cleanup (R, nestingDepth);
}

int main (int argCount, char **argVector)
{
  DiabloFrameworkInit (argCount, argVector);

  for (int i = 1; i < argCount; i++) 
    {
      if (argVector[i])
	printf ("%s", argVector[i]);
      // @TODO: ignore arguments processed by diablo core 
      if (argVector[i] == NULL || argVector[i][0] == '-') 
	{
	  continue;
	}

      Object object (argVector[i]);
      object.DisAssemble ();
      CFG *iCFG = object.ICFG ();  
      assert (iCFG != NULL);    
      
      FOREACH_FUNCTION_IN_CFG (F, iCFG) 
	{
#ifdef DEBUG
	  cout << "Processing " 
	       << F->Name() << ":" << hex << F->EntryBlock()->StartAddress() 
	       << endl; 	
	  if (strcmp(F->Name(),"main") == 0)
	    cout << "\n";
#endif	
	  DumpLoopNesting (F);
	  Region *regionHierarchy = RegionHierarchy (F);
	  ProcessRegionHierarchy (regionHierarchy, ProcessHooks);      
	}
    }  
  
  DiabloFrameworkEnd ();  
  return 0;
}

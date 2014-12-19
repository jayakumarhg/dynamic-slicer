
#include "diablo.hxx"
#include "cellset.hxx"

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

extern "C" 
{
#include <unistd.h>
} 

struct
{
  Address statement;
  int  instance; 
} 
slicingCriterion;

CFG *iCFG;
ifstream traceData;
ifstream traceControl;

Instruction*
SearchInstruction (Address addr)
{
  FOREACH_FUNCTION_IN_CFG (F, iCFG)
    {
      FOREACH_BB_IN_FUNCTION (BB, F)
	{
	  FOREACH_INS_IN_REV_ORDER_IN_BB (I, BB)
	    {
	      if (I->StartAddress () == addr)
		{
		  return I;
		}
	    }
	}
    }
  // should not be reachable!
  assert (0);
}

Instruction*
LookupInstruction (Address addr)
{
  static Address prevAddr;
  Instruction *prevInstruction = NULL;
  Instruction *foundInstruction = NULL;

  if (prevInstruction)
    {
      if (addr == prevAddr)
	foundInstruction = prevInstruction;  
      else if (prevInstruction->EnclBasicBlock ()->StartAddress () 
	       != prevInstruction->StartAddress ())
	foundInstruction = prevInstruction->PrevInstruction ();
      else
	{
	  FOREACH_PRED_EDGE_IN_BB (predE, prevInstruction->EnclBasicBlock ())
	    {
	      BasicBlock *predB = predE->Source ();
	      if (predB->LastInstruction ()->StartAddress () == addr)
		{
		  foundInstruction = predB->LastInstruction ();
		  break;
		}
	    }
	}
    }
   
  if (foundInstruction == NULL || foundInstruction->StartAddress () != addr)
    foundInstruction = SearchInstruction (addr);

  prevAddr = addr;
  prevInstruction = foundInstruction;
  return foundInstruction;
}

void
VarsUsed(Address addr, CellSet &regsUsed, CellSet &memsUsed, 
	 CellSet &regsDefined, CellSet &memsDefined)
{

  Instruction *I = LookupInstruction (addr);
  list<Variable*>& U = I->VarUses ();
  list<Variable*>::iterator iter;

  regsUsed.Clear ();
  memsUsed.Clear ();
  for (iter = U.begin (); iter != U.end (); iter++)
    {
      Variable *V = *iter;
      Cell C ((unsigned) V->addrID, (unsigned) V->size, 
	      (void *) I->StartAddress ());

      if (V->type == RegVar) 
	{
	  if (I->IsVarUsedBy (V, regsDefined, memsDefined))
	    regsUsed.Insert (C.addr, C.size, C.data);
	}
      else if (V->type == MemVar && V->addrID != 0)
	{
	  if (I->IsVarUsedBy (V, regsDefined, memsDefined))
	    memsUsed.Insert (C.addr, C.size, C.data);
	}
      else
	{
	  traceData.seekg (-1 * (int) sizeof (C.size), ios::cur);
	  assert (!traceData.fail ());	    
	  traceData.read ((char *) &C.size, sizeof (C.size));
	  traceData.seekg (-1 * (int) sizeof (C.size), ios::cur);
	  traceData.seekg (-1 * (int) sizeof (C.addr), ios::cur);
	  assert (!traceData.fail ());
	  traceData.read ((char *) &C.addr, sizeof(C.addr));
	  traceData.seekg (-1 * (int) sizeof (C.addr), ios::cur);
	  if (I->IsVarUsedBy (V, regsDefined, memsDefined))
	    memsUsed.Insert (C.addr, C.size, C.data);
	  // cout << "(R " << (void *) C.addr << "," << C.size << " ) " 
	  //     << (void *) addr << endl;
	}
    }     
  return;
}

void
VarsDefined (Address addr, CellSet& regsDefined, CellSet& memsDefined)
{
  regsDefined.Clear ();
  memsDefined.Clear ();
  Instruction *I = LookupInstruction (addr);
  list<Variable*>& D = I->VarDefs ();
  list<Variable*>::iterator iter;
  for (iter = D.begin (); iter != D.end (); iter++)
    {
      Variable *V = *iter;
      if (V->type == RegVar)
	regsDefined.Insert ((unsigned) V->addrID, (unsigned) V->size, 
			    (void *) I->StartAddress ());
      else if (V->type == MemVar && V->addrID != 0)
	memsDefined.Insert ((unsigned) V->addrID, (unsigned) V->size,
			    (void *) I->StartAddress ());
      else 
	{
	  unsigned size;  Address data;
	  traceData.seekg (-1 * (int) sizeof (size), ios::cur);
	  assert (!traceData.fail ());	    
	  traceData.read ((char *) &size, sizeof (size));
	  traceData.seekg (-1 * (int) sizeof (size), ios::cur);
	  traceData.seekg (-1 * (int) sizeof (data), ios::cur);
	  assert (!traceData.fail ());
	  traceData.read ((char *) &data, sizeof (data));
	  traceData.seekg (-1 * (int) sizeof (data), ios::cur);
	  memsDefined.Insert ( (unsigned) data, size, 
			       (void *) I->StartAddress ());
	  //  cout << "(W " << (void *) data << "," << size << " ) " 
	  //    << (void *) addr << endl;
	}	
    }  
  return;
}

set<Address>&
DynamicSlice ()
{
  Address addr;
  static set<Address> slice;
  static CellSet regsD, memsD, regsU, memsU;
  static CellSet toExplainMems, toExplainRegs;

  traceControl.seekg (0, ios::end);
  traceData.seekg (0, ios::end);
  
  while (true)
    {
      traceControl.seekg (-1 * (int) sizeof (addr), ios::cur);
      if (traceControl.fail ())
	break;
      traceControl.read ((char *) &addr, sizeof (addr));
      if (addr == slicingCriterion.statement)
	slicingCriterion.instance++;      
      VarsDefined (addr, regsD, memsD);
      VarsUsed (addr, regsU, memsU, regsD, memsD);
      traceControl.seekg (-1 * (int) sizeof (addr), ios::cur); 
      if (slicingCriterion.instance == 1)
	{
	  toExplainRegs.Insert (regsU);
	  toExplainMems.Insert (memsU);
	  break;
	}	
    }

  if (traceControl.fail ())
    return slice;

  while (true)
    {
      list<void *> cause;
      traceControl.seekg (-1 * (int) sizeof (addr), ios::cur);
      if (traceControl.fail ())
	break;
      traceControl.read ((char *) &addr, sizeof (addr));      
      VarsDefined (addr, regsD, memsD);
      bool rD = toExplainRegs.SubtractIfIntersecting (regsD, cause);
      bool mD = toExplainMems.SubtractIfIntersecting (memsD, cause);
      VarsUsed (addr, regsU, memsU, regsD, memsD);      
      if (rD || mD) 
	{
	  cerr << LookupInstruction (addr)->StringOut() << "::" << "{";
	  for (list<void *>::iterator iter = cause.begin ();
	       iter != cause.end (); iter++)
	    cerr << (void *) (*iter) << " ";
	  cerr << "}" << endl;
	  toExplainRegs.Insert (regsU);
	  toExplainMems.Insert (memsU);
	  slice.insert (addr);	  
	}
      traceControl.seekg (-1 * (int) sizeof (addr), ios::cur);
    }

  return slice;
}

void
Usage (char *progName)
{
  cerr << "Usage: " << progName << " -S <address> [-i <integer>] -T <path>" 
       << endl;
}  

void
RemoveNullOptions (int &argCount, char **&argVector)
{
  int index;
  int nonNullCount = 0;
  char *nonNullArgs[argCount];
  
  for (index = 0; index < argCount; index++)
    {
      if (argVector[index])
	nonNullArgs[nonNullCount++] = argVector[index];
    }

  for (argCount = 0; argCount < nonNullCount; argCount++)
    argVector[argCount] = nonNullArgs[argCount];
  
}

int 
main (int argCount, char **argVector)
{
  int option;
  string traceDataFile;
  string traceControlFile;


  DiabloFrameworkInit (argCount, argVector);

  RemoveNullOptions (argCount, argVector);

  while ((option = getopt (argCount, argVector, "t:S:i:")) != -1)
    switch (option)
      {
      case 'S':
	slicingCriterion.statement = (Address) strtol (optarg, NULL, 0);
	break;
      case 'i':
	slicingCriterion.instance = strtol (optarg, NULL, 0);
	break;
      case 't':
	traceDataFile = optarg;
	traceControlFile = optarg;
	traceDataFile.append ("/.trace.data");
	traceControlFile.append ("/.trace.control");
	traceData.open (traceDataFile.c_str ());
	traceControl.open (traceControlFile.c_str ());
	break;	
      case '?':
	cerr << "option -" << optopt << "missing an argument.\n";
	Usage (argVector[0]);	
	return 1;
      default:
	cerr << "unknown option -" << optopt << "\n";
	Usage (argVector[0]);
	return 1;	
      }

  if ((slicingCriterion.statement == 0) || (optind != argCount - 1))
    {
      cerr << "incorrect number of arguments" << endl;
      Usage (argVector[0]);
      return 1;
    }

  if (!traceData || !traceControl)
    {
      cerr << "could not find .trace.data and .trace.control in given path\n";
      return 1;
    }

  Object object (argVector[optind]);
  object.DisAssemble ();
  iCFG = object.ICFG ();
  assert (iCFG != NULL);

  set<Address> &slice = DynamicSlice ();
  set<Address>::iterator iter;
  
  cout << "{ ";
  for (iter = slice.begin (); iter != slice.end (); iter++)
    cout << (void *) (*iter) << " ";
  cout << " }";
  cout << endl; 
   
  DiabloFrameworkEnd ();
}

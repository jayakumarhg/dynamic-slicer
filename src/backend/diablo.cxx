
#include <map>
#include <list>
#include <fstream>
using namespace std;

#include <ext/hash_set>
using namespace __gnu_cxx;


#include "diablo.hxx"

//! @brief initialize flowgraph & object framework
void DiabloFrameworkInit (int argc, char **argv)
{ 
 DiabloFlowgraphInit (argc, argv);

#ifdef DIABLOFLOWGRAPH_ARMSUPPORT
  DiabloArmInit (argc, argv);
#endif

#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  DiabloI386Init (argc, argv);
#endif

#ifdef DIABLOFLOWGRAPH_AMD64SUPPORT
  DiabloAmd64Init (argc, argv);
#endif

  return;
}

//! @brief close & cleanup flowgraph & object framework
void DiabloFrameworkEnd ()
{
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  DiabloI386Fini ();
#endif

#ifdef DIABLOFLOWGRAPH_ARMSUPPORT
  DiabloArmFini ();
#endif

#ifdef DIABLOFLOWGRAPH_AMD64SUPPORT
  DiabloAmd64Fini ();
#endif

  DiabloFlowgraphFini ();
  DiabloBrokerFini ();

#ifdef DEBUG_MALLOC
  diablosupport_options.verbose++;
  PrintRemainingBlocks ();
#endif
  return;
}

void CFG::Reduce ()
{
  CfgRemoveDeadCodeAndDataBlocks (this);
  
  if (CfgPatchForNonReturningFunctions (this))
  CfgRemoveDeadCodeAndDataBlocks (this);
    
  CfgPatchToSingleEntryFunctions (this);
  CfgRemoveDeadCodeAndDataBlocks (this);
  return;
}

void CFG::ComputeDominators ()
{
  //Reduce ();
  //if(dominator_info_correct == FALSE) 
    //ComDominators (this);
  return;
}

Object::Object (char *filename)
{
  oname = filename;
  if (diabloobject_options.restore 
      || (diabloobject_options.restore_multi != -1)) 
    filename = RestoreDumpedProgram ();
  
  object = LinkEmulate (filename, FALSE);
  iCFG = NULL;
}

Object::~Object ()
{
  //ObjectDeflowgraph (object);
  //ObjectRebuildSectionsFromSubsections (object);
  //ObjectAssemble (object);
  //ObjectWrite (object, StringConcat2(oname,"-diablo"));
}

void Object::DisAssemble ()
{
   if (OBJECT_MAPPED_FIRST(object))
     ObjectMergeCodeSections (object);
  ObjectDisassemble (object);  
  ObjectFlowgraph (object, NULL, NULL);
  iCFG = (CFG *) SECTION_CFG (OBJECT_CODE (object)[0]);
  iCFG->Reduce ();
  return;
}

CFG *Object::ICFG()
{  
  return iCFG;
}

t_ins*
Instruction::Original ()
{
  t_ins *org;
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  org = this;
  t_i386_ins *i386_ins = (t_i386_ins *) this;
  
  if (I386_INS_AP_ORIGINAL (i386_ins))
    org = (t_ins *) I386_INS_AP_ORIGINAL (i386_ins);
#endif
  return org;
}

bool
Instruction::IsCall ()
{
  bool iscall = false;
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  t_i386_ins *i386_ins = (t_i386_ins *) this;

  switch (I386_INS_OPCODE (i386_ins))
    {
    case I386_CALL:
    case I386_CALLF:
    case I386_INT:
    case I386_INTO:
    case I386_INT3:
    case I386_SYSENTER:
      iscall = true;
      break;
    }
#endif
  return iscall;
}

   

  


/************************Def-Use Variables in BasicBlock**********************/

//! @struct variable key equality operator
struct varEqual
{
  bool operator()(Variable *V1, Variable *V2) const
  {
    return (V1->addrID == V2->addrID) && (V1->size == V2->size);
  }
};

//! @struct variable key hashing function
struct varHashFcn
{
  size_t operator() (Variable *V) const
  {
    return  (size_t) (V->addrID);
  }
};

//! @brief special variable that represents address computed dynamically
Variable VarStar = {0,0,0};

//! @brief variable symbol hash table
static hash_set <Variable *, varHashFcn, varEqual> varTable[4];

//! @brief loop's up the given variable in variable symbol table
Variable *LookupVar (int type, int addrID, int size)
{
  Variable *nV;
  Variable V = {addrID, type, size};
  hash_set <Variable *, varHashFcn, varEqual>::iterator it;

  if (V.type == DynVar)
    return &VarStar;

  it =  varTable[V.type].find (&V);
  if (it == varTable[V.type].end()) 
    {      
      nV = new Variable;   
      *nV = V;
      varTable[V.type].insert (nV);
    }
  else 
    {
      nV = *it;
    }
  
  return nV;   
}


#ifdef DIABLOFLOWGRAPH_I386SUPPORT
void
I386_AddOperandVars (t_i386_operand *op, list<Variable*> &vars)
{
  static Variable V;
  
  if (I386_OP_TYPE(op) == i386_optype_reg)
    {
      V.type = RegVar;
      V.addrID = (int) I386_OP_BASE(op) * 4;
      switch (I386_OP_REGMODE(op)) 
	{
	case i386_regmode_full32: V.size = 4; break;
	case i386_regmode_lo16:   V.size = 2; V.addrID += 2; break;
	case i386_regmode_lo8:    V.size = 1; V.addrID += 3; break;
	case i386_regmode_hi8:    V.size = 1; V.addrID += 2; break;
	case i386_regmode_invalid: break;
	}
      vars.push_front (LookupVar (V.type, V.addrID, V.size));        
    }
  else if (I386_OP_TYPE(op) == i386_optype_mem) 
    {
      V.size = I386_OP_MEMOPSIZE (op);
      if (I386_OP_BASE (op) == I386_REG_EBP && 
	  I386_OP_INDEX (op) == I386_REG_NONE) 
	{
	  V.type = StackVar;
	  V.addrID = (int) I386_OP_IMMEDIATE (op);
	}
      else 
	{
	  V.type = DynVar;
	  V.addrID = 0;
	  V.size = 0;
	}      
      vars.push_front (LookupVar (V.type, V.addrID, V.size));
    }
  else if (I386_OP_TYPE(op) == i386_optype_farptr) 
    {
      V.size = I386_OP_MEMOPSIZE (op);
      if (I386_OP_SEGSELECTOR(op) == I386_REG_DS) 
	{
	  V.type = MemVar;
	  V.addrID = (int) I386_OP_IMMEDIATE(op);
	}
      else 
	{
	  V.type = DynVar;
	  V.addrID = 0;
	  V.size = 0;
	}
	vars.push_front (LookupVar (V.type, V.addrID, V.size));
    }
  return;
}
#endif

#ifdef DIABLOFLOWGRAPH_I386SUPPORT
void
I386_AddImplicitVarDefs (t_i386_ins *ins, list<Variable*> &varDefs)
{
  switch (I386_INS_OPCODE (ins))
    {
    case I386_PUSH:
    case I386_PUSHA:
    case I386_PUSHF:
      varDefs.push_front (LookupVar (DynVar, 0, 0));
      // @NOTBUG fall-through
    case I386_POP:
    case I386_POPF:
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      break;

    case I386_POPA:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EBX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESI * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDI * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));
      break;

    case I386_MOVSB:
    case I386_MOVSD:
    case I386_CMPSB:
    case I386_CMPSD:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDI * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESI * 4, 4));
      break;

    case I386_LODSB:
    case I386_LODSD:
    case I386_OUTSB: 
    case I386_OUTSD:
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESI * 4, 4));
      break;
      
    case I386_STOSB: 
    case I386_STOSD:
    case I386_SCASB:
    case I386_SCASD:
    case I386_INSB: 
    case I386_INSD:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDI * 4, 4));
      break;

    case I386_CALL:
    case I386_CALLF:
      varDefs.push_front (LookupVar (DynVar, 0, 0));
      // @NOTBUG fall-through
    case I386_RET:
    case I386_RETF:
    case I386_IRET:
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      break;

    case I386_ENTER:
      varDefs.push_front (LookupVar (DynVar, 0, 0));
      // @NOTBUG fall-through
    case I386_LEAVE:
      varDefs.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));      
      break;  

    case I386_MUL:
    case I386_IMUL:
    case I386_DIV:
    case I386_IDIV:
      if (I386_OP_REGMODE(I386_INS_DEST(ins)) == i386_regmode_full32 || 
	  I386_OP_REGMODE(I386_INS_DEST(ins)) == i386_regmode_lo16 )
	varDefs.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      break;

    case I386_FSTSW:
    case I386_CMPXCHG:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      break;

    case  I386_PSEUDOCALL:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      break;
     
      /*
    case I386_INT:
    case I386_INTO:
    case I386_INT3:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      break;
      */
    case I386_LOOP:
    case I386_LOOPZ:
    case I386_LOOPNZ:
      varDefs.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      break;

    case I386_CPUID:
      varDefs.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EBX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      varDefs.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      break;
    }

  return;
}

void
I386_AddImplicitVarUses (t_i386_ins *ins, list<Variable *> &varUses)
{
  switch (I386_INS_OPCODE (ins))
   {
    case I386_PUSHA:
      varUses.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_ESI * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EDI * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));      
      // @NOTBUG fall-through
    case I386_PUSH:
    case I386_PUSHF:	       
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      break;

    case I386_POP:
    case I386_POPA:
    case I386_POPF:
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varUses.push_front (LookupVar (DynVar, 0, 0));
      break;

    case I386_CALL:
    case I386_CALLF:
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      break;

    case I386_RET:
    case I386_RETF:
    case I386_IRET:
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varUses.push_front (LookupVar (DynVar, 0, 0));     
      break;

    case I386_LEAVE:
      varUses.push_front (LookupVar (DynVar, 0, 0));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));
      break;
      
    case I386_ENTER:
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));
      break;

    case I386_DIV:
    case I386_IDIV:
      if (I386_OP_REGMODE (I386_INS_DEST(ins)) == i386_regmode_full32 ||
	   I386_OP_REGMODE(I386_INS_DEST(ins)) == i386_regmode_lo16 )
	varUses.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));

      /*
    case I386_INT:
    case I386_INTO:
    case I386_INT3:
    case I386_SYSENTER:
    case I386_SYSEXIT:
      varUses.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EDX * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_ESI * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EDI * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_ESP * 4, 4));
      varUses.push_front (LookupVar (RegVar, I386_REG_EBP * 4, 4));
      break;
      */

    case I386_JECXZ:
    case I386_LOOP:
    case I386_LOOPZ:
    case I386_LOOPNZ:
      varUses.push_front (LookupVar (RegVar, I386_REG_ECX * 4, 4));
      break;
      
    case I386_CMPXCHG:
      varUses.push_front (LookupVar (RegVar, I386_REG_EAX * 4, 4));
      break;
    }
 return;
}

inline void 
I386_AddOperandVarDefs (t_i386_operand *op, list<Variable *> &varDefs)
{
  I386_AddOperandVars (op, varDefs);
}

inline void
I386_AddOperandVarUses (t_i386_operand *op, list<Variable *> &varUses, 
			bool only_subops = false)
{  
  if (!only_subops)
    I386_AddOperandVars (op, varUses);
  switch (I386_OP_TYPE (op))
    {
    case i386_optype_mem:
      if (I386_OP_BASE (op) != I386_REG_NONE)
	varUses.push_front (LookupVar (RegVar, I386_OP_BASE (op) * 4, 4));
      if (I386_OP_INDEX (op) != I386_REG_NONE)
	varUses.push_front (LookupVar (RegVar, I386_OP_INDEX (op) * 4, 4));
      break;
    case i386_optype_farptr:
      if (I386_OP_SEGSELECTOR (op) != I386_REG_NONE)
	varUses.push_front (LookupVar 
			    (RegVar, I386_OP_SEGSELECTOR (op) * 4, 4));
      break;
    }      
}
#endif

list<Variable *> & 
Instruction::VarDefs()
{
  list<Variable *>& varDefs = *(new list<Variable *>);
  
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  t_i386_ins *ins = (t_i386_ins *) this;
  if (I386_INS_AP_ORIGINAL (ins))
    ins = I386_INS_AP_ORIGINAL (ins);

  if (I386_INS_DEST(ins))
    I386_AddOperandVarDefs (I386_INS_DEST (ins), varDefs);
 
  if (I386_INS_SOURCE1(ins) && I386_INS_HAS_FLAG (ins, I386_IF_SOURCE1_DEF)) 
    I386_AddOperandVarDefs (I386_INS_SOURCE1 (ins), varDefs);

  if (I386_INS_SOURCE2(ins) && I386_INS_HAS_FLAG(ins, I386_IF_SOURCE2_DEF))
    I386_AddOperandVarDefs (I386_INS_SOURCE2 (ins), varDefs);

  I386_AddImplicitVarDefs (ins, varDefs);
#endif

  return varDefs;
}

list<Variable *>&  
Instruction::VarUses()
{
  list<Variable *>& varUses = *(new list<Variable *>);
 
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  t_i386_ins * ins = (t_i386_ins *) this;
  if (I386_INS_AP_ORIGINAL (ins))
   ins = I386_INS_AP_ORIGINAL (ins);
  
  if (I386_INS_SOURCE1(ins))
    I386_AddOperandVarUses (I386_INS_SOURCE1 (ins), varUses,
			    I386_INS_OPCODE(ins) == I386_LEA);

  if (I386_INS_SOURCE2(ins))
    I386_AddOperandVarUses (I386_INS_SOURCE2 (ins), varUses);

  if (I386_INS_DEST(ins))
    I386_AddOperandVarUses (I386_INS_DEST(ins), varUses, 
			    !I386_INS_HAS_FLAG (ins, I386_IF_DEST_IS_SOURCE));

  I386_AddImplicitVarUses (ins, varUses);
#endif
 
  return varUses;
}

bool
Instruction::IsVarUsedBy (Variable *V, CellSet &regsD, CellSet &memsD)
{
#ifdef DIABLOFLOWGRAPH_I386SUPPORT
  t_i386_ins * ins = (t_i386_ins *) this;
  if (I386_INS_AP_ORIGINAL (ins))
   ins = I386_INS_AP_ORIGINAL (ins);

  list<Cell> regsDCells = regsD.Cells ();
  list<Cell> memsDCells = regsD.Cells ();

  list<Cell>::iterator regsDI = regsDCells.begin ();
  list<Cell>::iterator memsDI = memsDCells.begin ();
  
  switch (I386_INS_OPCODE (ins))
    {
    case I386_PUSH:
    case I386_PUSHA:
    case I386_PUSHF:
    case I386_CALL:
    case I386_CALLF:
      if ((memsDI == memsDCells.end ())  && 
	  !(V->type == RegVar && V->addrID == I386_REG_ESP * 4))
	return false;
      break;

    case I386_POP:
    case I386_POPA:
    case I386_POPF:
    case I386_RET:
    case I386_RETF:
    case I386_IRET:
      if (regsDI->addr == I386_REG_ESP * 4 && (regsDI++) == regsDCells.end ())
	if (!(V->type == RegVar && V->addrID == I386_REG_ESP * 4))
	  return false;
      break;

    case I386_ENTER:
      if ((memsDI == memsDCells.end ()) &&
	  !(V->type == RegVar && V->addrID == I386_REG_ESP * 4))
	return false;
      break;
	

    case I386_LEAVE:
      if (regsDI->addr == I386_REG_ESP * 4 && (regsDI++) == regsDCells.end ())
	if (!(V->type == RegVar && V->addrID == I386_REG_EBP * 4))
	  return false;
      break;

    case I386_XOR:
      if (V->type == RegVar && regsDI != regsDCells.end () && 
	  regsDI->addr == V->addrID)
	return false;
      break;
    default:
      break;
    }
#endif
  return true;
}



/*****************************Debug Functions*********************************/

#ifdef DEBUG
void ExportCFG (Function *F, char *fileName)
{
  ofstream cfgFile;
  cfgFile.open (fileName);	
  
  cfgFile << "graph: { title: \"" << "split" << "\"\n";
  
  FOREACH_BB_IN_FUNCTION (BB,F)
    {
      if (BB->StartAddress () == 0) 
	continue;

      cfgFile << "node: { title: \"" << BB << "\"\n";
      cfgFile << " label: \"\n";
      cfgFile << (void *) BB->StartAddress () << "\n";  
      cfgFile << "\"}\n";

      FOREACH_SUCC_EDGE_IN_BB (succE, BB) 
	{       
	  BasicBlock *succBB = succE->TargetIntraProc ();
	  
	  if (succBB == NULL || succBB->IsReturnBlock ()) 
	    continue;
	  
	  cfgFile << "edge: { sourcename: \"" << BB << "\"\n";
	  cfgFile << "targetname: \"" << succBB << "\"\n";
	  cfgFile << "label: \"edge " << succE << "\"\n}\n"; 
	}
    }

  cfgFile << "}\n";
}
#endif




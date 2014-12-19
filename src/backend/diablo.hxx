/*! 
 *  @file A C++ Wrapper for Diablo Flowgraph Framework!  
 */

#ifndef __DIABLO_WRAPPER_HXX
#define __DIABLO_WRAPPER_HXX

#include <set>
#include <list>
#include "cellset.hxx"

extern "C" {
#include <assert.h>
#include <stdio.h>

// @HACK: re-define indentifiers in diablo headers that clash with C++ keywords
#define new _diablo_c_new
#define operator _diablo_c_operator

#define DIABLOFLOWGRAPH_INTERNAL
#include <diabloflowgraph.h>

#ifdef DIABLOFLOWGRAPH_ARMSUPPORT
#include <diabloarm.h>
#endif 

#ifdef DIABLOFLOWGRAPH_I386SUPPORT
#include <diabloi386.h>
#endif 

#ifdef DIABLOFLOWGRAPH_AMD64SUPPORT
#include <diabloamd64.h>
#endif

void LoopRefAppendSorted (t_loopref * insert, t_loopref ** head);
void BackEdgeAppendSorted (t_back_edge * insert, t_back_edge ** head);

// @HACK: un-define indentifiers in diablo headers that clash with C++ keywords
#undef new
#undef operator
}

// type naming made consistent!
typedef t_node Node;
typedef t_loop Loop;
typedef t_address Address;
typedef t_loopref* LoopList;
typedef t_loopiterator LoopIterator;


/************************* Macro definitions *********************************/
//! @def CFG edge's head node which belongs to same procedure as its tail
#define CFG_EDGE_HEADP(e)						\
  (CFG_EDGE_IS_BACKWARD_INTERPROC(e)              ?			\
   (CFG_EDGE_CORR(e)                ?					\
    CFG_EDGE_HEAD(CFG_EDGE_CORR(e)) : NULL)       :			\
   (CFG_EDGE_HEAD(e)))

//! @def CFG edge's tail node which belongs to same procedure as its head
#define CFG_EDGE_TAILP(e)						\
  (CFG_EDGE_IS_FORWARD_INTERPROC(e)               ?			\
   (CFG_EDGE_CORR(e)                ?					\
    CFG_EDGE_TAIL(CFG_EDGE_CORR(e)) : NULL)       :			\
   (CFG_EDGE_TAIL(e)))



/************************ Wrapper Class Declarations *************************/

class Edge: public t_edge
{
public:
  Node *Source () { return (Node *) EDGE_HEAD (this); }
  Node *Target () { return (Node *) EDGE_TAIL (this); }
};

//! @class generic graph class based on diablo::t_graph
template <class NodeT>
class Graph
{
private:
  t_graph *G;  
  std::list<NodeT *> nodesInTorder;    
  void dfsTopologicalSort (NodeT *N);
  
public:  
   Graph () { G = GraphNew (sizeof(Node), sizeof(Edge)); }
  ~Graph ();

  t_graph *OrgGraph() { return G; }
  bool Contains(NodeT *N);

  void AddNode (NodeT *N) {GraphInitNode(G, N);}
  void AddEdge (NodeT *N1, NodeT *N2) { GraphNewEdge(G, N1, N2, 1); }
  void UnlinkNode (NodeT *N)   { GraphUnlinkNode (G, N); }
  void UnlinkEdge (Edge *E) { GraphUnlinkEdge (G, E); }
  void Dirtied () { nodesInTorder.clear (); }

  static NodeT* CopyNode (NodeT *N);
  std::list<NodeT *>& TopologicalSort ();
  NodeT* EntryNode (); 
}; 

enum { DynVar = 0, RegVar = 1, StackVar = 2, MemVar = 3};
typedef struct _Variable
{
  unsigned addrID;
  unsigned type: 4;
  unsigned size: 4;
} Variable;
//! @brief variable corressponding to indirect memory access
extern Variable VarStar;


// forward definition
class Function;
class BasicBlock;

//! @class C++ wrapper for diablo::t_ins
class Instruction: public t_ins
{
  t_ins *Original ();      
public:
  std::list<Variable*>& VarDefs();
  std::list<Variable*>& VarUses();
  bool IsVarUsedBy (Variable *, CellSet &, CellSet &);
  Address StartAddress () { return INS_OLD_ADDRESS (this); }
  BasicBlock* EnclBasicBlock () { return (BasicBlock *) INS_BBL (this); }
  Instruction* PrevInstruction () { return (Instruction *) INS_IPREV(this); }
  bool IsCall (); 
  char *StringOut () { return StringIo ("@I", Original ());   }
};

//! @class C++ wrapper for diablo::t_bbl
class BasicBlock: public t_bbl
{
public:
  // @TOFIX
  bool IsReturnBlock ()      { return StartAddress () == 0; }
  bool IsMarked()            { return BblIsMarked (this)? true: false; }
  void Mark()                { BblMark(this); }
  Instruction *FirstInstruction() {return (Instruction *) BBL_INS_FIRST(this);}
  Instruction *LastInstruction() {return (Instruction *) BBL_INS_LAST(this);}  
  Address StartAddress()     { return BBL_OLD_ADDRESS(this); }
  Function *EnclFunction ()  { return (Function *) BBL_FUNCTION (this); }
};

//! @class C++ wrapper for diablo::t_cfg_edge
class CfgEdge: public t_cfg_edge
{
public:
  BasicBlock *Source() { return (BasicBlock *) CFG_EDGE_HEAD (this); }
  BasicBlock *Target() { return (BasicBlock *) CFG_EDGE_TAIL (this); }
  BasicBlock *SourceIntraProc () {return (BasicBlock *) CFG_EDGE_HEADP (this);}
  BasicBlock *TargetIntraProc () {return (BasicBlock *) CFG_EDGE_TAILP (this);}
  bool IsMarked() { return CfgEdgeIsMarked (this)? true: false; }
  void Mark ()    { return CfgEdgeMark (this); }
};

//! @class C++ wrapper for diablo:t_function
class Function: public t_function
{
public:
  char *Name ()             { return FUNCTION_NAME (this); }
  BasicBlock *EntryBlock () { return (BasicBlock *)FUNCTION_BBL_FIRST (this); }
};

//! @class C++ wrapper for diablo:t_cfg
class CFG: public t_cfg
{
public:
  void Reduce ();
  void ComputeDominators ();  
};

//! @class C++ wrapper for diablo:t_object   
class Object
{
private:
  char *oname;
  t_object *object;
  CFG *iCFG;

public:
  Object(char *filename);  
  ~Object();              // @TOFIX leak?
  void DisAssemble();   
  CFG *ICFG(); 
};

/*****************************Graph Template Definiton**********************/

template <class NodeT>
Graph<NodeT>::~Graph () 
{      
  NodeT *N = (NodeT *) GRAPH_NODE_FIRST(G);
  while (N != NULL) 
    {
      NodeT * Nxt = (NodeT *) NODE_NEXT(N);
      delete (N);
      N = Nxt;
    }
  Edge *E = (Edge *) GRAPH_EDGE_FIRST(G);
  while (E != NULL)
    {
      Edge *Nxt = EDGE_NEXT(E);
      Free (E);
      E = Nxt;
    }
  Free (G);
}   

template <class NodeT>
bool Graph<NodeT>::Contains(NodeT *N) 
{   
   for(NodeT *NODE = (NodeT *) GRAPH_NODE_FIRST(G);
       NODE != NULL; NODE = (NodeT *) NODE_NEXT(NODE))      
     if (NODE == N)
       return true;
   return false;
}

template <class NodeT>
NodeT* Graph<NodeT>::CopyNode (NodeT *N) 
{ 
  NodeT *newN = new NodeT;
  *newN = *N;
  NODE_SET_NEXT (newN, NULL);
  NODE_SET_PREV (newN, NULL);
  NODE_SET_MARKED_NUMBER (newN, 0);
  NODE_SET_PRED_FIRST (newN, NULL);
  NODE_SET_PRED_LAST (newN, NULL);
  NODE_SET_SUCC_FIRST (newN, NULL);
  NODE_SET_SUCC_LAST (newN, NULL);
  return newN;
}

// @TOFIX interface memorizing the sort
template <class NodeT>
std::list<NodeT *>&  Graph<NodeT>::TopologicalSort ()
{
  if (nodesInTorder.empty() == false)
    return nodesInTorder;
  
  NodeMarkInit ();        
  for(NodeT *N = (NodeT *) GRAPH_NODE_FIRST(G);
      N != NULL; N = (NodeT *) NODE_NEXT(N)) 
    {
      if (!NodeIsMarked ((Node *) N))
	dfsTopologicalSort (N);
    }

  return nodesInTorder;
}

template <class NodeT>
NodeT* Graph<NodeT>::EntryNode ()
{
  for(NodeT *N = (NodeT *) GRAPH_NODE_FIRST(G);
      N != NULL; N = (NodeT *) NODE_NEXT(N))
    {
      if (!NODE_PRED_FIRST (N))
	return N;
    }
  return NULL;
}

template <class NodeT>
void Graph<NodeT>::dfsTopologicalSort (NodeT *N)
{
  NodeMark (N);    
  for (Edge *E = (Edge *) NODE_SUCC_FIRST((Node*) N);
       E != NULL; E = (Edge *) EDGE_SUCC_NEXT (E))
    {
      NodeT *M = (NodeT *) EDGE_TAIL (E);
      if (!NodeIsMarked ((Node *) M))
	dfsTopologicalSort (M);
    }  
  nodesInTorder.push_front (N);
  return;
}

/*************************** Function Declaration *********************/

void DiabloFrameworkInit (int argCount, char **argVector);
void DiabloFrameworkEnd ();
void ExportCFG (Function *F, char *fileName);

/**************************** Useful Iterators ***********************/


//! @def CFG function iterator
#define FOREACH_FUNCTION_IN_CFG(func,cfg) 			  \
  for (Function *func = (Function *) CFG_FUNCTION_FIRST(cfg);	  \
       func != NULL;		                                  \
       func = (Function *) FUNCTION_FNEXT(func))

//! @def loop basic block iterator
#define FOREACH_BB_IN_LOOP(bb,it)				  \
  for (BasicBlock *bb = (BasicBlock*) LoopGetNextBbl(it);	  \
       bb != NULL;                                                \
       bb = (BasicBlock *) LoopGetNextBbl(it)) 

//! @def basic block successor CFG edge iterator
#define FOREACH_SUCC_EDGE_IN_BB(e,node)		       \
  for (CfgEdge *e = (CfgEdge*) BBL_SUCC_FIRST(node);   \
       e != NULL;				       \
       e = (CfgEdge *) CFG_EDGE_SUCC_NEXT(e))

//! @def basic block predessor CFG edge iterator
#define FOREACH_PRED_EDGE_IN_BB(e,node)		       \
  for (CfgEdge *e = (CfgEdge*) BBL_PRED_FIRST(node);   \
       e != NULL;				       \
       e = (CfgEdge *) CFG_EDGE_PRED_NEXT(e))

//! @def node successor edge iterator
#define FOREACH_SUCC_EDGE_IN_NODE(e,node)	      \
  for (Edge *e = (Edge *) NODE_SUCC_FIRST(node);      \
       e != NULL;				      \
       e = (Edge *) EDGE_SUCC_NEXT (e))

//! @def node predecessor edge iterator
#define FOREACH_PRED_EDGE_IN_NODE(e,node)	      \
  for (Edge *e = (Edge *) NODE_PRED_FIRST(node);      \
       e != NULL;				      \
       e = (Edge *) EDGE_PRED_NEXT (e))


//! @def looplist loop iterator
#define FOREACH_LOOP_IN_LOOPLIST(l,it)		\
  for (Loop *l = NULL;				\
       it != NULL && (l = it->loop, 1);		\
       it = it->next)                 

//! @def function basic block iterator
#define FOREACH_BB_IN_FUNCTION(bb,func)					\
  for (BasicBlock *bb = (BasicBlock *) FUNCTION_BBL_FIRST(func);	\
       bb != NULL;							\
       bb = (BasicBlock *) BBL_NEXT_IN_FUN(bb))

//! @def graph node iterator
#define FOREACH_NODE_IN_GRAPH(nd,G)				\
  for (Node* nd = (Node *) GRAPH_NODE_FIRST(G->OrgGraph());	\
       nd != NULL;						\
       nd = NODE_NEXT(nd))

//! @def graph edge iterator
#define FOREACH_EDGE_IN_GRAPH(e,G)					\
  for (Edge* e = (Edge *) GRAPH_EDGE_FIRST(G->OrgGraph());		\
       e != NULL;							\
       e = (Edge *) EDGE_NEXT(e))

//! @def block instruction forward iterator
#define FOREACH_INS_IN_ORDER_IN_BB(ins,bb)				\
  for (Instruction *ins = (Instruction *) BBL_INS_FIRST(bb);		\
       ins != NULL;							\
       ins = (Instruction *) INS_INEXT(ins))

//! @def block instruction reverse iterator
#define FOREACH_INS_IN_REV_ORDER_IN_BB(ins,bb)				\
  for (Instruction *ins = (Instruction *)BBL_INS_LAST(bb);		\
       ins != NULL;							\
       ins = (Instruction *) INS_IPREV(ins))


//! @def loop iterator initializer
#define LOOP_ITERATOR_INIT(loop,it) t_loopiterator *it = LoopNewIterator(loop)
//! @def loop iterator destructor
#define LOOP_ITERATOR_END(loop,it) (Free(it))

#endif






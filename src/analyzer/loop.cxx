/*! 
 *  @file Loop detection & nesting functions
 */

#include <map>
#include <list>
using namespace std;

#include "diablo.hxx"
#include "loop.hxx"

//! @brief adds a basic block to the loop members in Judy array.
static t_bool loopAddBB (t_loop * loop, t_bbl * bbl)
{
  Word_t dummy = 1;
  Pvoid_t pdummy = &dummy;

  JLI(pdummy, loop->bbl_array, (Word_t) bbl);
  if (* (PWord_t) pdummy == 0)
    {
      * (PWord_t) pdummy = (Word_t) bbl;
      LOOP_COUNT(loop)++;
      return TRUE;
    }
  else
    return FALSE;
}

//! @brief merges loop_b into  loop_a
void mergeLoop (t_loop * loop_b, t_loop * loop_a)
{ 
  Pvoid_t dummy;
  Word_t index = 0;
  t_back_edge *new_backedge;

  if (loop_a == loop_b)
    FATAL(("Merging the same loop!"));

  if (LOOP_BACKEDGES(loop_b)->next)
    VERBOSE(1, ("loop_b already has multiple backedges"));

  new_backedge = (t_back_edge *) Malloc (sizeof (t_back_edge));
  new_backedge->edge = LOOP_BACKEDGES(loop_b)->edge;
  new_backedge->has_corr = LOOP_BACKEDGES(loop_b)->has_corr;
  new_backedge->next = LOOP_BACKEDGES(loop_a);
  LOOP_BACKEDGES(loop_a) = new_backedge;

  JLF(dummy, loop_b->bbl_array, index);

  while (dummy != NULL)
    {
      loopAddBB (loop_a, (t_bbl *) index);
      JLN(dummy, loop_b->bbl_array, index);
    } 
}

//! @return  element in loop_list (if exists) which has same header as loop
t_loop* loopWithSameHeaderExists (t_loopref *loop_list, t_loop *loop)
{    
  while (loop_list != NULL) 
    {    
      if (loop != loop_list->loop)
	if (LOOP_HEADER (loop) == LOOP_HEADER (loop_list->loop))
	  return loop_list->loop;         
      loop_list = loop_list->next;
    }
  return NULL;
}

//! @return "true" iff loop associated with "edge" exists in the "loop_list"
t_bool loopExists (t_loopref *loop_list, t_cfg_edge *edge)
{  
  t_bbl *footer, *header;

  header = T_BBL (CFG_EDGE_TAILP (edge));
  footer = T_BBL (CFG_EDGE_HEAD (edge));
  while (loop_list != NULL) 
    {
      t_loop *i_loop = loop_list->loop;
      t_bbl *i_footer = T_BBL (CFG_EDGE_HEAD (LOOP_BACKEDGES (i_loop)->edge));
      t_bbl *i_header = T_BBL (CFG_EDGE_TAILP (LOOP_BACKEDGES (i_loop)->edge));

      if (i_footer == footer && i_header == header)
	break;
      loop_list = loop_list->next;
    }
  return loop_list == NULL ? FALSE: TRUE;
}

//! @return true iff "loop" nests in the "parent" loop
t_bool isNested (t_loop *loop, t_loop *parent)
{
  t_back_edge *backedge;
  Word_t i_bbl = 0;
  Pvoid_t dummy;

  // if the loopcount is greater, loop cannot be nested in parent
  if (LOOP_COUNT (loop) >= LOOP_COUNT (parent))
    return FALSE;

  // if parent doesn't contain the loopheader or 
  // any of the heads of the backedges from loop, cannot be nested 
  backedge = LOOP_BACKEDGES (loop);
  if (!LoopContainsBbl (parent, T_BBL (LOOP_HEADER (loop))))
    return FALSE;
  while (backedge) 
    {
      if (!LoopContainsBbl (parent, T_BBL (CFG_EDGE_HEAD (backedge->edge))))
	return FALSE;
      backedge = backedge->next;
    }

  // check node per node, if all the nodes from loop are in the parent 
  JLF (dummy, loop->bbl_array, i_bbl);
  while (dummy != NULL) 
    {
      JLG (dummy, parent->bbl_array, i_bbl);
      if (dummy == NULL)
	return FALSE;
      JLN (dummy, loop->bbl_array, i_bbl);
    }

  return TRUE;
}

// remove i_loop from child loop list of j_loop
void unestLoop (t_loop *i_loop, t_loop *j_loop)
{
  t_loopref *iterparents,  *piterparents;

  iterparents = LOOP_PARENTS (i_loop);
  piterparents = NULL;

  // @HACK: maintain upward transitive relations
  // @CLEANUP: explain reason for above HACK more clearly
  // don't remove j_loop from parent loop list of i_loop
  /*
  while (iterparents)
  {
    if (iterparents->loop == j_loop)
      break;
    piterparents = iterparents;
    iterparents = iterparents->next;
  }
  if (iterparents) 
  {
    if (piterparents) 
      piterparents->next = iterparents->next;
    else
      LOOP_PARENTS(i_loop) = iterparents->next;
    Free (iterparents);
  }
  */

  iterparents = LOOP_CHILDS (j_loop);
  piterparents = NULL; 
  while (iterparents)
    {
      if (iterparents->loop == i_loop)
	break;
      piterparents = iterparents;
      iterparents = iterparents->next;
    }
  if (iterparents) 
    {
      if (piterparents) 
	piterparents->next = iterparents->next;
      else
	LOOP_CHILDS(j_loop) = iterparents->next;
      Free (iterparents);
    }  
  return;
}

//! @brief nest i_loop in j_loop
void nestLoop (t_loop *i_loop, t_loop *j_loop)
{
  t_loopref  *ichild, *iparent, *jchild, *jparent;
  t_loopref *newparent, *newchild;
  
  // return if already nested
  iparent = LOOP_PARENTS (i_loop);
  while (iparent) 
    {
      jchild = LOOP_CHILDS (j_loop);
      while (jchild) 
	{
	  if (iparent->loop == jchild->loop)
	    return;
	  jchild = jchild->next;
	}
      iparent = iparent->next;
    }  
  // remove i->k if j->k exists
  jparent = LOOP_PARENTS (j_loop);
  while (jparent)
    {
      unestLoop (i_loop, jparent->loop);
      jparent = jparent->next;
    }
  // remove g->j if g->i exists
  ichild = LOOP_CHILDS (i_loop);
  while (ichild) 
    {
      unestLoop (ichild->loop, j_loop);
      ichild = ichild->next;
    }
    
  newparent = (t_loopref *) Calloc (1, sizeof (t_loopref));
  newparent->loop = j_loop;
  newchild = (t_loopref *) Calloc (1, sizeof (t_loopref));
  newchild->loop = i_loop;
  
  newparent->next = LOOP_PARENTS (i_loop);
  LOOP_PARENTS (i_loop) = newparent;
  newchild->next = LOOP_CHILDS (j_loop);
  LOOP_CHILDS (j_loop) = newchild;     
}

//! @brief detect nesting within loops in list
void detectNesting (t_loopref *loop_list)
{
  t_loopref *i, *j;
  
  for (i = loop_list; i != NULL; i = i->next) 
    {
      for (j = loop_list; j != i; j = j->next) 
	{
	  t_loop *i_loop = i->loop; 
	  t_loop *j_loop = j->loop;

	  if (i_loop != j_loop) 
	    {      
	      bool i_nested_in_j = isNested (i_loop, j_loop);
	      bool j_nested_in_i = isNested (j_loop, i_loop);

	      if (i_nested_in_j ^ j_nested_in_i) 
		{
		  if(i_nested_in_j) 
		    nestLoop (i_loop, j_loop);
		  else	
		    nestLoop (j_loop, i_loop);
		}
	    }
	}
    }
  return;
}

//! @brief create a new loop structure corr to backedge
t_loop* newLoop (t_function *function,
		 t_back_edge *backedge)
{
  t_cfg *cfg  =  FUNCTION_CFG (function);
  t_cfg_edge *iter;
  list<t_bbl*> working_stack;
  t_bbl *header, *footer, *working_bbl; 
  t_loop  *loop = (t_loop *) Calloc (1, sizeof (t_loop));  

  // set loop members
  loop->cfg = cfg;  
  loop->bbl_array = (Pvoid_t) NULL; 
  BackEdgeAppendSorted (backedge, &LOOP_BACKEDGES(loop));
  
  header = CFG_EDGE_TAILP(backedge->edge);
  footer = CFG_EDGE_HEAD(backedge->edge);

  BblMarkInit ();
  BblMark (footer);
  working_stack.push_front (footer);

  // perform a reverse dfs to find nodes n, 
  // such that n is reachable from header but not-via footer.
  while (!working_stack.empty ()) 
    {
      working_bbl = working_stack.front ();
      working_stack.pop_front ();

      if (!loopAddBB (loop, working_bbl))
	continue;

      if (working_bbl == header)
	continue;

      BBL_FOREACH_PRED_EDGE (working_bbl, iter) 
	{
	  if (!BblIsMarked (CFG_EDGE_HEADP(iter))) 
	    {
	    t_bbl *blk = CFG_EDGE_HEADP(iter);
	    if (BBL_FUNCTION(blk) != BBL_FUNCTION (header)) continue;
	    BblMark (blk);	working_stack.push_front (blk);
	    }
	}
    }   
  return loop;
}

//! @return backedges in a graph root at node 
void dfs_back_edges (t_bbl *node, list<CfgEdge*>* back_edges)
{
  CfgEdge *edge;

  // @TODO t_graph node marking abstraction
  BblMark (node);  // mark as ancestor
  BblMark2 (node); // mark as visited

  FOREACH_SUCC_EDGE_IN_BB (edge, node) 
    {
      t_bbl *succ = CFG_EDGE_TAILP (edge);
      
      if (!succ || !BBL_CADDRESS (succ)) 
	continue;
      
      // perform dfs if not marked visited
      if (!BblIsMarked2 (succ))       
	dfs_back_edges (succ, back_edges);    

      // edge is a backedge since target is a ancestor
      if (BblIsMarked (succ)) 
	back_edges->push_front (edge);
    }

  BblUnmark (node); //unmark as ancestor
  return;
}

//! @return list of backedges in intra-procedural CFG of function
list<CfgEdge*> *BackEdgesInFunction (Function *F)
{
  list<t_bbl*> dfs_stack;
  static list<CfgEdge*> back_edges;

  // @TODO use only diablo abstraction (too many details exposed)
  // @TODO cfg node marking abstraction

  BblMarkInit();  
  BblMarkInit2 ();
  back_edges.clear ();
  dfs_back_edges (F->EntryBlock (), &back_edges);
  return &back_edges;
}

enum {REACHABLE_1 = 1, REACHABLE_2 = 2, REACHABLE = 3, NON_DOMINATED = 4};
static map<t_bbl*,char> node_mark;

//! @brief mark nodes reachable from node with mark 1
void dfs_mark_reachable_1_from (t_bbl *node)
{
  t_cfg_edge *succE;

  node_mark [node] |= REACHABLE_1;
  
  // @TODO: use only diablo abstraction (too many details exposed)
  
  if (node != FunctionGetExitBlock (BBL_FUNCTION (node)))
    FOREACH_SUCC_EDGE_IN_BB (succE, node) 
      {
	t_bbl * succN = CFG_EDGE_TAILP (succE);
	
	if (!succN || !BBL_CADDRESS (succN))
	  continue;

	if (!(node_mark[succN] & REACHABLE_1))
	  dfs_mark_reachable_1_from (succN);
      }
}

//! @brief mark nodes that can reach node with mark 2
void dfs_mark_reachable_2_to (t_bbl *node)
{
  t_bbl *predN;
  t_cfg_edge *predE;

  // @TODO: use only diablo abstraction (too many details exposed)

  node_mark[node] |= REACHABLE_2;
   
  if (node != FUNCTION_BBL_FIRST (BBL_FUNCTION (node)))
    FOREACH_PRED_EDGE_IN_BB (predE, node) 
      {
	predN = CFG_EDGE_HEADP (predE);

	if (!predN || !BBL_CADDRESS(predN))
	  continue;

	if (!(node_mark[predN] & REACHABLE_2))
	  dfs_mark_reachable_2_to (predN);
      }
}

//! @brief mark nodes which are not dominated by target
//! @param list l contains nodes marked dominated & marked 1 & 2
void dfs_list_nodes_not_dominated_by (BasicBlock *target, list<BasicBlock*> &l,
				      BasicBlock *node)
{ 
  CfgEdge *succE;

  if (node == target) 
    return;

  node_mark [node] |= NON_DOMINATED;
 
  FOREACH_SUCC_EDGE_IN_BB (succE, node) 
    {
      BasicBlock *succN = (succE)->TargetIntraProc ();
      if (succN == NULL || succN->IsReturnBlock ()) continue;
      if (!(node_mark [succN] & NON_DOMINATED))
	dfs_list_nodes_not_dominated_by (target, l, succN);
    }

  if ((node_mark [node] & REACHABLE_1) && (node_mark [node] & REACHABLE_2))
    l.push_front (node);
  
  return;
}

//! @return list of nodes in [target,source] and not dominated by target
bool 
IsSubgraphReducible (Function *F, BasicBlock *source, BasicBlock *target,
		     list<BasicBlock*> &nodesToSplit)
{
  BasicBlock  *entryNode;

  nodesToSplit.clear ();
  entryNode = F->EntryBlock ();

  if (source != target && target != entryNode) 
    {
      node_mark.clear ();
      node_mark[target] |= REACHABLE_2;
      dfs_mark_reachable_2_to (source);
      node_mark[source] |= REACHABLE_1;
      dfs_mark_reachable_1_from (target);  
      
      dfs_list_nodes_not_dominated_by (target, nodesToSplit, entryNode);
    }

  return nodesToSplit.empty ();
}

//! @return returns a cloned basic block of original
t_bbl *CloneBB (t_bbl *org)
{
  t_cfg *cfg;
  t_bbl *copy;
  t_function *f;

  // @TODO: use only diablo abstraction (too many details exposed)

  f = BBL_FUNCTION (org);
  cfg = FUNCTION_CFG (f);
  copy = BblNew (cfg);
  
  *copy = *org;
  
  BBL_SET_NEXT_IN_FUN (copy, BBL_NEXT_IN_FUN(org));
  BBL_SET_PREV_IN_FUN (BBL_NEXT_IN_FUN(org), copy);
  BBL_SET_NEXT_IN_FUN (org, copy);
  BBL_SET_PREV_IN_FUN (copy, org);

  BBL_SET_SUCC_FIRST (copy, NULL); BBL_SET_SUCC_LAST (copy, NULL);
  BBL_SET_PRED_FIRST (copy, NULL); BBL_SET_PRED_LAST (copy, NULL);
  BBL_SET_FUNCTION (copy, f);

  return copy;
}

//! @brief split nodes such that graph becomes reducible
void SplitNodesToReduce (Function *F, list<BasicBlock*> *nodes)
{
  t_bbl *source;
  list<BasicBlock*>::iterator iter;
  t_cfg_edge *predE, *succE, *newE;
  map<void *, void *> clone;
  t_cfg *cfg = FUNCTION_CFG (F);

  // @TODO: only use diablo abstraction (too many details exposed)

  for (iter = nodes->begin(); iter != nodes->end(); iter++) 
    {
      source = (*iter);
      clone[source] = CloneBB (source);
    }
  
  for (iter = nodes->begin(); iter != nodes->end(); iter++) 
    {
      source = *iter;
      BBL_FOREACH_SUCC_EDGE (source, succE) 
	{
	  t_bbl *tail = CFG_EDGE_TAIL (succE);
	  if (clone[tail])
	    newE = CfgEdgeNew (cfg, (t_bbl*) clone[source], 
			       (t_bbl *) clone[tail], CFG_EDGE_CAT(succE));
	  else
	    newE = CfgEdgeNew (cfg,(t_bbl *) clone[source], tail, 
			       CFG_EDGE_CAT(succE));
	  clone[succE] = newE;
	}
    }

  for (iter = nodes->begin(); iter != nodes->end(); iter++) 
    {    
      t_cfg_edge *predT;
      source = *iter;
      for (predE = BBL_PRED_FIRST(source); predE ; predE = predT) 
	{
	  predT = CFG_EDGE_PRED_NEXT (predE);
	  t_bbl *head = CFG_EDGE_HEAD (predE);
	  
	  if (CFG_EDGE_IS_BACKWARD_INTERPROC (predE) && 
	      clone[CFG_EDGE_CORR (predE)]) 
	    {
	      t_cfg_edge *newcorrE = (t_cfg_edge *) 
		clone[CFG_EDGE_CORR (predE)];
	      newE = CfgEdgeNew (cfg, head, (t_bbl*) clone[source], 
				 CFG_EDGE_CAT (predE));
	      CFG_EDGE_SET_CORR (newE, newcorrE);
	      CFG_EDGE_SET_CORR (newcorrE, newE);	
	    }
	  else if (((node_mark[head] & NON_DOMINATED) &&
		    !((node_mark[head] & REACHABLE_1) && 
		      (node_mark[head] & REACHABLE_2))) ||
		   (CFG_EDGE_IS_BACKWARD_INTERPROC (predE))) 
	    {
	      CFG_EDGE_SET_TAIL (predE, (t_bbl *) clone[source]);
	      if (CFG_EDGE_PRED_PREV (predE))
		CFG_EDGE_SET_PRED_NEXT (CFG_EDGE_PRED_PREV (predE),
					CFG_EDGE_PRED_NEXT (predE));
	      else
		BBL_SET_PRED_FIRST (source, CFG_EDGE_PRED_NEXT (predE));
	      if (CFG_EDGE_PRED_NEXT (predE))
		CFG_EDGE_SET_PRED_PREV (CFG_EDGE_PRED_NEXT (predE),
					CFG_EDGE_PRED_PREV (predE));
	      else
		BBL_SET_PRED_LAST (source, CFG_EDGE_PRED_PREV (predE));
	      
	      CFG_EDGE_SET_PRED_NEXT (predE, NULL);
	      CFG_EDGE_SET_PRED_PREV (predE, 
				      BBL_PRED_LAST ((t_bbl*) clone[source]));
	      if (BBL_PRED_LAST ((t_bbl*)clone[source]))
		CFG_EDGE_SET_PRED_NEXT (BBL_PRED_LAST ((t_bbl*) clone[source]),
					predE);
	      else 
		BBL_SET_PRED_FIRST ((t_bbl*)clone[source], predE);
	      
	      BBL_SET_PRED_LAST ((t_bbl*)clone[source], predE);	
	    }
	}
    }
  return;
}

//! @brief detect loops in function & their nesting 
t_loopref* DetectLoopNesting (Function *F)
{
  t_loopref *fLoops, *insert;
  t_loop *loopM, *loop;
  t_loopref *outermostLoops = NULL;
  t_back_edge *backEdge;
  list<CfgEdge*> *backEdges;
  list<CfgEdge*>::iterator elm;
  t_bool reducible;
  BasicBlock *source, *target;
  list<BasicBlock*> nodesToSplit;


  // @TODO: use list<CfgEdge*> for list of back-edges
  // @TODO: use list<Loop*> for list of loops
  
  // iterate till graph is refined to be reducible 
  // (i.e. all backedges have their head dominated by tail)  
  do 
    {             
      reducible = TRUE;
      backEdges = BackEdgesInFunction (F);  
      for (elm = backEdges->begin (); elm != backEdges->end (); elm++) 
	{
	  CfgEdge *e = *elm;
	  source = e->Source (); 
	  target = e->Target ();  
	  
	  if (!IsSubgraphReducible (F, source, target, nodesToSplit)) 
	    { 
	      reducible = FALSE; 
	      break;
	    }
	}

      if (!reducible) 
	{  
	  SplitNodesToReduce (F, &nodesToSplit);
#ifdef DEBUG_REDUCE
	  ExportCFG (F, ".split.vcg");
#endif
	}
    
    } while (!reducible);
  
  fLoops = NULL; 
  backEdges = BackEdgesInFunction (F); 
  for (elm = backEdges->begin (); elm != backEdges->end (); elm++) 
    {
      CfgEdge *edge = (*elm);
      source = edge->Source (); 
      target = edge->Target ();
      insert = NULL; backEdge = NULL;
      
      if (loopExists (fLoops, edge))
	continue;      

      backEdge = (t_back_edge *) Calloc (1, sizeof(t_back_edge));
      backEdge->edge = edge; backEdge->has_corr = FALSE;	   
      loop = newLoop (F, backEdge);
         
      if ((loopM = loopWithSameHeaderExists (fLoops, loop)) != NULL) 
	{
	  mergeLoop (loop, loopM); 
	  Free (loop);	  // TOFIX: Leak?    
	}
      else 
	{
	  insert = (t_loopref *) Calloc (1, sizeof(t_loopref));
	  insert->loop = loop; insert->next = NULL;
	  LoopRefAppendSorted (insert, &fLoops);
	}
    }
  detectNesting (fLoops);

  // collect the loops whit no parents in outermost_loops
  while (fLoops) 
    {
      t_loopref* fLoopsNext = fLoops->next;
      if (LOOP_PARENTS(fLoops->loop) == NULL)
	LoopRefAppendSorted (fLoops, &outermostLoops);    
      fLoops = fLoopsNext;
    }

  return outermostLoops;
}


/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2007 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/* ===================================================================== */
/*
  @ORIGINAL_AUTHOR: Robert Cohn
*/

/* ===================================================================== */
/*! @file
 *  Pin tool to trace dynamic memory accesses and control flow.
 */

#include "pin.H"
#include <iostream>
#include <map>
#include <fstream>
#include <iomanip>
/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
using namespace std;
ofstream traceDataFile;
ofstream traceControlFile;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "pinatrace.out", "specify trace file name");
KNOB<BOOL> KnobValues(KNOB_MODE_WRITEONCE, "pintool",
    "values", "1", "Output memory values reads and written");

/* ===================================================================== */

static INT32 Usage()
{
  cerr << "This tool produces a memory address & control flow trace.\n"
       << "For each (dynamic) instruction reading or writing to memory, \n"
       << "the the effective address used & defined are recorded\n";
  
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  
  return -1;
}

static VOID 
RecordMem(VOID * ip, CHAR r, VOID * addr, INT32 size, 
	  BOOL isPrefetch)
{
  traceDataFile.write ((char *) &addr, sizeof (addr));
  traceDataFile.write ((char *) &size, sizeof (size));
//  cerr << "(" << r << " " << addr << "," << size << " ) ";

}

static VOID *WriteAddr;
static INT32 WriteSize;

static VOID 
RecordWriteAddrSize (VOID * addr, INT32 size)
{
  WriteAddr = addr;
  WriteSize = size;
}

static VOID 
RecordMemWrite (VOID *ip)
{
  RecordMem(ip, 'W', WriteAddr, WriteSize, false);  
}

int insCount;

static VOID
RecordControlPred (VOID *ip)
{
  insCount++;	
  traceControlFile.write ((char *) &ip, sizeof (ip));
  // cerr << ip << endl;
}


VOID Instruction(INS ins, VOID *v)
{    
  // instruments loads using a predicated call, i.e.
  // the call happens iff the load will be actually executed
            
  if (INS_IsMemoryRead (ins))
    {
      INS_InsertPredicatedCall (ins, IPOINT_BEFORE, (AFUNPTR) RecordMem,
				IARG_INST_PTR,
				IARG_UINT32, 'R',
				IARG_MEMORYREAD_EA,
				IARG_MEMORYREAD_SIZE,
				IARG_UINT32, INS_IsPrefetch (ins),
				IARG_END);
    }

    if (INS_HasMemoryRead2 (ins))
      {
        INS_InsertPredicatedCall (ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
				  IARG_INST_PTR,
				  IARG_UINT32, 'R',
				  IARG_MEMORYREAD2_EA,
				  IARG_MEMORYREAD_SIZE,
				  IARG_UINT32, INS_IsPrefetch(ins),
				  IARG_END);
      }

    // instruments stores using a predicated call, i.e.
    // the call happens iff the store will be actually executed
    if (INS_IsMemoryWrite (ins))
      {
        INS_InsertPredicatedCall (ins, IPOINT_BEFORE, 
				  (AFUNPTR) RecordWriteAddrSize,
				  IARG_MEMORYWRITE_EA,
				  IARG_MEMORYWRITE_SIZE,
				  IARG_END);
        
        if (INS_HasFallThrough (ins))
	  {
            INS_InsertCall (ins, IPOINT_AFTER, 
			    (AFUNPTR) RecordMemWrite,
			    IARG_INST_PTR,
			    IARG_END);	    
	  }
        if (INS_IsBranchOrCall (ins))
	  {
            INS_InsertCall (ins, IPOINT_TAKEN_BRANCH, 
			    (AFUNPTR) RecordMemWrite,
			    IARG_INST_PTR,
			    IARG_END);
	  }        
      }

    // instrument each instruction to save predecessor's instruction pointer.
    if (INS_HasFallThrough (ins))
      {
	INS_InsertCall (ins, IPOINT_AFTER,
			(AFUNPTR) RecordControlPred,
			IARG_INST_PTR,
			IARG_END);
      }
    else if (INS_IsBranchOrCall (ins))
      {
	INS_InsertCall (ins, IPOINT_TAKEN_BRANCH,
			(AFUNPTR) RecordControlPred,
			IARG_INST_PTR,
			IARG_END);
      }
    return;
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{  
  cerr << "Instruction Count = " << insCount << endl;
  traceDataFile.close ();
  traceControlFile.close ();
}

/* ===================================================================== */

int main(int argc, char *argv[])
{
    string trace_header = string("#\n"
                                 "# Memory Access Trace Generated By Pin\n"
                                 "#\n");
    
    if( PIN_Init(argc,argv) )
      {
        return Usage();
      }
    
    traceDataFile.open (".trace.data");
    traceControlFile.open (".trace.control");
    
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();    
    
    return 0;
}


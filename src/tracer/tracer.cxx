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
 *  This file contains an ISA-portable PIN tool for tracing memory accesses.
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
ofstream TraceFile;
ofstream TraceIndexFile;
ofstream TraceStats;

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
    cerr <<
        "This tool produces a memory address trace.\n"
        "For each (dynamic) instruction reading or writing to memory the the ip and ea are recorded\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;

    return -1;
}

static INT32 ins_count = 0, r_count = 0, w_count = 0;
static map<int,int> ins_static, ins_dynamic;

enum {INS_REGISTER = 0, INS_IMMEDIATE , INS_DIRECT, INS_INDIRECT,
      INS_INDEXED, INS_SCALED};

static VOID RecordMem(VOID * ip, CHAR r, VOID * addr, INT32 size, BOOL isPrefetch)
{
    TraceFile.write ((char *) &ip, sizeof(ip));
    TraceFile.write ((char *) &addr,sizeof(addr));
    TraceFile.write ((char *) &size,sizeof(size));
    if (r == 'R')
      r_count ++;
    else if (r == 'W')
      w_count ++;
}

static VOID * WriteAddr;
static INT32 WriteSize;

static VOID RecordWriteAddrSize(VOID * addr, INT32 size)
{
    WriteAddr = addr;
    WriteSize = size;
}

static VOID RecordInsType (INT32 type)
{
  ins_count++;
  ins_dynamic[type]++;
} 

static VOID RecordMemWrite(VOID * ip)
{
    RecordMem(ip, 'W', WriteAddr, WriteSize, false);
}

VOID Instruction(INS ins, VOID *v)
{
  string ins_str =  INS_Disassemble(ins);

  
  if (INS_MemoryBaseReg(ins) == REG_EBP && 
      (!INS_SegmentPrefix(ins) || INS_SegmentRegPrefix(ins) == REG_SEG_SS) &&
      !REG_valid(INS_MemoryIndexReg(ins))
     )    
      return;

  if ( !REG_valid(INS_MemoryBaseReg(ins)) && 
       !REG_valid(INS_MemoryIndexReg(ins))
       )  
      return;
 
  unsigned int OpCount = INS_OperandCount (ins);
  int insType = INS_REGISTER;
  while (OpCount--) {
    if (INS_OperandIsImmediate (ins, OpCount)) 
      insType = INS_IMMEDIATE;    
    if (INS_OperandMemoryDisplacement (ins, OpCount) != 0)
      insType = INS_DIRECT;
    if (REG_valid (INS_OperandMemoryBaseReg (ins, OpCount)))
      insType = INS_INDIRECT;
    if (REG_valid (INS_OperandMemoryIndexReg (ins, OpCount)))
      insType = INS_INDEXED;
    if (INS_OperandMemoryScale (ins, OpCount) > 1)
      insType = INS_SCALED;
  }
  ins_static [insType]++;
      
  INS_InsertCall (
		  ins, IPOINT_BEFORE, (AFUNPTR) RecordInsType,
		  IARG_UINT32, insType,
		  IARG_END
		  );
  
  if (INS_IsMemoryRead(ins))
    {
      INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
            IARG_INST_PTR,
            IARG_UINT32, 'R',
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, INS_IsPrefetch(ins),
            IARG_END);
    }

    if (INS_HasMemoryRead2(ins))
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
            IARG_INST_PTR,
            IARG_UINT32, 'R',
            IARG_MEMORYREAD2_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_UINT32, INS_IsPrefetch(ins),
            IARG_END);
    }

    // instruments stores using a predicated call, i.e.
    // the call happens iff the store will be actually executed
    if (INS_IsMemoryWrite(ins))
    {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordWriteAddrSize,
            IARG_MEMORYWRITE_EA,
            IARG_MEMORYWRITE_SIZE,
            IARG_END);
        
        if (INS_HasFallThrough(ins))
        {
            INS_InsertCall(
                ins, IPOINT_AFTER, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_END);
        }
        if (INS_IsBranchOrCall(ins))
        {
            INS_InsertCall(
                ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_END);
        }
        
    }
}

/* ===================================================================== */

static char *instructionType[6] =  {
  "REGISTER",
  "IMMEDIATE",
  "DIRECT",
  "INDIRECT",
  "INDEXED",
  "SCALED"
};

VOID Fini(INT32 code, VOID *v)
{
  map<string, int>::iterator t;
  TraceStats << "# Instructions = " << ins_count << "\n";
  TraceStats << "# Read Instructions = " << r_count << "\n";
  TraceStats << "# Write Instructions = " << w_count << "\n";
  TraceStats << "\nStatic Addressing Mode Frequencies:\n";
  
  for (int i = 0; i < 6; i++)
    TraceStats << instructionType[i] << ":  " << ins_static [i] << "\n";

  TraceStats << "\nDynamic Addressing Mode Frequencies:\n";

  for (int i = 0; i < 6; i++)
    TraceStats <<  instructionType[i] << ":  " << ins_dynamic [i] << "\n";
  
  TraceFile.close();
  TraceStats.close();
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
    
    TraceFile.open(".trace");
    //    TraceFile.write(trace_header.c_str(),trace_header.size());
    //    TraceFile.setf(ios::showbase);
    TraceStats.open(".stats");
    
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns

    PIN_StartProgram();
    
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

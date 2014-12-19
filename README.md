Dynamic Slicer
==============

Introduction
------------
This is a prototype dynamic slicer tool for C programs on x86 platform. The tool
can handle programs using pointers, composite types and procedure calls. It
computes slices at machine code level, and thus handles (statically-linked)
library calls.

Details
-------
The tool requires the binary program to be compiled from a patched gcc
tool-chain and linked statically. It uses Diablo [1] to extract the Control
Flow Graph (CFG) of each procedure from machine code. To record the trace, PIN
[2] is used for instrumentation. See APLAS-2008-submission.pdf for more details.

[1] https://diablo.elis.ugent.be
[2] http://rogue.colorado.edu/Pin

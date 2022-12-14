/***********************************************************************/
/**   microCLAIRE                                       Yves Caseau    */
/**   readme.cpp for Kernel                                            */
/**  Copyright (C) 1998-00 Yves Caseau. All Rights Reserved.           */
/***********************************************************************/

The microCLAIRE library is made of two modules. The Kernel module is written in C++
and the Core module is written in CLAIRE and compiled into C++.

this directory contains the C++ code for the Kernel module of microCLAIRE v3.0.

last modified: 4/10/00


clAlloc.cpp   contains the ClaireAlloc class that encapsulates all alloc/gc services
              We use do not use C++ new and delete, so that we can allocate everything
              with a same reference address (&Cmemory[0]) which is necessary for OIDs.
             

clBag.cpp     contains the bag class and its 3 subclasses: array, list and set
              

clEnv.cpp     contains the environment classes:
                 ClaireRes manages worlds and symbols (+ ClaireSymbol)
                 ClaireEnv class that manages the eval and exception stacks
              

clString.cpp  contains the imported objects (Strings, Floats, int, char,...)
              

clPort.cpp    contains the ClairePort class and its subclasses
              

clReflect.cpp contains the reflective class for classes (ClaireClass) and properties
              


Kernel.cpp    contains the reflective description (claireClasses) of the Kernel module
              this file is equivalent to the module description that is generated by the
              compiler

clConsole.cpp  contains the C++ code for the Console module, whicg provides a text-based interface
               for CLAIRE



In addition, this directory contains 3 test files

testKernel.cpp

testmClaire.cpp

testiClaire.cpp

and a Makefile for building the Kernel.lib library and running the different test files

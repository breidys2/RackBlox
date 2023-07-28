This directory contains the p4 source code for RackBlox.
It will not compile standalone, due to missing tofino specific headers that are confidential.

The code is written and tested with Intel's 9.10.0 SDE on a Tofino 1 switch running Ubuntu 18.04.

The compile and run simply run: ./compile.sh
Please note that the compile script has a dependency on an intel provided script (part of the 9.10.0 SDE).

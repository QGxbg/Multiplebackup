#usage:
#   python fullnode.py
#       1.  cluster [lab]
#       2.  CODE [Clay|RDP|HHXORPlus|BUTTERFLY]
#       3.  ECN [4]
#       4.  ECK [2]
#       5.  ECW [4]
#       6.  method [centralize|offline|parallel]
#       7.  scenario [standby|scatter]
#       8.  blkMiB [1|256]
#       9.  pktKiB [64]
#       10. batchsize [3]
#       11. num stripes [20]
#       12. gendata [true|false]
#       13. BDWTMpbs [1000]

import os
import sys
import subprocess
import time

CLUSTER=sys.argv[1]
CODE=sys.argv[2]
ECN=int(sys.argv[3])
ECK=int(sys.argv[4])
ECW=int(sys.argv[5])
METHOD=sys.argv[6]
SCENARIO=sys.argv[7]
BLKMB=int(sys.argv[8])
PKTKB=int(sys.argv[9])
BATCHSIZE=int(sys.argv[10])
NSTRIPE=int(sys.argv[11])
GENDATASTR=sys.argv[12]
BDWT=int(sys.argv[13])
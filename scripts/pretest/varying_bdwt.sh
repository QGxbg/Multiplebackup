#!/bin/bash

CLUSTER=aliyun
CODE=Clay
BLKMB=256
PKTKB=64
STRIPENUM=50
GENDATA=false
# BDWT=1000
BATCHSIZE=29
LOGDIR=./log



# Clay 14 10
ECN=14
ECK=10
ECW=256

# scatter
SCENARIO=scatter
for i in {1..4}
do
    for bdwt in 500 2000 5000 10000
    do
        METHOD=centralize
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}

        METHOD=offline
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}

        METHOD=parallel
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $BATCHSIZE $STRIPENUM $GENDATA $bdwt > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${BATCHSIZE}_${bdwt}_${i}
    done
done

#!/bin/bash

CLUSTER=aliyun
CODE=Clay
BLKMB=256
# PKTKB=64

STRIPENUM=50
GENDATA=false
BDWT=1000
LOGDIR=./log

# Clay 14 10
ECN=14
ECK=10
ECW=256

# scatter
SCENARIO=scatter
for i in {1..4}
do
    for pktkb in 16 32 128
    do
        METHOD=centralize
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}

        METHOD=offline
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}

        METHOD=parallel
        echo "python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}"
        python bw.py $CLUSTER $CODE $ECN $ECK $ECW $METHOD $SCENARIO $BLKMB $PKTKB $batchsize $STRIPENUM $GENDATA 1000 > $LOGDIR/${CODE}_${ECN}_${ECK}_${ECW}_${METHOD}_${SCENARIO}_${batchsize}_${i}
    done
done

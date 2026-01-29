#!/bin/bash

HADOOP_SRC=/home/pararc/hadoop-3.3.4-src
HADOOP_CONF=$HADOOP_SRC/hadoop-dist/target/hadoop-3.3.4/etc/hadoop

cp core-site.xml $HADOOP_CONF/core-site.xml
cp hadoop-env.sh $HADOOP_CONF/hadoop-env.sh
cp workers $HADOOP_CONF/workers

for i in {1..5}
do
    rsync -avzr core-site.xml agent$i:$HADOOP_CONF/core-site.xml
    rsync -avzr hadoop-env.sh agent$i:$HADOOP_CONF/hadoop-env.sh
    rsync -avzr workers agent$i:$HADOOP_CONF/workers
done

#!/bin/bash

HDFS=/home/pararc/hadoop-3.3.4-src/hadoop-dist/target/hadoop-3.3.4

rm -rf $HDFS/logs
rm -rf $HDFS/dfs

for i in {1..5}
do
    echo $i
    ssh agent$i "rm -rf $HDFS/dfs"
    ssh agent$i "rm -rf $HDFS/logs"
done

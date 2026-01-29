#!/bin/bash

# CODE [Clay|RDP|HHXORPlus|BUTTERFLY]
# BUTTERFLY 6 4 8 
# BUTTERFLY 8 6 32
# BUTTERFLY 10 8 128
# BUTTERFLY 12 10 512
# RDP 12 10 10
# HHXORPlus 14 10 2

# home dir
system = "Multiple_ParaRC"
home_dir=$HOME
proj_dir="$home_dir/$system"
script_dir="$proj_dir/scripts"
script="${script_dir}/pretest/bw.py"

cd $proj_dir

# first exert, need to gendata
python $script aliyun BUTTERFLY 8 6 32 parallel scatter 256 64 29 100 true 1000
key=BUTTERFLY_8_6_32_256
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key


# first exert, need to gendata
python $script aliyun BUTTERFLY 10 8 128 parallel scatter 256 64 29 100 true 1000
key=BUTTERFLY_BUTTERFLY_10_8_128
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key


# first exert, need to gendata
python $script aliyun BUTTERFLY 10 8 128 parallel scatter 256 64 29 100 true 1000
key=BUTTERFLY_10_8_128_256
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key


# first exert, need to gendata
python $script aliyun BUTTERFLY 12 10 512 parallel scatter 256 64 29 100 true 1000
key=BUTTERFLY_12_10_512_256
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key


# first exert, need to gendata
python $script aliyun RDP 12 10 10 parallel scatter 256 64 29 100 true 1000
key=RDP_12_10_10
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key


# first exert, need to gendata
python $script aliyun HHXORPlus 14 10 2 parallel scatter 256 64 29 100 true 1000
key=HHXORPlus_14_10_2
for i in {001..030} ; do ssh agent$i "rm  ~/$system/blkDir/*.repair"; done
for i in {001..030} ; do ssh agent$i "mv  ~/$system/blkDir ~/$system/blkDir_$key"; done
cp  ./stripeStore/placement ./stripeStore/placement_$key

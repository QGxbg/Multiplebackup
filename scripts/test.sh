#!/bin/bash
system = "Multiple_ParaRC"
# home dir
home_dir=$HOME
proj_dir="$home_dir/$system"
script_dir="$proj_dir/scripts"
script="${script_dir}/pretest/bw.py"

cd $proj_dir

# 3. run
echo "python $script lab Clay 14 10 256 parallel scatter 4 1 1 10 true 1000"
date
python $script lab Clay 4 2 4 parallel scatter 4 1 1 10 true 1000
# log 
mv curr_output  log/varying_nk/Clay_14_10_256_256_output
#!/bin/bash
# simulation experiment for varying stripes num 

# home dir
home_dir=$HOME
proj_dir="$home_dir/parafullnode"
script_dir="$proj_dir/scripts"
simulation_dir="$script_dir/simulation"
log_dir="$simulation_dir/log"
ret_dir="$simulation_dir/ret"

# executable_file_path
executable_file_offline="$proj_dir/build/Sim_offline"
executable_file_parallel="$proj_dir/build/Sim_parallel"
executable_file_centralize="$proj_dir/build/Sim_centralize"
gen_simulation_data="$script_dir/data/gen_simulation_data.py"




code="Clay"
scenario="scatter"
# stripes_num=1000
agents_num=100

cd $proj_dir
rm ${log_dir}/varying_stripesnum/*
rm ${ret_dir}/varying_stripesnum/*

ecn=14
eck=10
ecw=256 

for stripes_num in {100,200.300,499,500,600,700,800,900}; do 
    batchsize=$((agents_num-1))
    centralize_scatter_log="$log_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_centralize_scatter"
    centralize_scatter_ret="$ret_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_centralize_scatter"
    offline_scatter_log="$log_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_offline_scatter"
    offline_scatter_ret="$ret_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_offline_scatter"
    parallel_scatter_log="$log_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_parallel_scatter"
    parallel_scatter_ret="$ret_dir/varying_stripesnum/${code}_${ecn}_${eck}_${ecw}_${stripes_num}_parallel_scatter"


    for j in {1..10}; do
        echo "for stripesnum = $stripes_num ":"$j times"
        python $gen_simulation_data $agents_num $stripes_num $ecn $eck 0  
        simulation_placement="$proj_dir/stripeStore/simulation_${agents_num}_${stripes_num}_${ecn}_${eck}"
        
        echo "centralize"
        $executable_file_centralize $simulation_placement $agents_num $stripes_num $code $ecn $eck $ecw $scenario $batchsize 0 >>  $centralize_scatter_log
        
        echo "offline" 
        $executable_file_offline $simulation_placement $agents_num $stripes_num $code $ecn $eck $ecw $scenario $batchsize 0 >>  $offline_scatter_log
        
        echo "parallel"
        $executable_file_parallel $simulation_placement $agents_num $stripes_num $code $ecn $eck $ecw $scenario  0 >>  $parallel_scatter_log
    done


    grep -oP 'overall load: \K\d+' $centralize_scatter_log >> $centralize_scatter_ret
    echo >> $centralize_scatter_ret
    grep -oP 'overall bdwt: \K\d+' $centralize_scatter_log >> $centralize_scatter_ret

    grep -oP 'overall load: \K\d+' $offline_scatter_log >> $offline_scatter_ret
    echo >> $offline_scatter_ret
    grep -oP 'overall bdwt: \K\d+' $offline_scatter_log >> $offline_scatter_ret

    grep -oP 'overall load: \K\d+' $parallel_scatter_log >> $parallel_scatter_ret
    echo >> $parallel_scatter_ret
    grep -oP 'overall bdwt: \K\d+' $parallel_scatter_log >> $parallel_scatter_ret
done

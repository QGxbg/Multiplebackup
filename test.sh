#!/bin/bash


for i in {1..9}
do
	echo $i
	#python scripts/data/gen_simulation_data.py 100 1000 6 4 0
	#./build/MulSim_parallel stripeStore/simulation_100_1000_6_4 100 1000  BUTTERFLY 6 4 8 scatter 99 0 > butterfly_6_4_8_parallel$i.output
	#./build/Sim_centralize stripeStore/simulation_100_1000_6_4 100 1000  BUTTERFLY 6 4 8 scatter 99 0 > butterfly_6_4_8_centralize$i.output
	#./build/Sim_offline stripeStore/simulation_100_1000_6_4 100 1000  BUTTERFLY 6 4 8 scatter 99 0 > butterfly_6_4_8_offline$i.output

	#
	#python scripts/data/gen_simulation_data.py 100 1000 8 6 0
	#./build/MulSim_parallel stripeStore/simulation_100_1000_8_6 100 1000  BUTTERFLY 8 6 32 scatter 99 0 > butterfly_8_6_32_parallel$i.output
	#./build/Sim_centralize stripeStore/simulation_100_1000_8_6 100 1000  BUTTERFLY 8 6 32 scatter 99 0 > butterfly_8_6_32_centralize$i.output
	#./build/Sim_offline stripeStore/simulation_100_1000_8_6 100 1000  BUTTERFLY 8 6 32 scatter 99 0 > butterfly_8_6_32_offline$i.output

	python scripts/data/gen_simulation_data.py 100 1000 10 8 0
	echo parallel
	./build/MulSim_parallel stripeStore/simulation_100_1000_10_8 100 1000  BUTTERFLY 10 8 128 scatter 99 0 > butterfly_10_8_128_parallel$i.output
	echo centralize
	./build/Sim_centralize stripeStore/simulation_100_1000_10_8 100 1000  BUTTERFLY 10 8 128 scatter 99 0 > butterfly_10_8_128_centralize$i.output
	echo offline
	./build/Sim_offline stripeStore/simulation_100_1000_10_8 100 1000  BUTTERFLY 10 8 128 scatter 99 0 > butterfly_10_8_128_offline$i.output

#	python scripts/data/gen_simulation_data.py 100 1000 12 10 0
#	echo parallel
#	./build/MulSim_parallel stripeStore/simulation_100_1000_12_10 100 1000  BUTTERFLY 12 8 512 scatter 99 0 > butterfly_10_8_128_parallel$i.output
#	echo centralize
#	./build/Sim_centralize stripeStore/simulation_100_1000_12_10 100 1000  BUTTERFLY 12 8 512 scatter 99 0 > butterfly_10_8_128_centralize$i.output
#	echo offline
#	./build/Sim_offline stripeStore/simulation_100_1000_12_10 100 1000  BUTTERFLY 12 10 512 scatter 99 0 > butterfly_10_8_128_offline$i.output


done



#!/bin/csh
  
#$ -M hgeng@nd.edu       # Email address for job notification
#$ -m abe                # Send mail when job begins, ends and aborts
#$ -pe smp 8    # Specify parallel environment and legal core size
#$ -q long              # Specify queue
#$ -N connectedcomponent_DataPrediction                 # Specify job name

module load python/3.10.5
scons build/X86/gem5.opt -j8
#build/X86/gem5.opt -d out configs/scattered_memory_config/config.py --cmd=./scattered_memory/memory & build/X86/gem5.opt -d out1 configs/scattered_memory_config/config.py --cmd=./scattered_memory/memory_seq

#build/X86/gem5.opt -d out configs/scattered_memory_config/config.py --cmd=./scattered_memory/good_locality
build/X86/gem5.opt -d out configs/scattered_memory_config/config.py --cmd=./Graph/connectedcomponent
mv /tmp/Data_DataPrediction.txt ~/Counter_Cache/Counter_Predict/Data_Prediction/ResultLog
#build/X86/gem5.opt -d out1 configs/scattered_memory_config/config.py --cmd=./scattered_memory/memory_seq

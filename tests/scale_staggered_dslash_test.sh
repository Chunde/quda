#!/bin/bash

function run_dslash_test {
    ngpus=$1
    gs_x=$2
    gs_y=$3
    gs_z=$4
    gs_t=$5

    xdim=$((64/$gs_x))
    ydim=$((64/$gs_y))
    zdim=$((64/$gs_z))
    tdim=$((192/$gs_t))
    
    precs="double single half"
    recons="18 12"
    
    prog="staggered_dslash_test"
    if [ ! -e "$prog" ]; then
        echo "The program $prog does not exist; this program will not be tested!"
        exit
    fi
    for prec in $precs ; do
	for recon in $recons ; do
	    cmd="mpirun -n $ngpus ./$prog --xdim $xdim --ydim $ydim --zdim $zdim --tdim $tdim --recon $recon --prec $prec --xgridsize $gs_x --ygridsize $gs_y --zgridsize $gs_z --tgridsize $gs_t"
	    echo running $cmd
	    echo "----------------------------------------------------------" 
	    echo $cmd 
	    $cmd 
	done
    done
}

#grid size for 16 gpus: C2050 does not have enough memory for this one
partitions[16]="
1 2 2 4
1 1 2 8
1 1 1 16
"

#gridsize for 32 GPUs
partitions[32]="
2 2 2 4 
1 2 2 8 
1 1 4 8 
1 1 1 32 
"

#gridsize for 48 GPUs
partitions[48]="
2 2 2 6 
1 2 2 12 
1 1 4 12 
"

#gridsize for 64 GPUs
partitions[64]="
2 2 2 8 
1 2 4 8 
1 1 4 16 
"
#gridsize for 96 GPUs
partitions[96]="
2 2 2 12 
1 2 4 12 
1 1 4 24 
"

#gridsize for 128 GPUs
partitions[128]="
2 2 4 8 
1 4 4 8 
1 1 8 16 
"


num_gpus="128 96 64 48 32"
for n in $num_gpus; do 
    #echo for $n GPUs
    plist=${partitions[$n]}
    j="0"	
    for v in $plist; do
	modj=$(($j % 4))
	if [ $modj == "0" ]; then
	    gridsize_x=$v
	fi
	
        if [ $modj == "1" ]; then
	    gridsize_y=$v
        fi
	
        if [ $modj == "2" ]; then
	    gridsize_z=$v
        fi
	
        if [ $modj == "3" ]; then
	    gridsize_t=$v
	    #echo gridsize_x/y/z/t/=$gridsize_x, $gridsize_y, $gridsize_z, $gridsize_t
	    run_dslash_test $n $gridsize_x $gridsize_y $gridsize_z $gridsize_t
        fi
	
	j=$(($j+1))
  done
done



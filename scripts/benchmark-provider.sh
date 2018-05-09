#!/bin/bash -eu

set -x
PROVIDER=${PROVIDER:?"Provider library is required"}

total_transactions=$((1 << 17))

function run_benchmark()
{
    servers=$1
    clients=$2
    transactions=$(($total_transactions/($servers*$clients)))
    result_file=dbms-bench-$(basename $PROVIDER)-$servers-$clients
    echo "Running benchmark for servers: $servers clients $clients"
    rm -r *_data/ || :
    command time -v ./src/dbms_simulator --servers=$servers --clients=$clients --transactions=$transactions --wsrep-provider="$PROVIDER" --fast-exit=1 >& $result_file
}


# for servers in 1 2 4 8 16
for servers in 16
do
#    for clients in 1 2 4 8 16 32
    for clients in 4 8 16 32
    do
	run_benchmark $servers $clients
    done
done

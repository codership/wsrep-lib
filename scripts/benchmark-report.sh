#!/bin/bash -eu

PROVIDER=${PROVIDER:?"Provider library is required"}

function report()
{
    servers=$1
    clients=$2
    result_file=dbms-bench-$(basename $PROVIDER)-$servers-$clients
    trx_per_sec=$(grep -a "Transactions per second" "$result_file" | cut -d ':' -f 2)
    cpu=$(grep -a "Percent of CPU this job got" "$result_file" | cut -d ':' -f 2)
    vol_ctx_switches=$(grep -a "Voluntary context switches" "$result_file" | cut -d ':' -f 2)
    invol_ctx_switches=$(grep -a "Involuntary context switches" "$result_file" | cut -d ':' -f 2)
    echo "$clients $cpu $trx_per_sec $vol_ctx_switches $invol_ctx_switches"
}

for servers in 1 2 4 8 16
do
    echo "Servers: $servers"
    for clients in 1 2 4 8 16 32
    do
	report $servers $clients
    done
done


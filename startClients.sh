declare -A CLIENT_PIDS

for ((i=1; i<=$2; i++)) 
do
  ./result -c $1 $i < data.txt &
  CLIENT_PIDS[$i]=$!
done

for ((i=1; i<=$2; i++ ))
do
  wait -f ${CLIENT_PIDS[$i]}
done



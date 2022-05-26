echo $( grep '::::' server_log.txt | tail -n 1 | sed 's/:::://' )

maxTime=0.0
for curTime in $( grep '+++' client_log_* | sed 's/^.*+++//' )
do
  [[ $( echo "$maxTime < $curTime" | bc ) -eq 1 ]] && maxTime=$curTime
done
echo "Max sum of delays for all clients was" $maxTime "seconds long"


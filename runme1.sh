rm -f /tmp/my_server_socket
rm -f server_log.txt client_log_*.txt

./result -s &

export SERVER_PID=$!

for (( i = 1; i <= 10; i++ ))
do
  source startClients.sh 0.1 100
done

./result -t

kill -SIGINT $SERVER_PID

grep '====' server_log.txt | head -n 1
grep '====' server_log.txt | tail -n 1
grep '@@@@' server_log.txt | head -n 1 
grep '@@@@' server_log.txt | tail -n 1 


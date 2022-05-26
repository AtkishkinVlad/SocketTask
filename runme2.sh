rm -f /tmp/my_server_socket
rm -f server_log.txt client_log_*.txt
./result -s &
export SERVER_PID=$!

source startClients.sh 1.0 1
source effectiveTime.sh
./result -t

source startClients.sh 0.8 10
source effectiveTime.sh
./result -t

source startClients.sh 0.6 20
source effectiveTime.sh
./result -t

source startClients.sh 0.4 50
source effectiveTime.sh
./result -t

source startClients.sh 0.2 100
source effectiveTime.sh
./result -t

kill -SIGINT $SERVER_PID



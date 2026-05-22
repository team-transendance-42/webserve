curl http://127.0.0.1:8080

// find running processes and kill desired
ps aux | grep webserv

kill 12345 or
kill -9 12345 // forcefully


// man nc
nc localhost 8080 \\enter
GET / HTTP/1.1 
echo "hello" > ./www/files/mydel.txt

curl -i -X DELETE http://127.0.0.1:8080/delete_create_file/mydel.txt


see which pr owns 8080
ss -ltnp | grep :8080

ss: Utility to dump socket statistics (shows open network connections, similar to netstat but faster and more modern).
-l: Show only listening sockets.
-t: Show TCP sockets.
-n: Show numerical addresses/ports (do not resolve names).
-p: Show process using the socket (PID/program name).
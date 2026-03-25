echo "hello" > ./www/files/mydel.txt

curl -i -X DELETE http://127.0.0.1:8080/files_auto/mydel.txt

see which pr owns 8080
ss -ltnp | grep :8080
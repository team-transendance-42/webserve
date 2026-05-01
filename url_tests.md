http://127.0.0.1:8081/api/v1
http://127.0.0.1:8081/api/health 

 no / root location. So http://127.0.0.1:8081/ gets 404
  ==========================
  port 8081 is an API server, not a general web
  server. It only handles specific API paths. Same for 8082:
  http://127.0.0.1:8082/static → serves index.html
  http://127.0.0.1:8082/downloads → directory listing
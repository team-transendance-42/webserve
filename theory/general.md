binding ports below 1024 requires root privileges on Linux — you'd need sudo

can use these ports: 8080, 8081, 8082, 8443 — common convention for HTTP dev servers.

## Virtual hosting works on the same port

Two servers sharing port `8080` is fine — the server picks which one to use via the `Host:` header the browser sends:
```
Request arrives on port 8080
        │
        ▼
Host: one        →  serve SERVER 1
Host: two        →  serve SERVER 2
Host: unknown    →  serve first server block (default)
```

To test locally, add to `/etc/hosts`:
```
127.0.0.1   one
127.0.0.1   two
127.0.0.1   three
127.0.0.1   four
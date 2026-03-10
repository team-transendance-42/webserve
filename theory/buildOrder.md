Step 1: Parse the config file          ← know what you're supposed to run
Step 2: Create & bind sockets          ← open the doors
Step 3: poll() event loop              ← listen for knocks
Step 4: Accept connections             ← let clients in
Step 5: Read & parse HTTP request      ← understand what they want
Step 6: Match server + location        ← decide who handles it
Step 7: Build & send HTTP response     ← answer them
Step 8: CGI                            ← run scripts
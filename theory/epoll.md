epoll is Linux kernel API for watching many file descriptors (mostly sockets) efficiently.
Instead of blocking on one socket, you ask kernel:
Tell me when this fd is readable
Tell me when this fd is writable
Tell me when peer hung up, errored, etc.
Then your loop reacts only to ready fds.
---------------------------------------------------------

Core epoll flow

1. epoll_create1 :  
  a.) Creates an epoll instance and returns epoll fd.

2. epoll_ctl :   
  a.) Add/modify/remove watched fds.    
  b.) Operations:      
	EPOLL_CTL_ADD			   
	EPOLL_CTL_MOD   
	EPOLL_CTL_DEL

3. epoll_wait   
  a.) Blocks (or times out) until events happen.   
  b.)  Returns a list of ready events.
----------------------------------------------------------------

tests:
Start server and open one browser tab;   
follow logs for one full read/write cycle.  
Open multiple tabs quickly; confirm only ready sockets are processed each tick.   
Test keep-alive with repeated requests; verify same fd goes read -> write -> read.  
Force disconnect mid-request; verify close path removes fd cleanly.






































Your server reads raw bytes from a socket. You must determine:
1. Where does the request end?
Headers end at the first \r\n\r\n sequence.
Body ends depends on:

Content-Length → read exactly N bytes
Transfer-Encoding: chunked → read chunks until terminal chunk
No body headers → no body (GET/DELETE)

2. Chunked Transfer Encoding
5\r\n          ← chunk size in hex
Hello\r\n      ← chunk data
6\r\n
 World\r\n
0\r\n          ← terminal chunk
\r\n
You read size (hex), then that many bytes, repeat until size is 0. Your server must handle sending chunked responses too.
3. State machine approach
Never try to parse HTTP in one pass. Use a state machine:
READING_REQUEST_LINE → READING_HEADERS → READING_BODY → DONE
Each recv() call may give you partial data. Buffer it. Only advance state when you have enough bytes.

#!/usr/bin/python3

import os
import html
import urllib.parse

params = urllib.parse.parse_qs(os.environ.get('QUERY_STRING', ''))
your_name = params.get('your_name', [None])[0]
company_name = params.get('company_name', [None])[0]

print("Content-type:text/html\n")
print("<html>")
print("<head>")
print("<title>First CGI Program</title>")
print("</head>")
print("<body>")
print("<h2>Hello, %s is working at %s</h2>"
      % (html.escape(your_name) if your_name else "N/A",
         html.escape(company_name) if company_name else "N/A"))
print("<p>This is a simple CGI script to demonstrate how to handle form data.</p>")
print("<p>Submit the form using '?your_name=YourName&company_name=YourCompany' in the URL.</p>")
print("</body>")
print("</html>")
#1/0 # test server error: 500 on crashed script
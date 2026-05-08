#!/usr/bin/python

import warnings
warnings.filterwarnings("ignore", category=DeprecationWarning)
import cgi

# Create instance of FieldStorage class which we can use to work with the submitted form data
form = cgi.FieldStorage()
your_name = form.getvalue('your_name')

# Get the data from fields
company_name = form.getvalue('company_name')

print ("Content-type:text/html\n")
print ("<html>")
print ("<head>")
print ("<title>First CGI Program</title>")
print ("</head>")
print ("<body>")
print ("<h2>Hello, %s is working at %s</h2>"
       % (your_name if your_name else "N/A",
		  company_name if company_name else "N/A"))
print("<p>This is a simple CGI script to demonstrate how to handle form data.</p>")
print("<p>Submit the form using '?your_name=YourName&company_name=YourCompany' in the URL.</p>")
print ("</body>")
print ("</html>")
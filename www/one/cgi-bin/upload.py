#!/usr/bin/env python3
import warnings

warnings.filterwarnings(
    "ignore",
    message=".*'cgi' is deprecated.*",
    category=DeprecationWarning
)

import cgi, os, sys
import html
import re

print("Content-Type: text/html\r\n\r\n")

form = cgi.FieldStorage()

if 'file' in form and form['file'].filename:
    fileitem = form['file']
    filename = os.path.basename(fileitem.filename)

    # Validate filename to prevent directory traversal
    if '..' in filename or '/' in filename or '\\' in filename:
        print("<h1>Invalid filename</h1>")
        sys.exit(1)\

    # Sanitize filename
    filename = re.sub(r'[^A-Za-z0-9._-]', '_', filename)

    upload_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'uploads')
    os.makedirs(upload_dir, exist_ok=True)

    filepath = os.path.join(upload_dir, filename)
    with open(filepath, 'wb') as f:
        f.write(fileitem.file.read())

    print(f"<h1>File uploaded: {html.escape(filename)}</h1>")
else:
    print("<h1>No file uploaded</h1>")
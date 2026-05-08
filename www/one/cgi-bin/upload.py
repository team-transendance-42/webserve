#!/usr/bin/env python3
import cgi, os, sys

print("Content-Type: text/html\r\n\r\n")

form = cgi.FieldStorage()

if 'file' in form and form['file'].filename:
    fileitem = form['file']
    filename = os.path.basename(fileitem.filename)

    # Validate filename
    if '..' in filename or '/' in filename or '\\' in filename:
        print("<h1>Invalid filename</h1>")
        sys.exit(1)

    upload_dir = './www/uploads'
    os.makedirs(upload_dir, exist_ok=True)

    filepath = os.path.join(upload_dir, filename)
    with open(filepath, 'wb') as f:
        f.write(fileitem.file.read())

    print(f"<h1>✓ File uploaded: {filename}</h1>")
    print(f"<p><a href='/upload/{filename}'>Download your file</a></p>")
else:
    print("""
    <h1>File Upload Form</h1>
    <form method="POST" enctype="multipart/form-data">
        <input type="file" name="file" required>
        <button type="submit">Upload</button>
    </form>
    """)

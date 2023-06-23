#!/usr/bin/python3

import cgi
import os
import sys

form = cgi.FieldStorage()
print("Content-type: text/plain")
print("Status: 201")
print("My-Super-Header: whatever")
print("")

if 'file' in form:
    file_item = form['file']
    if file_item.filename:
        filename = os.path.basename(file_item.filename)
        upload_dir = '../../upload'  # Specify the directory where you want to save the uploaded file
        upload_path = os.path.join(upload_dir, filename)

        print("Received file:", filename)
        print("Upload directory:", upload_dir)
        print("Upload path:", upload_path)

        with open(upload_path, 'wb') as file:
            file.write(file_item.file.read())

        print("File uploaded successfully.")
        exit(0)
    else:
        print("Error: No file selected.")
else:
    print("Error: No 'file' field in the request.")


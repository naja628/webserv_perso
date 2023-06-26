#!/usr/bin/python3

import cgi
import os
import sys

print("CGI PYTHON", file=sys.stderr)
def html_response(title, body, status = 200):
    print('Content-type: text/html')
    print('Status:', status)
    print('')
    print('<!DOCTYPE html>')
    print('<html>')
    print('<head><title>Upload</title></head>')
    print('<h1>', title, '</h1>', sep='')
    print('<body>', body, '</body>')
    print('</html>')

def respond_ok(filename):
    html_response(" Success !", "file '" + filename + "'uploaded successfully", 201)

def respond_failure(fail_msg):
    html_response(" Failure :( ", fail_msg)

form = cgi.FieldStorage()

if 'file' in form:
    file_item = form['file']
    if filename := file_item.filename:
        upload_path = os.path.join(os.environ["PATH_TRANSLATED"], file_item.filename)
        #
        print("Upload path:", upload_path, file = sys.stderr)
        try:
            with open(upload_path, 'xb') as file:
                file.write(file_item.file.read())
            respond_ok(filename)
        except FileExistsError:
            respond_failure("File {0} already exists".format(filename))
        except FileNotFoundError:
            respond_failure("Subdir does not exist")
        except:
            respond_failure("Unexpected error")
    else:
        respond_failure("No file selected")
else:
    respond_failure("Invalid request")


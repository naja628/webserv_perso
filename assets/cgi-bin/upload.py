import cgi
import os
import sys

#print("CGI PYTHON", file=sys.stderr)
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
    html_response(" Success !", f"File {filename} uploaded successfully", 201)

def respond_failure(fail_msg):
    html_response(" Failure :( ", fail_msg)

form = cgi.FieldStorage()

def try_open_and_respond(uppath, file_item):
    with open(upload_path, 'xb') as file:
        file.write(file_item.file.read())
    respond_ok(filename)


if 'file' in form:
    file_item = form['file']
    if filename := file_item.filename:
        upload_path = os.path.join(os.environ["PATH_TRANSLATED"], file_item.filename)
        #
        #print("Upload path:", upload_path, file = sys.stderr)
        try:
            try_open_and_respond(upload_path, file_item)
        except FileExistsError:
            respond_failure(f"File {filename} already exists")
        except FileNotFoundError:
            try: 
                os.makedirs(os.path.dirname(upload_path))
                try_open_and_respond(upload_path, file_item)
            except OSError:
                respond_failure("Could not create subdir")
        except:
            respond_failure("Unexpected error")
    else:
        respond_failure("No file selected")
else:
    respond_failure("Invalid request")


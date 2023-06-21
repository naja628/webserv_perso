#!/usr/bin/python3

import os
import sys

# Retrieve the query string from the environment variable
query_string = os.environ.get('QUERY_STRING', '')

# Parse the query string to extract the input values
params = {}
for param in query_string.split('&'):
    if '=' in param:
        key, value = param.split('=', 1)
        params[key] = value

# Get the values for num1 and num2, and set default values if they are missing
num1 = params.get('num1', '')
num2 = params.get('num2', '')

# Perform the addition
try:
    result = int(num1) + int(num2)
except ValueError:
    result = None

# Generate the HTML response
html_response = '''
<!DOCTYPE html>
<html>
<head>
    <title>Addition Result</title>
</head>
<body>
    <h1>Addition Result</h1>
'''

if result is not None:
    html_response += '<p>{0} + {1} = {2}</p>'.format(num1, num2, result)
else:
    html_response += '<p>Invalid input. Please provide two numbers.</p>'

html_response += '''
</body>
</html>
'''

# Print the HTML response to standard output
sys.stdout.write(html_response)


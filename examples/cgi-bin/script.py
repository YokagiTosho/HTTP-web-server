#!/usr/bin/env python

import os
import sys

print('Status: 200 OK')
print('Content-Type: text/html')
print('')

print(f'<h1>{sys.argv}</h1><p>{os.environ}</p>')
print(f'<div><a href="/">Go to home</a></div>')

exit(0)

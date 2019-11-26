#!/usr/bin/env python3

import urllib.request
import base64

def download(url, path, username=None, password=''):
  request = urllib.request.Request(url)
  if username != None:
    basic_auth = base64.b64encode(('%s:%s' % (username, password)).encode('utf-8'))
    request.add_header('Authorization', 'Basic %s' % basic_auth.decode('utf-8'))
  response = urllib.request.urlopen(request)
  data = response.read()
  with open(path, 'wb') as f:
    f.write(data)

if __name__ == '__main__':

  import sys
  import time

  url = 'http://[<FIXME>]:<FIXME>'
  username = '<FIXME>'
  password = '<FIXME>'

  if len(sys.argv) == 2:
    pad = 'gameboy'
  elif len(sys.argv) == 3:
    pad = sys.argv[2]
  else:
    print('%s <output-file> [<pad-name>]' % sys.argv[0])
    sys.exit(1)

  path = sys.argv[1]

  #FIXME: Avoid exploits
  export_url = url + '/p/' + pad + '/export/'

  # Keep a backup of the pad
  #FIXME: Handle errors
  download(export_url + 'etherpad', '%d_%s.etherpad' % (int(time.time()), pad), username, password)

  # Download the source code every time
  #FIXME: Handle errors
  download(export_url + 'txt', path, username, password)

  sys.exit(0)

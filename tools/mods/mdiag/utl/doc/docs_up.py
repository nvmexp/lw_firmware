# Usage: docs_up.py {page_id} {path_to_file}
import requests
from requests.auth import HTTPBasicAuth
import json
import sys
import io
import getpass


def check_response(response, errstr):
    if response.status_code != requests.codes.ok:
        print(f'ERROR: {errstr}')
        print(f'Error code: {response.status_code}')
        print('Returned data:')
        print(json.dumps(json.loads(response.text), sort_keys=True, indent=4, separators=(',', ': ')))
        sys.exit(1)


print(f'Uploading {sys.argv[2]} to Confluence page with ID {sys.argv[1]}')

conf_url = f'https://confluence.lwpu.com/rest/api/content/{sys.argv[1]}'
# fill in credentials here
conf_auth = HTTPBasicAuth('svc-utl-confluence', 'password123')

# get current version number and title
headers = {
    'Accept': 'application/json'
}
response = requests.get(
    conf_url,
    headers=headers,
    auth=conf_auth
)
check_response(response, 'Could not get version number of Confluence page!')
version = json.loads(response.text)['version']['number'] + 1
title = json.loads(response.text)['title']
print(f'Version number: {version}')

with io.open(sys.argv[2], 'r', encoding='utf-8') as f:
    # upload file contents as payload
    headers = {
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    }
    payload = json.dumps({
      'version': {
        'number': version
      },
      'title': title,
      'type': 'page',
      'status': 'current',
      'body': {
        'storage': {
          'value': f.read(),
          'representation': 'storage'
        },
      }
    })
    response = requests.put(
       conf_url,
       data=payload,
       headers=headers,
       auth=conf_auth
    )
    check_response(response, 'Confluence upload failed!')

print('Done!')

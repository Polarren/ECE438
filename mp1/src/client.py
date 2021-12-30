
import socket
import sys
import time
import requests

url = "10.0.2.15:3490/index.html"

r = requests.get(url)
print(r.content) 
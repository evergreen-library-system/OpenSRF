#!/bin/sh

# Simple demonstration of invoking a public OpenSRF method via curl

# Expected output will be something like:
# [{"__c":"osrfMessage","__p":{"threadTrace":"0","locale":"en-CA","type":"RESULT","payload":{"__c":"osrfResult","__p":{"status":"OK","statusCode":"200","content":4}}}},{"__c":"osrfMessage","__p":{"threadTrace":"0","locale":"en-CA","type":"STATUS","payload":{"__c":"osrfConnectStatus","__p":{"status":"Request Complete","statusCode":"205"}}}}]

curl -H "X-OpenSRF-service: opensrf.math" --data 'osrf-msg=[{"__c":"osrfMessage","__p":{"threadTrace":0,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"add","params":[2,2]}},"locale":"en-CA"}}]' http://localhost/osrf-http-translator

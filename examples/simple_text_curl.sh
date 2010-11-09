#!/bin/sh

# Simple demonstration of invoking a public OpenSRF method via curl

# Expected output will be something like:
# [{"__c":"osrfMessage","__p":{"threadTrace":"0","locale":"en-CA","type":"RESULT","payload":{"__c":"osrfResult","__p":{"status":"OK","statusCode":"200","content":4}}}},{"__c":"osrfMessage","__p":{"threadTrace":"0","locale":"en-CA","type":"STATUS","payload":{"__c":"osrfConnectStatus","__p":{"status":"Request Complete","statusCode":"205"}}}}]

#curl -H "X-OpenSRF-service: opensrf.simple-text" --data 'osrf-msg=[{"__c":"osrfMessage","__p":{"threadTrace":0,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"opensrf.simple-text.reverse","params":["foobar"]}},"locale":"en-CA"}}]' http://localhost/osrf-http-translator
curl -H "X-OpenSRF-service: opensrf.simple-text" --data 'osrf-msg=[{"__c":"osrfMessage","__p":{"threadTrace":0,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"opensrf.simple-text.split","params":["This is a test, it%27s only a test"]}},"locale":"en-CA"}}]' http://localhost/osrf-http-translator
#curl -H "X-OpenSRF-service: opensrf.simple-text" --data 'osrf-msg=[{"__c":"osrfMessage","__p":{"threadTrace":0,"type":"REQUEST","payload":{"__c":"osrfMethod","__p":{"method":"opensrf.simple-text.substring","params":["foobar", 3, 1]}},"locale":"en-CA"}}]' http://localhost/osrf-http-translator

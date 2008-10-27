#!/usr/bin/env python
import osrf.system
import osrf.ses

# XXX: Replace with command line arguments
file = '/openils/conf/opensrf_core.xml'
operator = 'add'
operand1 = 5
operand2 = 7

# Pull connection settings from <config><opensrf> section of opensrf_core.xml
osrf.system.System.connect(config_file=file, config_context='config.opensrf')

# Set up a connection to the opensrf.math service
session = osrf.ses.ClientSession('opensrf.math')

# Call one of the methods defined by the opensrf.math service
request = session.request(operator, operand1, operand2)

# Retrieve the response from the method
response = request.recv(timeout=2)

print(response.content())

# Cleanup request and connection resources
request.cleanup()
session.cleanup()

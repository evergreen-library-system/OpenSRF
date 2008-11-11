#!/bin/bash
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
 

OPT_ACTION=""
OPT_CONFIG=""
OPT_PID_DIR=""

# ---------------------------------------------------------------------------
# Make sure we're running as the correct user
# ---------------------------------------------------------------------------
[ $(whoami) != 'opensrf' ] && echo 'Must run as user "opensrf"' && exit;


function usage {
	echo "";
	echo "usage: $0 [OPTION]... -c <c_config> -a <action>";
	echo "";
	echo "Mandatory parameters:";
	echo -e "  -a\t\taction to perform";
	echo "";
	echo "Optional parameters:";
	echo -e "  -c\t\tfull path to C configuration file (opensrf_core.xml)";
	echo -e "  -d\t\tstore PID files in this directory";
	echo -e "  -l\t\taccept 'localhost' as the fully-qualified domain name";
	echo "";
	echo "Actions include:";
	echo -e "\tstart_router"
	echo -e "\tstop_router"
	echo -e "\trestart_router"
	echo -e "\tstart_perl"
	echo -e "\tstop_perl"
	echo -e "\trestart_perl"
	echo -e "\tstart_c"
	echo -e "\tstop_c"
	echo -e "\trestart_c"
	echo -e "\tstart_osrf"
	echo -e "\tstop_osrf"
	echo -e "\trestart_osrf"
	echo -e "\tstop_all" 
	echo -e "\tstart_all"
	echo -e "\trestart_all"
	echo "";
	echo "Examples:";
	echo "  $0 -a restart_all";
	echo "  $0 -l -c opensrf_core.xml -a restart_all";
	echo "";
	exit;
}

# Get root directory of this script
function basepath {
	BASEDIR=""
	script_path="$1"
	IFS="/"
	for p in $script_path
	do
		if [ -z "$BASEDIR" ] && [ -n "$p" ]; then
			BASEDIR="$p"
		fi;
	done
	BASEDIR="/$BASEDIR"
	IFS=
}

basepath $0

# ---------------------------------------------------------------------------
# Load the command line options and set the global vars
# ---------------------------------------------------------------------------
while getopts  "a:d:c:lh" flag; do
	case $flag in	
		"a")		OPT_ACTION="$OPTARG";;
		"c")		OPT_CONFIG="$OPTARG";;
		"d")		OPT_PID_DIR="$OPTARG";;
		"l")		export OSRF_HOSTNAME="localhost";;
		"h"|*)	usage;;
	esac;
done

OSRF_CONFIG=`find $BASEDIR -name osrf_config`

[ -z "$OPT_CONFIG" ] && OPT_CONFIG=`$OSRF_CONFIG --sysconfdir`/opensrf_core.xml;
if [ ! -r "$OPT_CONFIG" ]; then
	echo "Please specify the location of the opensrf_core.xml file using the -c flag";
	exit 1;
fi;
[ -z "$OPT_PID_DIR" ] && OPT_PID_DIR=`$OSRF_CONFIG --localstatedir`/run;
[ -z "$OPT_ACTION" ] && usage;

PID_ROUTER="$OPT_PID_DIR/router.pid";
PID_OSRF_PERL="$OPT_PID_DIR/osrf_perl.pid";
PID_OSRF_C="$OPT_PID_DIR/osrf_c.pid";


# ---------------------------------------------------------------------------
# Utility code for checking the PID files
# ---------------------------------------------------------------------------
function do_action {

	action="$1"; 
	pidfile="$2";
	item="$3"; 

	if [ $action == "start" ]; then

		if [ -e $pidfile ]; then
			pid=$(cat $pidfile);
			echo "$item already started : $pid";
			return 0;
		fi;
		echo "Starting $item";
	fi;

	if [ $action == "stop" ]; then

		if [ ! -e $pidfile ]; then
			echo "$item not running";
			return 0;
		fi;

        while read pid; do
            echo "Stopping $item process $pid..."
            kill -s INT $pid
        done < $pidfile;
		rm -f $pidfile;

	fi;

	return 0;
}


# ---------------------------------------------------------------------------
# Start / Stop functions
# ---------------------------------------------------------------------------


function start_router {
	do_action "start" $PID_ROUTER "OpenSRF Router";
	opensrf_router $OPT_CONFIG routers
	pid=$(ps ax | grep "OpenSRF Router" | grep -v grep | awk '{print $1}')
	echo $pid > $PID_ROUTER;
	return 0;
}

function stop_router {
	do_action "stop" $PID_ROUTER "OpenSRF Router";
	return 0;
}

function start_perl {
    echo "Starting OpenSRF Perl";
    opensrf-perl.pl --verbose --pid-dir $OPT_PID_DIR \
        --config $OPT_CONFIG --action start_all --settings-startup-pause 3
	return 0;
}

function stop_perl {
    echo "Stopping OpenSRF Perl";
    opensrf-perl.pl --verbose --pid-dir $OPT_PID_DIR --config $OPT_CONFIG --action stop_all
	sleep 1;
	return 0;
}

function start_c {
	host=$OSRF_HOSTNAME
	if [ "_$host" == "_" ]; then
		host=$(perl -MNet::Domain=hostfqdn -e 'print hostfqdn()');
	fi;

	do_action "start" $PID_OSRF_C "OpenSRF C (host=$host)";
	opensrf-c $host $OPT_CONFIG opensrf;
	pid=$(ps ax | grep "OpenSRF System-C" | grep -v grep | awk '{print $1}')
	echo $pid > "$PID_OSRF_C";
	return 0;
}

function stop_c {
	do_action "stop" $PID_OSRF_C "OpenSRF C";
	sleep 1;
	return 0;
}



# ---------------------------------------------------------------------------
# Do the requested action
# ---------------------------------------------------------------------------
case $OPT_ACTION in
	"start_router") start_router;;
	"stop_router") stop_router;;
	"restart_router") stop_router; start_router;;
	"start_perl") start_perl;;
	"stop_perl") stop_perl;;
	"restart_perl") stop_perl; start_perl;;
	"start_c") start_c;;
	"stop_c") stop_c;;
	"restart_c") stop_c; start_c;;
	"start_osrf") start_perl; start_c;;
	"stop_osrf") stop_perl; stop_c;;
	"restart_osrf") stop_perl; stop_c; start_perl; start_c;;
	"stop_all") stop_c; stop_perl; stop_router;;
	"start_all") start_router; start_perl; start_c;;
	"restart_all") stop_c; stop_perl; stop_router; start_router; start_perl; start_c;;
	*) usage;;
esac;




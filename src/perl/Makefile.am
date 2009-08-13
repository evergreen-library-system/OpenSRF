# Copyright (C) 2009 Equinox Software, Inc.
# Shawn Boyette <sboyette@esilibrary.com>
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

all:
	perl Build.PL || make -s install-perl-fail

install-perl-fail:
	echo
	echo ">>> Installation of Perl modules has failed. The most likely"
	echo ">>> possibility is that a dependency is not pre-installed"
	echo ">>> or that a test has failed."
	echo ">>> See the messages above this one for more information."
	echo
	exit 1

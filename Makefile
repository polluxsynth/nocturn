#
#
# Copyright (C) 2014  Ricard Wanderlof <ricard2013@butoba.net>
# Portions of Makefile Copyright (C) Lars Luthman <larsl@users.sourceforge.net>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

PROGNAME = nocturn

PREFIX = /usr/local
DOC_DIR = $(PREFIX)/share/doc/$(PROGNAME)
BIN_DIR = $(PREFIX)/bin
UI_DIR = $(PREFIX)/share/$(PROGNAME)
UDEV_DIR = /etc/udev/rules.d

# For development, we keep everything in the same (development) directory
UI_DIR=.

OBJS = nocturn.o
INCS = 
UI_FILES = 
DOC_FILES = README COPYING
UDEV_FILES = 40-nocturn.rules

all: $(PROGNAME)

%.o: %.c $(INCS) Makefile
	gcc -Werror -c -o $@ $< `pkg-config --cflags libusb-1.0 alsa` -DUI_DIR=\"$(UI_DIR)\" -g -O2

$(PROGNAME): $(OBJS)
	@echo $(OBJS)
	gcc -Werror -o $@ $^ `pkg-config --libs libusb-1.0 alsa`

clean:
	rm -f $(PROGNAME) $(OBJS) *~

install: $(PROGNAME)
	#install -d $(BIN_DIR) $(UI_DIR) $(DOC_DIR)
	#install $(PROGNAME) $(BIN_DIR)
	#install $(UI_FILES) $(UI_DIR)
	#install $(DOC_FILES) $(DOC_DIR)
	install $(UDEV_FILES) $(UDEV_DIR)

uninstall:
	rm $(BIN_DIR)/$(PROGNAME)
	rm -r $(UI_DIR) $(DOC_DIR)

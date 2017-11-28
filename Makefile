SHELL=/bin/sh
OS=$(shell uname)

# WARNING: If you get errors here using 'make', be sure you're using GNU make.
#          Some OS's stock make (e.g. FreeBSD) is /not/ GNU make.
#
include Makefile.$(OS)

CXXFLAGS += -DSPOOL_DIR=\"$(SPOOL_DIR)\"
CXXFLAGS += -DCONFIG_FILE=\"$(CONFIG_DIR)\"
CXXFLAGS += -DSENDMAIL=\"$(SENDMAIL)\"
CXXFLAGS += -g
LDFLAGS   =
NROFF     = nroff
VERSION   = `cat VERSION.H | sed 's/^[^"]*"//' | sed 's/"//'`


# Rules
.SUFFIXES: .5 .8 .C .H .pod .man .o

# Default build
all: newsd man

# Clean
clean:
	echo Cleaning all files
	-rm -f core*
	-rm -f *.o
	-rm -f newsd
	-rm -f newsd.8 newsd.conf.8
	-rm -f newsd.html newsd.conf.html

# Build Newsd

Article.o: Article.C Article.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Article.C
 
Configuration.o: Configuration.C Configuration.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Configuration.C

Group.o: Group.C Group.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Group.C

newsd.o: newsd.C everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c newsd.C

newsd:  newsd.o Article.o Configuration.o Group.o Server.o
	$(CXX) $(LDFLAGS) newsd.o Article.o Configuration.o Group.o Server.o -o newsd

# Build man pages

man: newsd.pod newsd.conf.pod
	pod2man --center "newsd Documentation" --section=8 newsd.pod | \
	    $(NROFF) -man > newsd.8
	pod2man --center "newsd Documentation" --section=8 newsd.conf.pod | \
	    $(NROFF) -man > newsd.conf.8

# Build html pages
html: newsd.pod newsd.conf.pod
	pod2html newsd.pod > newsd.html
	pod2html newsd.conf.pod > newsd.conf.html

FORCE:

SHELL=/bin/sh
OS=$(shell uname)

include Makefile.$(OS)

CXXFLAGS += -DSPOOL_DIR=\"$(SPOOL_DIR)\"
CXXFLAGS += -DCONFIG_FILE=\"$(CONFIG_FILE)\"
CXXFLAGS += -DSENDMAIL=\"$(SENDMAIL)\"
CXXFLAGS += -g
LDFLAGS   =
NROFF     = nroff
POD2MAN   = pod2man --center "newsd Documentation"
VERSION   = `cat VERSION.H | sed 's/^[^"]*"//' | sed 's/"//'`

# Default build
all: newsd man html

# Clean
clean:
	@echo Cleaning all files
	-rm -f core*
	-rm -f *.o
	-rm -f pod2*.tmp
	-rm -f newsd
	-rm -f newsd.8 newsd.conf.8
	-rm -f newsd.html newsd.conf.html

# Build Newsd
Subs.o: Subs.C Subs.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Subs.C

Article.o: Article.C Article.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Article.C
 
Configuration.o: Configuration.C Configuration.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Configuration.C

Server.o: Server.C Server.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Server.C

Group.o: Group.C Group.H everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c Group.C

newsd.o: newsd.C everything.H VERSION.H
	$(CXX) $(CXXFLAGS) -c newsd.C

newsd:  newsd.o Subs.o Article.o Configuration.o Group.o Server.o
	$(CXX) $(LDFLAGS) newsd.o Subs.o Article.o Configuration.o Group.o Server.o -o newsd

# Build man pages
man: newsd.pod newsd.conf.pod
	$(POD2MAN) --section=8 newsd.pod      > newsd.8
	$(POD2MAN) --section=8 newsd.conf.pod > newsd.conf.8

# Build html pages
html: newsd.pod newsd.conf.pod
	pod2html newsd.pod      > newsd.html
	pod2html newsd.conf.pod > newsd.conf.html

# Install
install: newsd man html
	( killall newsd; exit 0) 2> /dev/null
	cp newsd  $(BIN_DIR)/newsd
	chmod 755 $(BIN_DIR)/newsd
	@if [ ! -e $(CONFIG_FILE) ]; then \
	    @echo Installing new $(CONFIG_FILE); \
	    cat newsd.conf | sed 's%^ErrorLog.*%ErrorLog $(LOG_DIR)/newsd.log%' | \
	                     sed 's%^SendMail.*%SendMail $(SENDMAIL) -t%'       | \
	                     sed 's%^SpoolDir.*%SpoolDir $(SPOOL_DIR)%'         | \
	                     cat > $(CONFIG_FILE); \
	else \
	    echo "NOT installing $(CONFIG_FILE) (already exists)"; \
	fi
	@echo Installing manpages in $(MAN_DIR)
	cp newsd.8      $(MAN_DIR)
	cp newsd.conf.8 $(MAN_DIR)

# Undo whatever install did
uninstall: FORCE
	-rm $(BIN_DIR)/newsd
	-rm $(CONFIG_FILE)
	-rm $(MAN_DIR)/newsd.8
	-rm $(MAN_DIR)/newsd.conf.8

FORCE:

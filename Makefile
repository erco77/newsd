SHELL=/bin/sh
OS=$(shell uname)

include Makefile.$(OS)

CXXFLAGS += -DSPOOL_DIR=\"$(SPOOL_DIR)\"
CXXFLAGS += -DCONFIG_FILE=\"$(CONFIG_FILE)\"
CXXFLAGS += -DSENDMAIL=\"$(SENDMAIL)\"
CXXFLAGS += -g
LDFLAGS   =
NROFF     = nroff
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
	pod2html newsd.pod      > newsd.html
	pod2html newsd.conf.pod > newsd.conf.html

# Install
install: newsd man html
	cp newsd  ${BIN_DIR}/newsd
	chmod 755 ${BIN_DIR}/newsd
	cat newsd.conf |					  \
	    sed 's%^ErrorLog.*%ErrorLog ${LOG_DIR}/newsd.log%'	| \
	    sed 's%^SendMail.*%SendMail ${SENDMAIL} -t%' 	| \
	    sed 's%^SpoolDir.*%SpoolDir ${SPOOL_DIR}%'		| \
	    cat > ${CONFIG_FILE}
	cp newsd.8      ${MAN_DIR}
	cp newsd.conf.8 ${MAN_DIR}

# Undo whatever install did
uninstall: FORCE
	-rm ${BIN_DIR}/newsd
	-rm ${CONFIG_FILE}
	-rm ${MAN_DIR}/newsd.8
	-rm ${MAN_DIR}/newsd.conf.8

FORCE:

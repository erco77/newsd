# newsd
newsd -- Standalone Local NNTP News Server
------------------------------------------

WHAT IS NEWSD?
    
    Newsd is a single host NNTP news server for managing private
    newsgroups (NOT a part of usenet).
    
    It is useful for serving private newsgroup(s) to an intranet
    or the Internet. It can receive postings via NNTP or via a
    mail gateway.

    Messages can be read and replied to using NNTP clients;
    tested with Thunderbird and Opera. One can also create
    web tools to monitor the newsgroup spooler directly,
    or via NNTP.
    
    Newsd does /NOT/ interface with other news servers, and cannot
    manage distributed news feeds, i.e. Usenet news. This is what
    keeps newsd simple, acting as a single server for newsgroups
    that NNTP clients can connect to for interaction.
    

LICENSING

    Newsd comes with complete free source code.  Newsd is
    available under the terms of the GNU General Public
    License.  See the file "LICENSE" for more info.


BUILD INSTRUCTIONS

    Run GNU make to build the newsd binary:

	make
	
    (NOTE: on some systems 'gnu make' is called 'gmake')

    Currently newsd reverted to old school separate Makefiles
    to manage supporting different operating systems. (I find
    autoconf too complicated, and haven't fully learned cmake yet)

    The correct file will be loaded automatically (if it exists), e.g. 
    	
	Makefile.Linux		-- linux presets
	Makefile.FreeBSD	-- FreeBSD presets

    If your OS is not offered, run 'uname' to find the name
    of your operating system, and make copy one of the files
    most like your OS, e.g.

	$ uname
	OpenBSD

    	$ cp Makefile.FreeBSD Makefile.OpenBSD

    Then edit the new file, and make any necessary customizations.

    If you want newsd to default to using a particular directory
    hierarchy, e.g. $HOME/newsd/test, edit your Makefile.XXX
    file (XXX being your operating system's name), and change
    the settings, e.g.

	$ vi Makefile.Linux
	____________________________________________________
	# Linux settings
	CXX          = g++
	BIN_DIR     := /usr/myhome/newsd/test/bin             << Make these
	CONFIG_FILE := /usr/myhome/newsd/test/etc/newsd.conf  << changes.
	SPOOL_DIR   := /usr/myhome/newsd/test/spool           << Be sure to use
	LOG_DIR     := /usr/myhome/newsd/test/log             << absolute paths
	MAN_DIR     := /usr/myhome/newsd/test/man             << for each setting.
	SENDMAIL    := /usr/sbin/sendmail
	____________________________________________________

    Once configured, these two commands will create the 'test'
    directory hierarchy, and builds/installs newsd into it:

	$ mkdir -p /usr/myhome/newsd/test/{bin,etc,spool,log,man}

	$ make clean all install

    This will preconfigure the newsd.conf file and compiled-in
    defaults into newsd to use that directory layout:

	    $HOME/newsd/test/bin/newsd		-- the newsd executable
	    $HOME/newsd/test/etc/newsd.conf	-- a pre-configured newsd.conf
	    $HOME/newsd/test/log/		-- newsd.log will be written here
	    $HOME/newsd/test/spool/		-- newsgroups will be managed here
	    $HOME/newsd/test/man/		-- manual pages

    To uninstall newsd from that directory hierarchy, use:

    	make uninstall

INSTALL INSTRUCTIONS

    Once the software builds (see above), you should be able to
    install the software with:

    	make install

    Then edit the installed newsd.conf file to make any needed
    changes for your setup.

    Then try running the daemon in foreground mode with debugging
    enabled, to see if it likes your settings. If it does, it will
    continue running, printing diagnostic messages whenever NNTP
    clients connect to it:

    	./newsd -d -f

    ..or to run it in the background as a daemon, then just:

        ./newsd

    ..which will log output to ${LOG_DIR}/newsd.log

CREATING NEWSGROUPS

    To create a few empty newsgroups, you can use:

    	./newsd -newgroup

    ..and just answer the questions. 
    
    Once a new group is created, any news clients should
    immediately be able to subscribe to the new group
    and if posting is enabled, post messages to it.

    Or, you can just as easily use unix commands to do
    what 'newsd -newgroup' does. For example, to create
    a rush.test newsgroup:

    	mkdir -p ${SPOOL_DIR}/rush/test

	( echo description Test group;
          echo creator     John Doe;
	  echo postok      1;
          echo postlimit 0 ) > ${SPOOL_DIR}/rush/test/.config

MAIL GATEWAY

    Email messages can be injected into the newsd groups
    using e.g.

        cat email_message | ./newsd -mailgateway rush.general

    ..which would add the text contents of email_message to
    the newsgroup 'rush.general'.

    You can use 'newsd -mailgateway <GROUP_NAME>' as a mail
    forward command for various email addresses, one email
    address per group, so that e.g. mailing lists can be
    forwarded into the newsd groups.

DOCUMENTATION

    There's documentation in both man page format and HTML:

    	make man	-- makes manual pages (ending in *.8)
	make html	-- makes html docs (ending in *.html)
    
    'make install' should automatically create and install
    the man pages.

FEATURES

    Newsd provides simple file-based system administration.  No
    satellite binaries or scripts are needed to install,
    configure, administer, or maintain newsd.


LIMITATIONS
   
    Refer to the "newsd" man page.

HISTORY
    
    This tool was originally written by Greg Ercolano in January 2003
    to manage newsgroups for his commercial product "Rush". At that time
    the project was managed as a series of tar files:
    http://seriss.com/people/erco/unixtools/newsd/

    In 2004, Mike Sweet became interested in using newsd to replace
    innd to manage the FLTK project's newsgroup, and later the CUPS 
    series of newsgroups. Mike put newsd into service by November 3 2004.

    Since then, FLTK and CUPS newsgroups were managed by newsd 1.44
    between June 2005 and May 2013. In May 2013, Mike's server at easysw
    crashed.
    
    Greg took over the FLTK website and moved the two main newsgroups
    to google groups (mainly to manage spam better), but left newsd
    in place to act as an NNTP mirror of the google groups, and to manage
    the bugs, commits and administration newsgroups.

    Since 2013, Greg has returned to maintaining newsd, and in 2017
    put it up on github as: https://github.com/erco77/newsd, where its
    public access can continue. The autoconf and web tools that Mike
    added were removed to return the project to a simpler code base.

REPORTING BUGS

    Please use github's issue page:

        https://github.com/erco77/newsd/issues

    ..or email Greg directly: erco@seriss.com


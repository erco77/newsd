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

    Run the following commands to build the newsd software:

	./configure --prefix=/some/directory
	make

    Once the software is compiled, use the following command to
    install it:

	make install


DOCUMENTATION

    See the "newsd" and "newsd.conf" man pages for more information.


FEATURES

    Newsd provides simple file-based system administration.  No
    satellite binaries or scripts are needed to install,
    configure, administer, or maintain newsd.


LIMITATIONS
   
    Refer to the "newsd" man page.

HISTORY
    
    This tool was originally written by Greg Ercolano in January 2003
    to manage a newsgroup for his commercial product, Rush. 
    The newsd project was at that time managed as a set of tar files:
    http://seriss.com/people/erco/unixtools/newsd/

    In 2004, Mike Sweet became interested in using newsd for managing the
    FLTK project's newsgroup, having had too many problems with using innd
    (the Usenet server), and put it into service around November 3 2004.

    Since then, all the fltk newsgroups were managed by newsd 1.44
    between June 2005, and May 2013, when Mike's server at easysw
    crashed. Greg took over the FLTK website and newsgroup management,
    moving the two main newsgroups over to google groups (mainly to
    handle spam prevention), leaving newsd as a readonly NNTP mirror
    of the google groups.

    Since 2013, Greg has returned to maintaining newsd, and in 2017
    putting 1.44 up on github as: https://github.com/erco77/newsd
    where its public access can continue. The autoconf and web tools
    were removed to return the project to simpler management.

REPORTING BUGS

    Please use github's issue page:

        https://github.com/erco77/newsd/issues


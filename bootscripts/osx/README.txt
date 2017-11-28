CONFIGURING NEWSD TO START ON BOOT
==================================

Newer versions of OSX prefer launchd starts daemons
with a plist file in /Library/LaunchDaemons/<filename>.plist

Assuming newsd has been installed with 'make install', and
verified it runs OK by testing it from a shell with:

     newsd -f -d

..then to configure it to start on boot, use these steps:

    1. Copy the com.seriss.newsd.plist file into the LaunchDaemons
       directory.

            sudo cp com.seriss.newsd.plist /Library/LaunchDaemons/
	    sudo chmod 644 /Library/LaunchDaemons/com.seriss.newsd.plist

    2. Use Apple's 'launchctl' command to enable the new file:

	    sudo launchctl load /Library/LaunchDaemons/com.seriss.newsd.plist

    3. The daemon should now be running. Check the log to see if there
       are any error messages, e.g.

            tail -f /var/log/newsd.log

    4. Configure a weekly cron entry to rotate the log.
       Under OSX, the best way is to edit roots crontab, e.g.

       		sudo crontab -e

       ..and then add this entry:
	
		5 0 * * 0  ( /usr/sbin/newsd -rotate 2>&1 ) | logger -t newsd

       ..this configures cron to rotate the log every Sunday morning at 12:05am.
       Any errors from this operation are added to the system log.

    5. That's it.

       Assuming you've creates newsgroups with 'newsd -newgroup',
       the groups should be available to NNTP clients.

       You should be able to use launchctl to manage newsd, like any
       other OSX daemons.

For questions, contact erco@seriss.com


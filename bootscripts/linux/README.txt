CONFIGURING NEWSD TO START ON BOOT
==================================

Systemd or init? Note that even if your OS uses systemd,
it still supports the /etc/init.d/ approach to starting
daemons.

Assuming newsd has been installed with 'make install',
and verified it runs OK by testing it from a shell with:

     newsd -f -d

..then to configure it to start on boot, use these steps:

    1. Copy the ./newsd-boot script to /etc/init.d/newsd

            sudo cp newsd-boot /etc/init.d/newsd
	    sudo chmod 755     /etc/init.d/newsd

    2. Create 'start' (S99) and 'kill' (K01) boot links.
       Stops the daemon for run levels 0,1,6, and starts
       the daemon for run levels 2-5:

	    sudo ln -s /etc/init.d/newsd /etc/rc0.d/K01newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc1.d/K01newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc6.d/K01newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc2.d/S99newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc3.d/S99newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc4.d/S99newsd
	    sudo ln -s /etc/init.d/newsd /etc/rc5.d/S99newsd

    3. To start the daemon:

	    sudo /etc/init.d/newsd start

    4. Check the daemon log for errors.

            tail -f /var/log/newsd.log

    5. Configure a weekly cron entry to rotate the log.
       Add this entry to your /etc/crontab

	    5 0 * * 0   root  ( /usr/sbin/newsd -rotate 2>&1 ) | logger -t newsd

       ..This rotates the log every Sunday morning at 12:05am.
       Any errors from this operation are added to the system log.

    6. That's it.

       Assuming you've creates newsgroups with 'newsd -newgroup',
       the groups should be available to NNTP clients.

For questions, contact erco@seriss.com


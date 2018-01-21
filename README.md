# CUPS-PDF v3 with text print support
This is a [cups-pdf v3.0.1](http://www.cups-pdf.de/src/cups-pdf_3.0.1.tar.gz) with a patch to allow printing text as text, not as images. This issue is also known as "cups-pdf not embedding text", or "[producing large PDFs with text that can not be searched](https://bugs.launchpad.net/ubuntu/+source/cups-pdf/+bug/366949)"

This is fix is achieved by adding PDF passthrough functionality, so incoming PDFs remain PDFs, instead of being converted from [PDF to PostScript, then back to PDF](https://bugs.launchpad.net/ubuntu/+source/cups-pdf/+bug/820820). Why is this fix not in the original code? The author had different goals for CUPS-PDF in mind, [refusing to fix it.](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=658004)

### How is this better than a Save-as-PDF or Print-as-file functionality in many GTK/QT interfaces?

1. You can do post-processing, for example choose pdf filename and saving location in a standard file save dialog, and automatically open generated PDF.
2. Use PDF printing from non GTK/QT apps, like Wine
3. Provide consistent PDF printing experience


Installing
----------
There is a cups-pdf version 2.6.1 with the fix in an easy to install deb patch form in [ppa:jethrogb/ppa](https://launchpad.net/~jethrogb/+archive/ubuntu/ppa). Add it to your list of apt sources and update.
For Arch Linux you can find it in AUR: [cups-pdf-to-pdf-git](https://aur.archlinux.org/packages/cups-pdf-to-pdf-git/)

For all the other system, or if you want the latest functionality follow the directions below:


1. Get the development prerequisites

``apt-get install libcups2-dev``

2. Compile

``gcc -O9 -s  -o cups-pdf cups-pdf.c -lcups``


(note the different order of options than the one suggested on the cups-pdf website)

3. Copy to your system folder

```
	sudo cp /usr/lib/cups/backend/cups-pdf /usr/lib/cups/backend/cups-pdf.bak
	sudo cp cups-pdf /usr/lib/cups/backend/
```

4. Copy CUPS-PDF_opt.ppd to your CUPS model directory

``sudo cp CUPS-PDF_opt.ppd /usr/share/cups/model``

5. Copy cups-pdf.conf to /etc/cups (and then edit as needed):

``sudo cp cups-pdf.conf /etc/cups/``

6. Remove CUPS-PDF printer, if you have any, and recreate it. Make sure to pick "Generic CUPS-PDF Printer (w/ options)" as the driver


Troubleshooting
---------------

### The options from /etc/cups/cups-pdf.conf are ignored
If you create the printer with the URL like this: <cups-pdf://localhost>, then it will be looking for a file cups-pdf-/localhost.conf
Change the URL to cups-pdf:/ (it is a valid url after it's created, but you might not be able to use it *during* creation). It will then look for cups-pdf.conf

### Apparmor is getting in the way
``sudo vi /etc/apparmor.d/usr.sbin.cupsd``


1. At the end of the ``/usr/sbin/cupsd flags=(attach_disconnected)`` section add

```
	unix peer=(label=/usr/lib/cups/backend/cups-pdf),
	signal peer=/usr/lib/cups/backend/cups-pdf,
```


2. At the end of the ``/usr/lib/cups/backend/cups-pdf`` section add

```
	/var/log/cups/ r,
	/var/log/cups/** rwk,
	/etc/cups/ r,
	unix peer=(label=/usr/sbin/cupsd),
	signal peer=/usr/sbin/cupsd,
	@{HOME}/bin/pdfpostproc.sh rUx,
```

(last line is needed if you indicated a post-processor in your /etc/cups/cups-pdf.conf)


3. Reload apparmor profile

``sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.cupsd ``

Other
-----

* For other issues refer to the [original README](http://www.cups-pdf.de/cups-pdf-CURRENT/README) or [the original documentation](http://www.cups-pdf.de/documentation.shtml)
* Kudos to [Björgvin Ragnarsson](https://launchpad.net/~nifgraup) for the patch to CUPS-PDF v2.6

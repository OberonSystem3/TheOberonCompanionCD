Linux Oberon System 3 for X Windows

This particular distribution contains an implementation of Oberon System 3
release 2.2 for Linux/Intel and X Windows.

For technical correspondence, please contact:
    Swiss Federal Institute of Technology
    Institute for Computer Systems
    The Oberon System 3 Development Team
    ETH Zentrum
    CH-8092 Zurich
    Switzerland

    e-mail: oberon@inf.ethz.ch
    web: http://www.oberon.ethz.ch/system3/
    fax: +41 1 632 1307

SYSTEM REQUIREMENTS

    - An Intel-based Personal Computer running Linux/ELF and X Windows.
    - 10 MB of hard disk space.
    - Preferably a screen with resolution of 1024 by 768 pixels
      with 256 colors (although smaller size screens will work too).
    - A three button mouse.

INSTALLATION OF OBERON

    Oberon is delivered as a number of tar files compressed with gzip.

    LinuxOberonSystem3.tgz  - Linux Oberon for X Windows
    LinuxOberonXFonts.tgz   - X Windows fonts used by Oberon
    LinuxOberonDocu.tgz     - Documentation and books
    LinuxOberonApps.tgz     - Collection of Oberon applications, games and
                              tools
    LinuxOberonSrc.tgz      - System source code

    LinuxOberonSystem3.tgz and LinuxOberonXFonts.tgz are the minimum parts
    of the Oberon system.
    
INSTALLATION OF OBERON AS A PRIVATE COPY (PER-USER INSTALLATION)

1.  Create a new directory, e.g. ~/Oberon  (recommended)

2.  Extract the Oberon archives into this directory. The following directories
    are created automatically when unpacking the archives:

    System3
    System3/obj
    System3/system
    System3/docu
    System3/apps
    System3/src
    System3/resource
    System3/work

3.  Extract the Oberon X fonts archive into the directory created in step 1.
    This will create a new directory OberonXFonts which contains the
    necessary fonts.

INSTALLATION OF OBERON AS A PUBLIC COPY (SHARED INSTALLATION).

The following steps have to be executed with root permissions

1.  Create a new directory, e.g. /usr/local/Oberon (recommended)

2.  Extract the Oberon archives except LinuxOberonXFonts.tgz into this
    directory. The following directories are created automatically when
    unpacking the archives:

    System3
    System3/obj
    System3/system
    System3/docu
    System3/apps
    System3/src
    System3/resource
    System3/work

3.  Change the owner of the Oberon files with the following unix command:

      chown -R root.root System3

4.  If you exctracted the Oberon archives into another directory than
    /usr/local/Oberon you have to change the shell script
    System3/runsys3.shared. Open the script System3/runsys3.shared with
    a text editor and change the line

      setenv BASEDIR /usr/local/Oberon/System3

    to

      setenv BASEDIR oberon_dir/System3

    where oberon_dir is the directory created in step 1.

5.  Extract the Oberon X Windows fonts archive LinuxOberonXFonts.tgz into
    the directory /usr/X11R6/lib/X11/fonts. The fonts are placed in a
    newly created directory OberonXFonts.

6.  Change the owner of the Oberon font files with the unix command:

      chown -R root.root /usr/X11R6/lib/X11/fonts/OberonXFonts

7.  Add the Oberon fonts directory to the X server font path. This changes
    depend on which kind of X server you are using. For commercial X servers
    please consult their documentation on how to change the font path. For
    the freely available XFree86 servers follow these steps:

    Open the X server configuration file XF86Config with a text editor.
    This file may be located in one of the directories /etc, /etc/X11 or
    /usr/X11R6/lib/X11, depending on your Linux distribution.

    In the section "Files" add a new FontPath entry for the Oberon fonts.
    This entry has the form:

      FontPath  "/usr/X11R6/lib/X11/fonts/OberonXFonts/"

    Note: If the X server is already running, it has to be restarted
    to reread its configuration file, or you can execute the command

      xset fp+ /usr/X11R6/lib/X11/fonts/OberonXFonts/

    to dynamically adjust the font path of the running X server.

    For more information about XF86Config please read its man page
    (man XF86Config).

BEFORE RUNNING OBERON FOR THE FIRST TIME

In a shared installation of Linux Oberon System 3 every user has
to create his own working directories. Run the shell script
"MakePrivOberonDir" to create this directories. The following
directories are created:

    ~/Oberon/System3
    ~/Oberon/System3/obj
    ~/Oberon/System3/system
    ~/Oberon/System3/docu
    ~/Oberon/System3/apps
    ~/Oberon/System3/resource
    ~/Oberon/System3/work
    ~/Oberon/System3/src

Note: In a shared installation the shared files are normally write
protected. If you want to change one of these files you have to store them
as a local copy in your private directories. As your private directories
appear first in the OBERON search path your copy will be found first.

RE-INSTALLATION OF OBERON

If you re-install Oberon or upgrade your Oberon version, you run the
risk of overwriting the files of your current configuration, which you might
have adjusted to your personal preferences. You should make a backup
copy of the following "critical" files before installing Oberon.

    runsys3
    oberon.ini
    EditKeys.Text
    TextPopups.Text
    Access.Ring
    Public.Ring
    Private.Ring
    Configuration.Text
    MailIn.DB

STARTING OBERON

a) Using the predefined shell script "runsys3.shared" or "runsys3.private"

The easiest way to run Linux Oberon for X Windows is by executing
one of the runsys3 shell scripts. These scripts set all the required
environment variables and start Linux Oberon. If you have a per-user
installation of Linux Oberon use the shell script "runsys3.private",
if you have a shared installation of Linux Oberon take the shell
script "runsys3.shared" to start Linux Oberon.

b) Executing the program "oberon" manually

You can run Linux Oberon by manually executing the program oberon. Before
running Linux Oberon you have to set the environment variable OBERON which
contains an ordered list of directory names in which Linux Oberon looks
for its components.

QUITTING OBERON

The Oberon command System.Quit terminates the Oberon application. This is the
normal way to exit Oberon. Alternatively, the application can be terminated 
by choosing "Close" in the application window's system menu or by using the 
keyboard accelerator Alt-F4 (the window's system menu and keyboard
accelerator may depend on the window manager you are using).

KNOWN PROBLEMS

On some older Linux systems Oberon may not start but exit with an error
message like "... can't resolve symbol ..." because of problems with
the ELF dynamic linker. In this case replace the program "oberon" with
the program "oberon.static" which can be downloaded as a separate file
from the Linux Oberon FTP server.

COPYRIGHTS

Oberon and Linux Oberon System 3 are trademarks of Eidgenoessische
Technische Hochschule, Zurich
Intel is a registered trademark, i486 and Pentium are trademarks of Intel
Corporation.

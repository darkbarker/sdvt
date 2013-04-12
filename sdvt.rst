======
 sdvt
======

-------------------------------
Simple desktop virtual terminal
-------------------------------

:Author: Dmitry Medvedev <barkdarker@gmail.com>
:Manual section: 1


SYNOPSIS
========

``sdvt -e``


DESCRIPTION
===========

The ``sdvt`` is a minimal, fast, lightweight GTK-VTE-based terminal emulator for the desktop (of the X Window System).


USAGE
=====

This program follows the usual GNU command line syntax, with long
options starting with two dashes ('-'). A summary of options is
included below.

-e COMMAND, --command=COMMAND
              Execute the argument to this option inside the terminal, instead of the user shell.

-w PATH, --workdir=PATH
              Set working directory before running the command/shell (or any other command).

-f FONT, --font=FONT
              Font used by the terminal, in FontConfig syntax (the default is "monospace 10").

-s NUMBER, --scrollback=NUMBER
              Number of scrollback lines (the default is 1024).

-b, --bold    Allow usage of bold font variants.

-o, --one-screen
              One screen.

--bg-transparent
              Background transparent.

--bg-saturation=DOUBLE
              Background saturation.

--bg-image=FILENAME
              Background file image.

--scroll-keystroke
              Scroll on keystroke.

--scroll-output
              Scroll on output.

--audible-bell
              Audible bell.

--visible-bell
              Visible bell.

--browser=COMMAND
              Browser command.

-v, --version
              Print version information and exit.

-h, --help    Show a summary of available options.


EXAMPLES
========

Set wallpaper::

  sdvt --bg-image=/home/user/wallpaper.jpg --bg-saturation=0.5

ENVIRONMENT
===========

``SHELL`` - default command (otherwise getpwuid, "/bin/sh").

``BROWSER`` - default browser command (otherwise "xdg-open", "gnome-open", "exo-open", "firefox")


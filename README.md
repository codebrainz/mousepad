[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/mousepad/-/blob/master/COPYING)

# mousepad

Mousepad is a simple text editor for the [Xfce](https://www.xfce.org) desktop environment.

**Mousepad** aims to be an easy-to-use and fast editor. Our target is an
editor for quickly editing text files, not a development environment or an
editor with a huge bunch of plugins. On the other hand we try to use the latest
GTK features available, which means that if GTK adds something new in a major
release that is useful for the editor, we will likely bump the GTK dependency
and integrate this new feature in **Mousepad**.

----

### Homepage

[Mousepad documentation](https://docs.xfce.org/apps/mousepad/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/mousepad/-/blob/master/NEWS) for details on changes and fixes made in the current release.


### Required Packages 

Mousepad depends on the following packages:

* [GTK](https://www.gtk.org) >=3.20.0
* [GLib](https://wiki.gnome.org/Projects/GLib) >=2.42.0
* [xfconf](https://gitlab.xfce.org/xfce/xfconf) >=4.12.0
* [GtkSourceView](https://wiki.gnome.org/Projects/GtkSourceView) >=3.20

The following packages are optional:

* [D-Bus](https://www.freedesktop.org/wiki/Software/dbus) >=0.34 (strongly suggested)

### Source Code Repository

[Mousepad source code](https://gitlab.xfce.org/apps/mousepad)

### Download a Release Tarball

[Mousepad archive](https://archive.xfce.org/src/apps/mousepad)
    or
[Mousepad tags](https://gitlab.xfce.org/apps/mousepad/-/tags)

### Installation

From source code repository: 

    % cd mousepad
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf mousepad-<version>.tar.bz2
    % cd mousepad-<version>
    % ./configure
    % make
    % make install

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/mousepad/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.


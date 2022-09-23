<!-- vi: tw=80
  -->

mstatus
=======
`mstatus` is a minimal status bar intended for the DWM window manager but which
also works on other window managers; I use it on Sway, for instance.  The status
bar is comprised of a sequence of “blocks” which can be created, modifed, and
removed at will by writing commands to a named pipe.

For usage instructions see the `mstatus(1)` manual page.  This can either be
done by running `man -l mstatus.1`, or `man mstatus` if you have already
installed the program.  Online documentation is also available [here][1]


Installation
============
Assuming you have cloned the repo, you can install the status bar and manual
page in two commands.

```sh
$ make
$ make install  # Will require root permissions
```

If you are installing `mstatus` for usage in DWM, then the installation is
slightly different:

```sh
$ make HAS_DWM=true
$ make install  # Will require root permissions
```

[1]: https://thomasvoss.com/man/mstatus.1.html

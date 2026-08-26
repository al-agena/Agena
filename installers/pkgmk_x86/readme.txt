Building a Solaris 10 package:

It is recommended to first uninstall any installations of Agena before building
a new package by issuing the

> pkgrm SMCagena

command.

After that, change into /export/home/proglang/agena/installers/pkgmk and enter:

> pkgmk -o

This installs all the package files into the /var/spool/pkg/SMCagena folder.

Now convert the package into datastream format and thus create one package file
named `agena-0.14.0-sol10-x86-local`:

> pkgtrans -s /var/spool/pkg /export/home/proglang/agena/agena-0.14.0-sol10-x86-local SMCagena

You may compress this file with gzip or other utilities.

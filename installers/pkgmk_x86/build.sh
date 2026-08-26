#/bin/sh
AGENANAME="agena-7.9.2"
pkgrm SMCagena
pkgmk -o
pkgtrans -s /var/spool/pkg /export/home/proglang/agena/$AGENANAME-sol10-x86-local SMCagena
gzip -9 /export/home/proglang/agena/$AGENANAME-sol10-x86-local

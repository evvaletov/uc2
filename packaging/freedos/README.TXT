UC2 3.0.0a13+dos1 for DOS / FreeDOS

This DOS package adds the FreeDOS file-opening fix to alpha13.
Its version suffix identifies that patch; the included source contains it.
FDNPKG shows package version 2.99.13.1: pre-3.0, alpha13, DOS revision 1.
This numeric spelling lets its version comparator order package updates
and sort the eventual 3.0.0 stable release above these alpha packages.

This alpha release reads and writes UltraCompressor II archives. Keep your
original archives and extract into a fresh directory. This build was tested
with a Pentium CPU and 32 MB RAM. Large archives may need more memory.

Install from the directory containing UC2.ZIP:
  FDNPKG INSTALL UC2.ZIP
Remove the installed package:
  FDNPKG REMOVE UC2
FDNPKG's PROGS setting determines the installation directory. The supplied
LINKS/UC2.BAT entry lets the installer create a launcher in the DOS BIN path.
For manual installation, unzip and keep PROGS/UC2's files together, including
CWSDPMI.EXE. Run UC2.EXE from that directory or add it to PATH.

List, check, and extract an archive:
  UC2 -l OLD.UC2
  UC2 -t OLD.UC2
  MD RESTORED
  UC2 -d RESTORED OLD.UC2
Change to the input directory and use relative DOS 8.3 filenames:
  UC2 -qw -L4 NEW.UC2 HELLO.TXT

Levels 2-5 without --chunked create archives readable by original DOS UC2.
Levels 6-9 use new rANS coding. All chunked archives need revival version
3.0.0a11 or newer. DOS extraction refuses to replace existing output files;
choose a fresh destination. A failed extraction can leave earlier members
that were successfully extracted. This is not a whole-archive transaction.

CWSDPMI is loaded automatically if DOS has no DPMI host. Its binary is
unmodified; COPYING.CWS and CWSDPMI.DOC explain its licence and operation.
You have the right to receive its source and binary updates. This package
includes the source in SOURCE/UC2/CWSSRC.ZIP. Upstream:
  http://sandmann.dotster.com/cwsdpmi/

COPYING and LGPL.TXT contain the CLI licences. BSD.TXT covers the optional
integration source. See CREDITS.TXT for authors.
The complete corresponding UC2 source is SOURCE/UC2/UC2SRC.TGZ. Extract it
on a host with long filename support. SOURCE.TXT describes the build.
Report problems with the command, version and error message at:
  https://github.com/evvaletov/uc2/issues

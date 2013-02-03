DatabaseFS
==========
Eric Lorimer's DatabaseFS VFS Samba module.

Description
-----------
_From the Samba documentation (http://www.samba.org/samba/docs/man/Samba3-HOWTO/VFS.html)_.

  * URL: http://www.css.tayloru.edu/~elorimer/databasefs/index.php
  * By Eric Lorimer <elorimer@css.tayloru.edu>.

I have created a VFS module that implements a fairly complete read-only filesystem. It presents information from a database as a filesystem in a modular and generic way to allow different databases to be used. (Originally designed for organizing MP3s under directories such as “Artists,” “Song Keywords,” and so on. I have since easily applied it to a student roster database.) The directory structure is stored in the database itself and the module makes no assumptions about the database structure beyond the table it requires to run. 

Any feedback would be appreciated: comments, suggestions, patches, and so on. If nothing else, it might prove useful for someone else who wishes to create a virtual filesystem.

Information
-----------
I found the code of this module on an obscure Chinese website. I did not write any of it, but since it's GPL and pretty interesting, I figured I'd host it on GitHub.

  --ZeWaren

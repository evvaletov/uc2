int TType (char *filename, char *aext, int mustexist);
   // determines type of archive (aext (if not NULL) is filled with its extention)
   //   0 not existing file
   //   1 UC2 archive
   //   2 convertable non UC2 archive
   //   3 unknown file format
   //   4 UE2 encrypted archive

void Convert (char *filename, int type);
   // convert archive to UC2 archive
   //   0 convert X   to UC2 (convert command)
   //   1 convert UC2 to UC2 (optimize command)
   //   2 convert UC2 to UC2 'ignore' errors (archive repair)

char* NewExt (char *filename);

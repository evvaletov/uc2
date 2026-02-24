char* getvol(char drive);
// Returns the volume label of the specified drive, NULL if no volume
// label found. Returns a pointer to an internal static object.
// Drives are always 0 = default, 1 = A: etc...

int setvol(char drive, char* label);
// Sets the volume label of the specified drive. Returns -1 on error or
// 0 on success.

int rmvol(char drive);
// Removes the volume label on the specified drive. Returns -1 on error or
// 0 on success.


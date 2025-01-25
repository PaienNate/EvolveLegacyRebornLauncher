## How to use:
This document explains how to patch n3n-edge.c for the N3NManager. 

Comments indicate where in the code this needs to be placed!

Use this to clone the n3n repo and compile it (don't forget to patch it first):
```shell
wsl
git clone --depth 1 --branch 3.4.4 https://github.com/n42n/n3n.git
cd n3n
./autogen.sh
./configure --host "x86_64-w64-mingw32"
make
```

```diff
- static bool keep_on_running = true;
+ static BOOL* keep_on_running = NULL;
```

you'll also need to replace all assignments to keep_on_running like this:

```diff
- keep_on_running = true;
+ *keep_on_running = true;
```

```diff
- eee->keep_running = &keep_on_running;

+ SECURITY_DESCRIPTOR sd;
+ InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
+ SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
+ SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
+ SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE};
+
+ HANDLE hMapFile;
+ 
+ // Create a memory-mapped file
+ hMapFile = CreateFileMapping(
+         INVALID_HANDLE_VALUE,    // Use system paging file
+         &sa,                     // Default security
+         PAGE_READWRITE,          // Read/write access
+         0, MAPPED_FILE_SIZE,     // Size of the mapped file (1 byte)
+         MAPPED_FILE_NAME         // Name of the mapping
+ );
+ 
+ if (hMapFile == NULL) {
+     traceEvent(TRACE_ERROR, "CreateFileMapping failed!");
+     return 1;
+ }
+ 
+ // Map the memory to process space
+ keep_on_running = (BOOL*)MapViewOfFile(
+         hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, MAPPED_FILE_SIZE
+ );
+ 
+ if (keep_on_running == NULL) {
+     traceEvent(TRACE_ERROR, "MapViewOfFile failed!");
+     CloseHandle(hMapFile);
+     return 1;
+ }
+ 
+ *keep_on_running = true;
+ 
+ eee->keep_running = (_Bool*) keep_on_running;
```

```diff
/* Cleanup */
edge_term_conf(&eee->conf);
tuntap_close(&eee->device);
edge_term(eee);

+ UnmapViewOfFile(keep_on_running);
+ CloseHandle(hMapFile);
```
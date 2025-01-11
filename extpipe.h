/*
 * extpipe.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _EXTPIPE_H
#define _EXTPIPE_H

#include <vdr/tools.h>

class cExtPipe
{
private:
   cString sourceName;
   pid_t pid;
   int f_stdout;
   int f_stderr;
public:
   cExtPipe(cString SourceName);
   ~cExtPipe(void);
   bool Open(cString Command);
   bool Close(int *ReturnCode = NULL);
   bool GetResult(char **outBuffer, size_t &outBufferSize, char **errBuffer, size_t &errBufferSize);
};

#endif


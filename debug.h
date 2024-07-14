/*
 * debug.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __debug_h_
#define __debug_h_

//#define DBG_EPGHANDLER  1
//#define DBG_EPGHANDLER2 1
//#define DBG_EPISODES  1
//#define DBG_EPISODES2 1
//#define DBG_DROPEVENTS  1
//#define DBG_DROPEVENTS2 1
//#define DEBUG

#include "config.h"
#include "source.h"

class cEPGSource;
class XMLTVConfig;

extern void logger(cEPGSource *source, char logtype, const char* format, ...);
extern int SysLogLevel;

#ifdef esyslog
#undef esyslog
#endif

#ifdef isyslog
#undef isyslog
#endif

#ifdef dsyslog
#undef dsyslog
#endif

#define esyslog(a...) logger(NULL,'E', a)
#define isyslog(a...) logger(NULL,'I', a)
#define dsyslog(a...) logger(NULL,'D', a)
#define tsyslog(a...) void( (!isempty(XMLTVConfig.LogFilename())) ? logger(NULL, 'T', a) : void() )

#define esyslogs(s,a...) logger(s,'E', a)
#define isyslogs(s,a...) logger(s,'I', a)
#define dsyslogs(s,a...) logger(s,'D', a)
#define tsyslogs(s,a...) void( (!isempty(XMLTVConfig.LogFilename())) ? logger(s, 'T', a) : void() )

#endif

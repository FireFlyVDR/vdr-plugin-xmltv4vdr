/*
 * setup.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __setup_h
#define __setup_h

#include <vdr/plugin.h>

// ----------------- main setup menu
class cMenuSetupXmltv4vdr : public cMenuSetupPage
{
protected:
   virtual void Store(void);
private:
   cStringList channelStringList;
   int mappingBegin, mappingEnd;
   int sourcesBegin, sourcesEnd;
   int orderEntry;
   int epEntry;
   eOSState edit(void);
   void generateChannelList();
   void SetHelpKeys(void);
   int wakeup;
   int tmpFixDuplTitleInShortttext;
   int imgdelafter;
public:
   cMenuSetupXmltv4vdr();
   ~cMenuSetupXmltv4vdr();
   void Set(void);
   virtual eOSState ProcessKey(eKeys Key);
   const cStringList *ChannelList() const { return &channelStringList; }
};

#endif

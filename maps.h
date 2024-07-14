/*
 * maps.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _MAPS_H
#define _MAPS_H

#include <vdr/channels.h>

#define USE_NOTHING            0x000000
#define USE_NEVER              0x000000ULL
#define USE_MOVIES_ONLY        0x000001ULL
#define USE_SERIES_ONLY        0x000002ULL
#define USE_MOVIES_SERIES      (USE_MOVIES_ONLY|USE_SERIES_ONLY)
#define USE_ALWAYS             0x000004ULL
#define USE_MOVIES_MASK        0x000005ULL
#define USE_SERIES_MASK        0x000006ULL
#define USE_CATEGORY_MASK      0x000007ULL

#define SHIFT_TITLE            6
#define USE_TITLE              (7ULL << SHIFT_TITLE)

#define SHIFT_SHORTTEXT        9
#define USE_SHORTTEXT          (7ULL << SHIFT_SHORTTEXT)

#define SHIFT_DESCRIPTION      12
#define USE_DESCRIPTION        (7ULL << SHIFT_DESCRIPTION)

#define SHIFT_COUNTRYYEAR      15
#define USE_COUNTRYYEAR        (7ULL << SHIFT_COUNTRYYEAR)

#define SHIFT_ORIGTITLE        18
#define USE_ORIGTITLE          (7ULL << SHIFT_ORIGTITLE)

#define SHIFT_CATEGORIES       21
#define USE_CATEGORIES         (7ULL << SHIFT_CATEGORIES)

#define SHIFT_REVIEW           24
#define USE_REVIEW             (7ULL << SHIFT_REVIEW)

#define SHIFT_STAR_RATING      27
#define USE_STAR_RATING        (7ULL << SHIFT_STAR_RATING)

#define SHIFT_SEASON_EPISODE   30
#define USE_SEASON_EPISODE     (7ULL << SHIFT_SEASON_EPISODE)
#define SHIFT_SEASON_EPISODE_MULTILINE 33
#define USE_SEASON_EPISODE_MULTILINE (1ULL << SHIFT_SEASON_EPISODE_MULTILINE)

#define SHIFT_CREDITS            34
#define USE_CREDITS              (7ULL << SHIFT_CREDITS)
#define SHIFT_CREDITS_ACTORS     37
#define CREDITS_ACTORS           (3ULL << SHIFT_CREDITS_ACTORS)
#define CREDITS_ACTORS_MULTILINE (2ULL << SHIFT_CREDITS_ACTORS) // 0 = none, 1 = single line, 2 = multi line
#define SHIFT_CREDITS_DIRECTORS  38
#define CREDITS_DIRECTORS        (1ULL << SHIFT_CREDITS_DIRECTORS)
#define SHIFT_CREDITS_OTHERS     40
#define CREDITS_OTHERS           (1ULL << SHIFT_CREDITS_OTHERS)

#define SHIFT_PARENTAL_RATING    41
#define USE_PARENTAL_RATING      (1ULL << SHIFT_PARENTAL_RATING)
#define SHIFT_PARENTAL_RATING_TEXT 42
#define PARENTAL_RATING_TEXT     (1ULL << SHIFT_PARENTAL_RATING_TEXT)

#define USE_MASK               (USE_TITLE | USE_SHORTTEXT | USE_DESCRIPTION | USE_COUNTRYYEAR | \
                                USE_ORIGTITLE | USE_CATEGORIES | USE_REVIEW | USE_STAR_RATING | \
                                USE_SEASON_EPISODE | USE_CREDITS | USE_PARENTAL_RATING)

// --------------------------------------------------------------------------------------------------------
class cChannelIdObject : public tChannelID
{
private:
   tChannelID channelid;
public:
   cChannelIdObject(tChannelID ChannelID) { channelid = ChannelID; }
   cChannelIdObject(const char *ChannelIDString) { channelid = tChannelID::FromString(ChannelIDString); }
   tChannelID GetChannelID() { return channelid; };
   cString GetChannelIDString() { return channelid.ToString(); };
};

class cChannelList : public cVector<cChannelIdObject *>
{
public:
   cChannelList(int Allocated = 4): cVector<cChannelIdObject *>(Allocated) {}
   virtual ~cChannelList();
   int IndexOf(tChannelID channelID) const;
   virtual void Clear(void);
};

// --------------------------------------------------------------------------------------------------------
class cEPGChannel : public cListObject
{
private:
   cString name;
   cChannelList channelList;
   bool inUse;
public:
   cEPGChannel(const char *Name, bool InUse = false);
   ~cEPGChannel();
   const char *Name() { return name; }
   bool InUse() { return inUse; }
   void SetUsage(bool InUse) { inUse = InUse; }
   virtual int Compare(const cListObject &ListObject) const;
};

class cEPGSource;
// --------------------------------------------------------------------------------------------------------
class cEPGMapping : public cListObject
{
private:
   cString epgChannelName;
   cChannelList channelList;  // TEST assigned VDR channels
   cEPGSource *epgSource;
   uint64_t flags;
public:
   cEPGMapping(const char *EpgChannelName, const char *Flags_and_Mappings);
   cEPGMapping(void);
   cEPGMapping(cEPGMapping *Mapping);
   ~cEPGMapping();
   cEPGMapping &operator= (const cEPGMapping &EpgMapping);
   cString ToString(void);
   void SetFlags(uint64_t Flags) { flags = Flags; }
   void SetChannelList(cChannelList *ChannelList);
   void AddChannel(tChannelID ChannelID);
   void RemoveChannel(tChannelID ChannelID, bool MarkOnlyInvalid = false);
   cEPGSource *EPGSource()  { return epgSource; }
   void SetEpgSource(cEPGSource *EPGSource) { epgSource = EPGSource; }
   uint64_t Flags()               { return flags; }
   const char *EPGChannelName() { return *epgChannelName; }
   cChannelList *ChannelMapList()  { return &channelList; }
};

// --------------------------------------------------------------------------------------------------------
class cEPGMappings : public cList<cEPGMapping>
{
public:
   cEPGSource *GetSource(const char *EPGchannel);
   cString GetActiveEPGChannels(const char *SourceName);
   cEPGMapping *GetMap(const char *EpgChannelName);
   cEPGMapping *GetMap(tChannelID ChannelID);
   void SetAllFlags(uint64_t flags);
   bool ProcessChannel(tChannelID ChannelID);
   void Remove();
};

#endif

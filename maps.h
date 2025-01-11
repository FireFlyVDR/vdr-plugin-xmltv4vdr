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

#define SHIFT_APPEND_EXT_EVENTS  43
#define USE_APPEND_EXT_EVENTS    (3ULL << SHIFT_APPEND_EXT_EVENTS)
#define NO_EXT_EVENTS            0ULL
#define APPEND_EXT_EVENTS        1ULL
#define ONLY_EXT_EVENTS          2ULL


#define USE_MASK               (USE_TITLE | USE_SHORTTEXT | USE_DESCRIPTION | USE_COUNTRYYEAR | \
                                USE_ORIGTITLE | USE_CATEGORIES | USE_REVIEW | USE_STAR_RATING | \
                                USE_SEASON_EPISODE | USE_CREDITS | USE_PARENTAL_RATING | USE_APPEND_EXT_EVENTS)

// --------------------------------------------------------------------------------------------------------
class cChannelIDObject : public tChannelID
{
private:
   tChannelID channelID;
public:
   cChannelIDObject(tChannelID ChannelID) { channelID = ChannelID; }
   cChannelIDObject(const char *ChannelIDString) { channelID = tChannelID::FromString(ChannelIDString); }
   tChannelID GetChannelID() { return channelID; };
   cString GetChannelIDString() { return channelID.ToString(); };
};

class cChannelIDList : public cVector<cChannelIDObject *>
{
public:
   cChannelIDList(int Allocated = 4): cVector<cChannelIDObject *>(Allocated) {}
   virtual ~cChannelIDList();
   int IndexOf(tChannelID channelID) const;
   virtual void Clear(void);
};

class cEPGSource;
// --------------------------------------------------------------------------------------------------------
class cEPGChannel : public cListObject
{
private:
   cString epgChannelName;
   cChannelIDList channelIDList;  // TEST assigned VDR channels
   cEPGSource *epgSource;
   uint64_t flags;
public:
   cEPGChannel(const char *EpgChannelName, const char *Flags_and_Channels);
   cEPGChannel(void);
   cEPGChannel(cEPGChannel *NewEpgChannel);
   ~cEPGChannel();
   cEPGChannel &operator= (const cEPGChannel &EpgChannel);
   cString ToString(void);
   const char *EPGChannelName()             { return *epgChannelName; }

   void AddChannel(tChannelID ChannelID);
   void RemoveChannel(tChannelID ChannelID, bool MarkOnlyInvalid = false);

   void SetFlags(uint64_t Flags)            { flags = Flags; }
   uint64_t Flags()                         { return flags; }

   void SetChannelIDList(cChannelIDList *ChannelIDList);
   const cChannelIDList &ChannelIDList() const { return channelIDList; }

   void SetEpgSource(cEPGSource *EPGSource) { epgSource = EPGSource; }
   cEPGSource *EPGSource()                  { return epgSource; }
};

// --------------------------------------------------------------------------------------------------------
class cEPGChannels : public cList<cEPGChannel>
{
public:
   cEPGSource *GetEpgSource(const char *EPGchannel);
   bool HasActiveEPGChannels(const char *SourceName);
   cString GetActiveEPGChannels(const char *SourceName);
   bool IsActiveEPGChannel(const char *EpgChannelName, const char *SourceName);
   cEPGChannel *GetEpgChannel(const char *EpgChannelName);
   cEPGChannel *GetEpgChannel(tChannelID ChannelID);
   void SetAllFlags(uint64_t flags);
   bool ProcessChannel(tChannelID ChannelID);
   void RemoveAll();
};

#endif

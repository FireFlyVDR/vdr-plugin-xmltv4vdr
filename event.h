/*
 * event.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _EVENT_H
#define _EVENT_H

#include <vdr/epg.h>

class cXMLTVStringList : public cStringList
{
private:
   cString toString;
public:
   const char *ToString(const char *delimiter = "@");
   int SetStringList(const char *StringList, const char *delimiter = "@");
   bool AppendUnique(const char* String);
   int FindStr(const char *s) const;
};

#define xmlTagStart   "<xmltv4vdr>"
#define xmlTagEnd     "</xmltv4vdr>"
#define xmlTagTVStart "<xmltv4vdrTV>"
#define xmlTagTVEnd   "</xmltv4vdrTV>"
#define EXTERNAL_EVENT_OFFSET  0x10000

class cXMLTVEvent
{
private:
   cString sourceName;
   tChannelID channelID;
   uint64_t xtEventID;
   tEventID eventid;
   uchar tableID;
   uchar version;
   time_t starttime;
   int duration;
   cString title;
   cString shortText;
   cString description;
   cString country;
   cString origTitle;
   cXMLTVStringList credits;
   cXMLTVStringList category;
   cXMLTVStringList review;
   cXMLTVStringList parentalRating;
   cXMLTVStringList starRating;
   cXMLTVStringList pics;
   int year;
   int season;
   int episode;
   int episodeOverall;
   bool evaluateFlags(uint64_t Flags, bool isMovie, bool isSeries);
public:
   cXMLTVEvent();
   ~cXMLTVEvent();
   void Clear();

   void SetSourceName(const char *SourceName);
   const char *SourceName(void) const { return *sourceName; }

   void SetChannelID(const char *ChannelID);
   const tChannelID ChannelID(void) const  { return channelID; }

   void SetTitle(const char *Title = NULL);
   const char *Title(void) const { return *title; }
   bool HasTitle(void)           { return !isempty(title); }

   void SetOrigTitle(const char *OrigTitle = NULL);
   const char *OrigTitle(void) const { return *origTitle; }

   void SetShortText(const char *ShortText = NULL);
   const char *ShortText(void) const { return *shortText; }

   void SetStartTime(time_t StartTime) { starttime = StartTime; }
   time_t StartTime() const       { return starttime; }

   void SetDuration(int Duration) { duration = Duration; }
   int Duration() const           { return duration; }

   void SetEventID(tEventID EventID) { eventid = EventID; }
   tEventID EventID(void) const   { return eventid; }

   void SetTableID(uchar TableID) { tableID = TableID; }
   uchar TableID() const          { return tableID; }

   void SetVersion(uchar Version) { version = Version; }
   uchar Version() const          { return version; }

   void SetXTEventID(uint64_t XtEventID) { xtEventID = XtEventID; }
   uint32_t GenerateEventID(time_t StartTime, uint32_t Offset = 0);
   void SetXTEventIDFromTime(time_t StartTime);
   uint64_t XTEventID(void) const  { return xtEventID; }

   void SetDescription(const char *description = NULL);
   const char *Description(void) const { return *description; }

   void SetCountry(const char *Country = NULL);
   const char *Country(void) const { return *country; }

   void SetCredits(const char *Credits = NULL);
   void AddCredits(const char *CreditType, const char *Credit, const char *Addendum = NULL);
   cXMLTVStringList *Credits()   { return &credits; }

   void SetCategories(const char *Categories = NULL);
   void AddCategory(const char *Category);
   cXMLTVStringList *Category()  { return &category; }

   void SetReviews(const char *Reviews = NULL);
   void AddReview(const char *Review);
   cXMLTVStringList *Review()    { return &review; }

   void SetParentalRating(const char *ParentalRating = NULL);
   void AddParentalRating(const char *System, const char *ParentalRating);
   cXMLTVStringList *ParentalRating()    { return &parentalRating; }

   void SetStarRating(const char *StarRating = NULL);
   void AddStarRating(const char *System, const char *StarRating);
   cXMLTVStringList *StarRating(){ return &starRating; }

   void SetPics(const char *Pics = NULL);
   void AddPic(const char *Pic);
   cXMLTVStringList *Pics()      { return &pics; }

   void SetSeason(int Season = 0){ season = Season; }
   int Season(void)              { return season; }

   void SetEpisode(int Episode = 0) { episode = Episode; }
   int Episode(void)             { return episode; }

   void SetEpisodeOverall(int EpisodeOverall = 0) { episodeOverall = EpisodeOverall; }
   int EpisodeOverall(void)      { return episodeOverall; }

   void SetYear(int Year = 0)    { year = Year; }
   int Year() const              { return year; }

   void LinkPictures(bool LinkToEventID = false);
   void FillEventFromXTEvent(cEvent *Event, uint64_t Flags);
   int CompareEvent(cEvent *Event, int matchOffset = 0);
   bool FetchSeasonEpisode();
};

int strsplit(const char *Source, const char delimiter, cString &Before, cString &Middle, cString &After);
cString StringNormalize(const char *String);
cString StringCleanup(const char *String, bool ExtendedRemove = false, bool ExtendedRemoveWithSpace = false);
uint LevenshteinDistance(uint *s1, uint l1, uint *s2, uint l2);

#endif

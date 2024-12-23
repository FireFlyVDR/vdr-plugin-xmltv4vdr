/*
 * config.h: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "database.h"
#include "handler.h"

#define EPGSOURCESDIR "/var/lib/epgsources"
#define DELTA_HOUSEKEEPINGTIME 600

enum
{  // program elements
   DESC_DESC = 0, // Description (Longtext)
   DESC_CRED,     // Credits
   DESC_COYR,     // country year
   DESC_ORGT,     // Original Title
   DESC_CATG,     // Catgeory
   DESC_SEAS,     // Season
   DESC_PRAT,     // Parental Rating
   DESC_SRAT,     // Star Rating
   DESC_REVW,     // Review
   DESC_COUNT
};

struct descriptionSeq {
   uchar seq[DESC_COUNT]; 
};

class cConfigLine : public cListObject
{
private:
   char *section;
   char *name;
   char *source;
   char *value;
public:
   cConfigLine(void);
   cConfigLine(const char *Section, const char *Name, const char *Value);
   cConfigLine(const char *Section, const char *Name, const char *Source, const char *Value);
   virtual ~cConfigLine();
   virtual int Compare(const cListObject &ListObject) const;
   const char *Section(void) { return section; }
   const char *Name(void) { return name; }
   const char *Extension(void) {return source; }
   const char *Value(void) { return value; }
   bool Parse(char *s);
   bool Save(FILE *f);
};


// -------------------------------------------------------------
class cXMLTVConfig : public cConfig<cConfigLine>
{
private:
   cString epgDBFile;
   cString imageDir;
   cString epgSourcesDir;
   cString episodesDir;
   cString episodesDBFile;
   cString episodesServer;
   int episodesServerPort;
   time_t lastEpisodesUpdate;
   cString logFilename;

   bool DB_initialized;
   struct descriptionSeq descrSequence;
   cEPGSources *epgSources;
   cEPGChannels *epgChannels;
   cHouseKeeping *houseKeeping;
   bool wakeup;
   bool fixDuplTitleInShortttext;
   FILE *fhLogfile;

public:
   cXMLTVConfig(void);
   ~cXMLTVConfig(void);

   bool Parse(const char *Name, const char *Extension, const char *Value, const char *Section);
   bool Load(const char *FileName);
   cConfigLine *Get(const char *Name, const char *Section);
   cConfigLine *Get(const char *Name, const char *Extension, const char *Section);
   void Store(const char *Name, const char *Extension, const char *Value, const char *Section);
   void Store(const char *Name, const char *Value, const char *Section);
   void StoreEpgChannel(cEPGChannel *EpgChannel);
   void StoreSourceParameter(cEPGSource *Source);
   bool Save(void);

   void SetEPGDBFile(const char *EPGDBFile){ epgDBFile = EPGDBFile; }
   const char *EPGDBFile()                 { return *epgDBFile; }
   void SetDBinitialized(bool initialized) { DB_initialized = initialized; }
   bool DBinitialized()                    { return DB_initialized; }

   void SetImageDir(const char *ImageDir)  { imageDir = ImageDir; }
   const char *ImageDir()                  { return *imageDir; }

   void SetEPGSourcesDir(const char *EPGSourcesDir) { epgSourcesDir = EPGSourcesDir; }
   const char *EPGSourcesDir()             { return *epgSourcesDir; }

   void SetEpisodesDir(const char *EpisodesDir) { episodesDir = EpisodesDir; }
   const char *EpisodesDir()               { return *episodesDir; }

   void SetEpisodesDBFile(const char *EpisodesDBFile) { episodesDBFile = EpisodesDBFile; }
   const char *EpisodesDBFile()            { return *episodesDBFile; }
   bool UseEpisodes()                      { return !isempty(*episodesDBFile); }

   void SetEpisodesServer(const char *EpisodesServer) { episodesServer = EpisodesServer; }
   const char *EpisodesServer()            { return *episodesServer; }

   void SetEpisodesServerPort(int EpisodesServerPort) { episodesServerPort = EpisodesServerPort; }
   int EpisodesServerPort()                { return episodesServerPort; }

   void SetLastEpisodesUpdate(time_t LastUpdate) { lastEpisodesUpdate = LastUpdate; }
   time_t LastEpisodesUpdate()                { return lastEpisodesUpdate; }

   void SetLogFilename(const char *LogFilename) { logFilename = LogFilename; }
   const char *LogFilename(void)           { return *logFilename; }
   void SetLogfile(FILE *LogfileHandle)    { fhLogfile = LogfileHandle; }
   FILE *LogFile()                         { return fhLogfile; }

   void SetHouseKeeping(cHouseKeeping *HouseKeeping) { houseKeeping = HouseKeeping; }
   cHouseKeeping *HouseKeeping()           { return houseKeeping; }
   bool HouseKeepingActive(void)           { return houseKeeping != NULL && houseKeeping->Active(); }
   bool ImportActive(void)                 { return epgSources != NULL && epgSources->ImportIsRunning(); }

   cEPGChannels *EPGChannels()             { return epgChannels; }
   cEPGSources  *EPGSources()              { return epgSources; }

   void SetDescrSequence(const struct descriptionSeq NewSequence) { descrSequence = NewSequence; }
   void SetDescrSequence(const char *NewSequence);
   struct descriptionSeq GetDescrSequence() { return  descrSequence; }
   struct descriptionSeq GetDefaultDescrSequence() { return  {0, 1, 2, 3, 4, 5, 6, 7, 8}; }
   const char *GetDescrSequenceString();

   void SetWakeUp(bool Value)            { wakeup = Value; }
   bool WakeUp()                         { return wakeup; }

   void SetFixDuplTitleInShortttext(bool Value) { fixDuplTitleInShortttext = Value; }
   bool FixDuplTitleInShortttext()              { return fixDuplTitleInShortttext; }
};

extern cXMLTVConfig XMLTVConfig;

#endif

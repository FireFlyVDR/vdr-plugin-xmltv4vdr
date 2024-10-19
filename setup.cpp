/*
 * setup.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <vdr/menu.h>
#include "setup.h"
#include "debug.h"

#define CHNUMWIDTH (numdigits(cChannels::MaxNumber())+1)
#define NEWTITLE(x) new cOsdItem(cString::sprintf("%s%s%s", "---- ", x, " ----"), osUnknown, false)


// ----------------- menu setup field sequence
class cMenuSetupDescriptionFieldSequence : public cMenuSetupPage
{
protected:
   virtual void Store(void);
   struct descriptionSeq sequence;
private:
   void Set(void);
   void SetHelpKeys(void);
   void sequenceSwap(int a, int b);
public:
   cMenuSetupDescriptionFieldSequence(void);
   ~cMenuSetupDescriptionFieldSequence() { }
   virtual eOSState ProcessKey(eKeys Key);
};

// ----------------- menu select channels in source which should be enabled
class cMenuSetupSource : public cMenuSetupPage
{
protected:
   virtual void Store(void);
private:
   cEPGSource *epgsrc;
   int *sel;
   time_t day;
   int execDays, execTime;
   int daysInAdvance;
   int enabled;
   int usePics;
   char pin[255];
   void Set(void);
public:
   cMenuSetupSource(cEPGSource *epgSrc);

   ~cMenuSetupSource();
   virtual eOSState ProcessKey(eKeys Key);
   void SetHelpKeys(void);
};

// ----------------- menu channel mapping between EPG source and VDR
class cMenuSetupMapping : public cMenuSetupPage
{
protected:
   virtual void Store(void);
private:
   cEPGMapping *tmpEPGMapping;
   bool hasMappedChannels;
   uint64_t flags;
#define MAX_MOVIESSERIES 5
   const char *optionsEPGAux[4];
   const char *optionsMovieSeries[MAX_MOVIESSERIES];
   const char *optionsLines[3];
   const char *optionsYesNo[2];
   cString title;
   int lineMap, lineOpts;
   int c1, c2, c3, c4, c5;
   cOsdItem *option(const char *s, bool yesno);
   void Set(void);
   int sourceIndex;
   cStringList sourcesList;
   void Store(cEPGMapping *newmapping, bool replacemapping = true);
public:
   cMenuSetupMapping(const char *EpgChannelName);
   ~cMenuSetupMapping();
   virtual eOSState ProcessKey(eKeys Key);
   void SetHelpKeys(void);
};


// ----------------- menu select channel from VDR to add to ChannelMap
class cMenuSetupMapping_AddChannel : public cOsdMenu
{
private:
   cEPGMapping *tmpEPGMapping;
   bool epgMappingExists(tChannelID channelid);
   void Set(void);
public:
   cMenuSetupMapping_AddChannel(cEPGMapping *tmpEPGMapping);
   virtual eOSState ProcessKey(eKeys Key);
};

// ----------------- menu LOG menu (yellow key in main setup menu)
class cMenuSetupLog : public cMenuText
{
private:
   enum
   {
      VIEW_ERROR = 0,
      VIEW_INFO,
      VIEW_DEBUG
   };
   int level;
   cEPGSource *source;
   char nextrun_str[30];
   int width;
   time_t lastRefresh;
   const cFont *font;
   void SetHelpKeys(void);
   void Set(void);
public:
   cMenuSetupLog(cEPGSource *Source);
   virtual eOSState ProcessKey(eKeys Key);
};


// ----------------- 
class cMenuEditMultiBitItem : public cMenuEditIntItem
{
protected:
   uint64_t *value;
   uint64_t mask;
   uint shift;
   const char * const *strings;
   uint64_t index;
protected:
   virtual void Set() {
      SetValue(strings[index]);
      *value = (*value & ~mask) | (index << shift);
   }
public:
   cMenuEditMultiBitItem(const char *Name, uint64_t *Value, uint64_t Mask, uint Shift, int NumStrings, const char * const *Strings)
   :cMenuEditIntItem(Name, (int *)&index, 0, NumStrings - 1)
   {
      strings = Strings;
      value = Value;
      mask = Mask;
      shift = Shift;
      index = (*Value & mask) >> shift;
      Set();
   }
};

// --------------------------------------------------------------------------------------------------------
// Main Menu
cMenuSetupXmltv4vdr::cMenuSetupXmltv4vdr()
{
   sourcesBegin = sourcesEnd = mappingBegin = mappingEnd = 0;
   wakeup = XMLTVConfig.WakeUp();
   tmpFixDuplTitleInShortttext = XMLTVConfig.FixDuplTitleInShortttext();
   SetCurrent(Get(0));
   Set();
}

cMenuSetupXmltv4vdr::~cMenuSetupXmltv4vdr()
{
}

void cMenuSetupXmltv4vdr::Set(void)
{
   int current = Current();
   Clear();

   if (XMLTVConfig.ImportActive()) {
      Add(new cOsdItem(tr("import is currently active"), osUnknown, false));
      Add(new cOsdItem(tr("no modififcations possible"), osUnknown, false));
   }
   else {
      Add(new cOsdItem(tr("order of EPG elements"), osUnknown), true);
      orderEntry = Current();
      Add(new cMenuEditBoolItem(tr("automatic wakeup"), &wakeup), true);
      Add(new cMenuEditBoolItem(tr("fix duplicate Title in Shorttext"), &tmpFixDuplTitleInShortttext), true);

      // ------------ episodes update
      if (XMLTVConfig.UseEpisodes()) {
         Add(new cOsdItem(*cString::sprintf("%s:\t%s", tr("last Episodes Update"), *DayDateTime(XMLTVConfig.LastEpisodesUpdate())), osUnknown, false), true);
      }

      // ------------ EPG sources
      Add(NEWTITLE(tr("EPG sources (last successful run)")), true);

      if (!XMLTVConfig.EPGSources()->Count())
      {
         Add(new cOsdItem(cString::sprintf("%s %s", tr("no EPG sources defined in"), XMLTVConfig.EPGSourcesDir()), osUnknown, false));
         sourcesBegin = sourcesEnd = 0;
      }
      else 
      {
         sourcesBegin = Current() + 1;
         for (int i = 0; i < XMLTVConfig.EPGSources()->Count(); i++)
         {
            cEPGSource *epgsrc = XMLTVConfig.EPGSources()->Get(i);
            if (epgsrc)
            {
               cString buffer = cString::sprintf("%s  (%s):\t%s", epgsrc->SourceName(), epgsrc->LastSuccessfulRun() ? *DayDateTime(epgsrc->LastSuccessfulRun()): tr("unknown"), epgsrc->Enabled() ? tr("enabled") : tr("disabled"));
               Add(new cOsdItem(*buffer, osUnknown), true);
            }
         }
         sourcesEnd = Current();
      }
      // ------------ channel mapping Main Menu
      Add(NEWTITLE(tr("EPG source channels")), true);
      generateChannelList();

      mappingBegin = Current() + 1;
      for (int i = 0; i < channelStringList.Size(); i++)
      {
         cEPGSource *src = XMLTVConfig.EPGMappings()->GetSource(channelStringList[i]);
         Add(new cOsdItem(cString::sprintf("%s\t%s", channelStringList[i], src ? src->SourceName() : "---"), osUnknown), true);
      }
      mappingEnd = Current();
#ifdef DEBUG
      for (cEPGMapping *epgMap = XMLTVConfig.EPGMappings()->First(); epgMap; epgMap = XMLTVConfig.EPGMappings()->Next(epgMap))  // gehe alle EPGsources durch (warum -1 ? EIT?)
      {
         tsyslog("mapping: %2d %s # %s", epgMap->ChannelMapList()->Size(), epgMap->EPGSource() ? epgMap->EPGSource()->SourceName() :" --- ", epgMap->EPGChannelName());
      }
#endif
   }
   SetCurrent(Get(max(0, current)));
   SetHelpKeys();
   Display();
}

void cMenuSetupXmltv4vdr::generateChannelList()
{  // generate list of all channels of all sources
   channelStringList.Clear();

   // all EPGchannels of all active sources
   for (cEPGSource *epgSrc = XMLTVConfig.EPGSources()->First(); epgSrc; epgSrc = XMLTVConfig.EPGSources()->Next(epgSrc)) {
      if (epgSrc && epgSrc->Enabled()) {
         cEPGChannels *srcChannellist = epgSrc->ChannelList();
         if (srcChannellist) {
            for (cEPGChannel *epgCha = srcChannellist->First(); epgCha; epgCha = srcChannellist->Next(epgCha)) {
               if (channelStringList.Find(epgCha->Name()) == -1 )
                  channelStringList.Append(strdup(epgCha->Name()));
            }
         }
      }
   }
   channelStringList.Sort(true);
}


void cMenuSetupXmltv4vdr::SetHelpKeys(void)
{
   if (XMLTVConfig.ImportActive())
      SetHelp(NULL);
   else
   {
      if ((Current() >= sourcesBegin) && (Current() <= sourcesEnd))
         SetHelp(tr("Button$Start Import"),  NULL,  tr("Button$Log"), tr("Button$Edit"));
      else if (((Current() >= mappingBegin) && (Current() <= mappingEnd)) || (Current() == orderEntry))
         SetHelp(tr("Button$Start Import"),  NULL,  NULL,  tr("Button$Edit"));
      else
         SetHelp(tr("Button$Start Import"),  NULL,  NULL,  NULL);
   }
}

eOSState cMenuSetupXmltv4vdr::ProcessKey(eKeys Key)
{
   bool HadSubMenu = HasSubMenu();

   eOSState state = cOsdMenu::ProcessKey(Key);

   if (!HasSubMenu()) {
      if (state == osUnknown) {
         if (XMLTVConfig.ImportActive()) {
            state = osBack;
         }
         else {
         switch (Key)
         {
            case kRed:
                  XMLTVConfig.EPGSources()->StartImport(true);
                  Set();
                  state = osContinue;
                  break;
            case kOk:
            case kBlue:   // OK or Edit key
                  if (Current() == orderEntry)
                     return AddSubMenu(new cMenuSetupDescriptionFieldSequence());

                  if ((Current() >= sourcesBegin) && (Current() <= sourcesEnd))
                     return AddSubMenu(new cMenuSetupSource(XMLTVConfig.EPGSources()->Get(Current() - sourcesBegin)));

                  if ((Current() >= mappingBegin) && (Current() <= mappingEnd)) {
                     cString epgchannel = channelStringList.At(Current() - mappingBegin);
#ifdef DEBUG
                     if (isempty(*epgchannel))
                        tsyslog("AddSubMenu %s ", "empty");
                     else
                        tsyslog("AddSubMenu #%s# ", *epgchannel);
#endif
                     return AddSubMenu(new cMenuSetupMapping(channelStringList.At(Current() - mappingBegin)));
                  }
                  Store();
                  state = osContinue;
                  break;
            case kYellow:  // show Log
                  if ((Current() >= sourcesBegin) && (Current() <= sourcesEnd))
                  {
                     cEPGSource *src = XMLTVConfig.EPGSources()->Get(Current() - sourcesBegin);
                     if (src)
                        return AddSubMenu(new cMenuSetupLog(src));
                  }
                  state = osContinue;
                  break;
            default:
               break;
         }
         }
      }
   }
   if (HadSubMenu && !HasSubMenu())
   {  // return from submenu (was closed)
      Set();
      Display();
   }

   if (!HasSubMenu() && Key != kNone)
      SetHelpKeys();

   return state;
}

void cMenuSetupXmltv4vdr::Store(void)
{  // store all general settings in setup.conf
   cString srcorder("");
   if (XMLTVConfig.EPGSources()->Count())
   {
      for (int i = 0; i < XMLTVConfig.EPGSources()->Count(); i++)
      {
         cEPGSource *epgsrc = XMLTVConfig.EPGSources()->Get(i);
         if (epgsrc && epgsrc->SourceName())
         {
            if (!epgsrc->Enabled())
               srcorder.Append("-");
            srcorder.Append(epgsrc->SourceName());
            if (i < XMLTVConfig.EPGSources()->Count() - 1) 
               srcorder.Append(",");
         }
      }
      SetupStore("source.order", *srcorder);
   }

   SetupStore("options.wakeup", wakeup);
   SetupStore("options.fixDuplTitleInShortttext", tmpFixDuplTitleInShortttext);
   XMLTVConfig.SetWakeUp((bool) wakeup);
   XMLTVConfig.SetFixDuplTitleInShortttext((bool) tmpFixDuplTitleInShortttext);
}

// --------------------------------------------------------------------------------------------------------
// squence of fields in Description (main setup menu red button)
cMenuSetupDescriptionFieldSequence::cMenuSetupDescriptionFieldSequence(void)
{
   cPlugin *plugin = cPluginManager::GetPlugin(PLUGIN_NAME_I18N);
   if (!plugin) return;
   SetPlugin(plugin);
   SetSection(cString::sprintf("%s '%s' : %s", trVDR("Plugin"), plugin->Name(), tr("sequence")));

   sequence = XMLTVConfig.GetDescrSequence();
   Set();
}

void cMenuSetupDescriptionFieldSequence::SetHelpKeys(void)
{
   SetHelp(trVDR("Button$Reset"), Current() > 0 ? tr("Button$Up") : NULL, Current() < Count() -1 ? tr("Button$Down") : NULL);
}

void cMenuSetupDescriptionFieldSequence::Set(void)
{
   int current = Current();
   Clear();

   for (int i = 0; i < DESC_COUNT; i++) {
      switch (sequence.seq[i])
      {
         case DESC_DESC: Add(new cOsdItem(tr("description"))); break;
         case DESC_CRED: Add(new cOsdItem(tr("credits"))); break;
         case DESC_COYR: Add(new cOsdItem(tr("country and year"))); break;
         case DESC_ORGT: Add(new cOsdItem(tr("original title"))); break;
         case DESC_CATG: Add(new cOsdItem(tr("category"))); break;
         case DESC_SEAS: Add(new cOsdItem(tr("season and episode"))); break;
         case DESC_PRAT: Add(new cOsdItem(tr("rating"))); break;
         case DESC_SRAT: Add(new cOsdItem(tr("starrating"))); break;
         case DESC_REVW: Add(new cOsdItem(tr("review"))); break;
      }
   }

   SetCurrent(Get(current));
   SetHelpKeys();
   Display();
}

void cMenuSetupDescriptionFieldSequence::sequenceSwap(int a, int b)
{
   uchar tmp = sequence.seq[a];
   sequence.seq[a] = sequence.seq[b];
   sequence.seq[b] = tmp;
}

eOSState cMenuSetupDescriptionFieldSequence::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch (Key) {
         case kGreen:
            if (Current() > 0)
            {  // up
               sequenceSwap(Current() - 1, Current());
               CursorUp();
               Set();
            }
            break;
         case kYellow:
            if (Current() < Count() - 1)
            {  // down
               sequenceSwap(Current(), Current() + 1);
               CursorDown();
               Set();
            }
            break;
         case kRed:
            sequence = XMLTVConfig.GetDefaultDescrSequence();
            Set();
            break;
         case kOk:
            Store();  // only in case of OK: take over and save sequence 
            state = osBack;
            break;
         default:
            break;
      }
   }
   if (Key != kNone)
      SetHelpKeys();

   return state;
}

void cMenuSetupDescriptionFieldSequence::Store(void)
{
   XMLTVConfig.SetDescrSequence(sequence);
   XMLTVConfig.Save();
}


// --------------------------------------------------------------------------------------------------------
// sub-menu for each EPG source (main menu blue button (edit)) to select execution, pic download and en/disable each channel)
cMenuSetupSource::cMenuSetupSource(cEPGSource *epgSrc)
{
   cPlugin *plugin = cPluginManager::GetPlugin(PLUGIN_NAME_I18N);
   if (!plugin) return;
   if (!epgSrc) return;
   epgsrc = epgSrc;

   sel = NULL;

   day = 0;
   pin[0] = 0;

   SetPlugin(plugin);
   SetSection(cString::sprintf("%s '%s' : %s", trVDR("Plugin"), plugin->Name(), epgsrc->SourceName()));

   enabled = epgsrc->Enabled() ? 1 : 0;
   usePics = epgsrc->UsePics();
   execDays = epgsrc->ExecDays();
   execTime = epgsrc->ExecTime();
   daysInAdvance = epgsrc->DaysInAdvance();
   Set();
}

cMenuSetupSource::~cMenuSetupSource()
{
   if (sel) delete [] sel;
}

void cMenuSetupSource::Set(void)
{
    int current = Current();
    Clear();

   Add(new cMenuEditBoolItem(tr("enabled"), &enabled));
   Add(new cMenuEditDateItem(tr("update on"), &day, &execDays));
   Add(new cMenuEditTimeItem(tr("update at"), &execTime));
   Add(new cMenuEditIntItem(tr("days in advance"), &daysInAdvance, 1, epgsrc->MaxDaysProvided()));

   if (epgsrc->NeedPin())
   {
      if (epgsrc->Pin())
      {
         strncpy(pin, epgsrc->Pin(), sizeof(pin)-1);
         pin[sizeof(pin)-1] = 0;
      }
      Add(new cMenuEditStrItem(tr("pin"), pin, sizeof(pin)));
   }

   if (epgsrc->HasPics())
   {
      Add(new cMenuEditBoolItem(tr("download pictures"), &usePics));
   }

   SetCurrent(Get(current));
   SetHelpKeys();
   Display();
}

void cMenuSetupSource::SetHelpKeys(void)
{
   SetHelp(NULL);
}

eOSState cMenuSetupSource::ProcessKey(eKeys Key)
{
   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown) 
   {
      switch (Key)
      {
         case kOk:
            Store();
            state = osBack;
            break;

         case kBack:
            state = osBack;
            break;

         default:
            break;
      }
   }
   return state;
}

void cMenuSetupSource::Store(void)
{
   epgsrc->SetExecTime(execTime);
   epgsrc->SetExecDays(execDays);

   epgsrc->Enable(enabled == 1);
   epgsrc->SetDaysInAdvance(daysInAdvance);
   if (epgsrc->NeedPin())
      epgsrc->SetPin(pin);
   if (epgsrc->HasPics())
      epgsrc->SetUsePics(usePics);

   XMLTVConfig.StoreSourceParameter(epgsrc);
   XMLTVConfig.Save();
}

// --------------------------------------------------------------------------------------------------------
// mapping between one external channelID and one or more VDR channels and the corresponding flags
cMenuSetupMapping::cMenuSetupMapping(const char *EpgChannelName)
{
   cPlugin *plugin = cPluginManager::GetPlugin(PLUGIN_NAME_I18N);
   if (!plugin) return;

   hasMappedChannels = false;
   tmpEPGMapping = NULL;
   flags = 0;
   //optionsEPGAux[0] = tr("images");
   //optionsEPGAux[1] = tr("EPG and images");
   //optionsEPGAux[2] = tr("Aux field and images");
   //optionsEPGAux[3] = tr("EPG, Aux field and images");
   optionsMovieSeries[0] = trVDR("no");
   optionsMovieSeries[1] = tr("movies only");
   optionsMovieSeries[2] = tr("series only");
   optionsMovieSeries[3] = tr("movies + series only");
   optionsMovieSeries[4] = tr("always");
   optionsLines[0] = trVDR("no");
   optionsLines[1] = tr("single line");
   optionsLines[2] = tr("multiline");
   optionsYesNo[0] = trVDR("no");
   optionsYesNo[1] = trVDR("yes");

   SetPlugin(plugin);

   cEPGMapping* epgMapping = XMLTVConfig.EPGMappings()->GetMap(EpgChannelName);
   if (epgMapping)
      tmpEPGMapping = new cEPGMapping(epgMapping);
   else
      tmpEPGMapping = new cEPGMapping(EpgChannelName, NULL); // no mapping yet, create an empty one

   SetTitle(*cString::sprintf("%s - %s '%s': %s", trVDR("Setup"), trVDR("Plugin"), plugin->Name(), tmpEPGMapping->EPGChannelName()));

   flags = tmpEPGMapping->Flags();
   c1 = c2 = c3 = c4 = c5 = 0;

   // setup array of available sources
   sourceIndex = 0;
   int i = 0;
   sourcesList.Append(strdup(trVDR("none")));
   for (cEPGSource *src = XMLTVConfig.EPGSources()->First(); src; src = XMLTVConfig.EPGSources()->Next(src)) {
      if (src->Enabled() && src->ProvidesChannel(tmpEPGMapping->EPGChannelName())) {
         sourcesList.Append(strdup(src->SourceName()));
         i++;
         if (tmpEPGMapping->EPGSource() && !(strcmp(src->SourceName(), tmpEPGMapping->EPGSource()->SourceName())))
            sourceIndex = i;
      }
   }

   Set();
}

cMenuSetupMapping::~cMenuSetupMapping()
{
   delete tmpEPGMapping;
}


void cMenuSetupMapping::Set(void)
{
   int lineSrc, lineFirstMap, lineLastMap;

   int current = Current();
   Clear();

   hasMappedChannels = false;

   Add(new cMenuEditStraItem(tr("EPG source"), &sourceIndex, sourcesList.Size(), &sourcesList.At(0)));

   Add(NEWTITLE(tr("EPG source channel mappings")), true);
   lineMap = Current();
   for (int i = 0; i < tmpEPGMapping->ChannelMapList()->Size(); i++)
   {
      LOCK_CHANNELS_READ
      if (Channels)
      {
         const cChannel *chan = Channels->GetByChannelID(tmpEPGMapping->ChannelMapList()->At(i)->GetChannelID());
         if (chan)
         {
            cString buffer = cString::sprintf("%*i %s", CHNUMWIDTH, chan->Number(), chan->Name());
            Add(new cOsdItem(buffer), true);
            hasMappedChannels = true;
         }
      }
   }

   if (!hasMappedChannels)
   {
      Add(new cOsdItem(trVDR("none")), true);
   }

   Add(NEWTITLE(tr("EPG source channel options")), true);
   lineOpts = Current();

   c1 = Current();
   //Add(new cMenuEditMultiBitItem(tr("add to"), &flags, USE_CATEGORY_MASK, 0, 4, optionsEPGAux), true);
   //c1Add = Current();
   bool selectable = true; //(flags & USE_DESC_OR_AUX) > 0;
   cOsdItem *tmpItem;
   Add(tmpItem = new cMenuEditMultiBitItem(tr("title"), &flags, USE_TITLE, SHIFT_TITLE, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);
   Add(tmpItem = new cMenuEditMultiBitItem(tr("short text"), &flags, USE_SHORTTEXT, SHIFT_SHORTTEXT, MAX_MOVIESSERIES, optionsMovieSeries),true);
   //tmpItem->SetSelectable(selectable);
   Add(tmpItem = new cMenuEditMultiBitItem(tr("description"), &flags, USE_DESCRIPTION, SHIFT_DESCRIPTION, MAX_MOVIESSERIES, optionsMovieSeries),true);
   //tmpItem->SetSelectable(selectable);

   Add(tmpItem = new cMenuEditMultiBitItem(tr("country and date"), &flags, USE_COUNTRYYEAR, SHIFT_COUNTRYYEAR, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);

   Add(tmpItem = new cMenuEditMultiBitItem(tr("original title"), &flags, USE_ORIGTITLE, SHIFT_ORIGTITLE, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);
   Add(tmpItem = new cMenuEditMultiBitItem(tr("category"), &flags, USE_CATEGORIES, SHIFT_CATEGORIES, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);

   Add(tmpItem = new cMenuEditMultiBitItem(tr("credits"), &flags, USE_CREDITS, SHIFT_CREDITS, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);
   c2 = Current();
   if ((flags & USE_CREDITS) > 0)
   {
      // TRANSLATORS: note the leading blank!
      Add(tmpItem = new cMenuEditMultiBitItem(tr(" director"), &flags, CREDITS_DIRECTORS, SHIFT_CREDITS_DIRECTORS, 2, optionsYesNo), true);
      //tmpItem->SetSelectable(selectable);
      // TRANSLATORS: note the leading blank!
      Add(tmpItem = new cMenuEditMultiBitItem(tr(" actors"), &flags, CREDITS_ACTORS, SHIFT_CREDITS_ACTORS, 3, optionsLines), true);
      //tmpItem->SetSelectable(selectable);
      // TRANSLATORS: note the leading blank!
      Add(tmpItem = new cMenuEditMultiBitItem(tr(" other crew"), &flags, CREDITS_OTHERS, SHIFT_CREDITS_OTHERS, 2, optionsYesNo), true);
      //tmpItem->SetSelectable(selectable);
   }

   c3 = Current();
   Add(tmpItem = new cMenuEditMultiBitItem(tr("parental rating"), &flags, USE_PARENTAL_RATING, SHIFT_PARENTAL_RATING, 2, optionsYesNo), true);
   //tmpItem->SetSelectable(selectable);
   c4 = Current();
   if ((flags & USE_PARENTAL_RATING) > 0)
   {
      // TRANSLATORS: note the leading blank!
      Add(tmpItem = new cMenuEditMultiBitItem(tr(" parental rating as text"), &flags, PARENTAL_RATING_TEXT, SHIFT_PARENTAL_RATING_TEXT, 2, optionsYesNo), true);
      //tmpItem->SetSelectable(selectable);
   }
   Add(tmpItem = new cMenuEditMultiBitItem(tr("starrating"), &flags, USE_STAR_RATING, SHIFT_STAR_RATING, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);
   Add(tmpItem = new cMenuEditMultiBitItem(tr("review"), &flags, USE_REVIEW, SHIFT_REVIEW, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);

   Add(tmpItem = new cMenuEditMultiBitItem(tr("season and episode"), &flags, USE_SEASON_EPISODE, SHIFT_SEASON_EPISODE, MAX_MOVIESSERIES, optionsMovieSeries), true);
   //tmpItem->SetSelectable(selectable);
   c5 = Current();
   if ((flags & USE_SEASON_EPISODE) > 0)
   {
      // TRANSLATORS: note the leading blank!
      Add(tmpItem = new cMenuEditMultiBitItem(tr(" display"), &flags, USE_SEASON_EPISODE_MULTILINE, SHIFT_SEASON_EPISODE_MULTILINE, 2, &optionsLines[1]), true);
      //tmpItem->SetSelectable(selectable);
   }

   if (current == lineMap)
      current++;
   if (current == lineOpts)
      current--;

   SetCurrent(Get(current));
   SetHelpKeys();
   Display();
}

void cMenuSetupMapping::SetHelpKeys(void)
{
   if (Current() > lineOpts) {
      if (XMLTVConfig.EPGMappings())   // no Mappings yet => no keys
         SetHelp(NULL, NULL, tr("Button$Reset"), tr("Button$Copy"));   // reset & copy settings => active below channel mapping
      else
         SetHelp(NULL);
   }
   else {
      if (Current() > lineMap)
         SetHelp(hasMappedChannels ? tr("Button$Unmap") : NULL, tr("Button$Map"));    // add/remove VDR channels
      else 
         SetHelp(NULL);
   }
}

eOSState cMenuSetupMapping::ProcessKey(eKeys Key)
{
   cOsdItem *item = NULL;
   bool HadSubMenu = HasSubMenu();

   eOSState state = cOsdMenu::ProcessKey(Key);

   if (state == osUnknown)
   {
      switch ((int)Key)
      {
         case kOk:
            tmpEPGMapping->SetEpgSource(XMLTVConfig.EPGSources()->GetSource(sourcesList[sourceIndex]));
            Store();
            state = osBack;
            break;

         case kBack:
            state = osBack;
            break;

         case kRed:  // remove channel
            if (Current() > lineMap && Current() < lineOpts) {
               item = Get(Current());
               if (item) {
                  if (tmpEPGMapping) {
                     {
                        LOCK_CHANNELS_READ
                        tmpEPGMapping->RemoveChannel(Channels->GetByNumber(atoi(item->Text()))->GetChannelID());
                     }
                     Set();
                     state = osContinue;
                  }
               }
            }
            break;

         case kGreen: // add channel
            if (Current() > lineMap && Current() <= lineOpts)
               return AddSubMenu(new cMenuSetupMapping_AddChannel(tmpEPGMapping));
            break;

         case kYellow: // reset all channels to previous mapping (?) - Delete all flags!
            if ((Current() > lineOpts) && XMLTVConfig.EPGMappings()) {
               if (Skins.Message(mtInfo, tr("Reset channel options of all channels?")) == kOk) {
                  flags = 0;   // Reset all flags to 0x00
                  XMLTVConfig.EPGMappings()->SetAllFlags(flags);
                  XMLTVConfig.Save();
                  Set();
                  state = osContinue;
               }
            }
            break;

         case kBlue: // copy current mapping to all channels - or just flags without Source?
            if ((Current() > lineOpts) && XMLTVConfig.EPGMappings()) {
               if (Skins.Message(mtInfo, tr("Copy options to all channels?")) == kOk) {
                  XMLTVConfig.EPGMappings()->SetAllFlags(flags);
                  XMLTVConfig.Save();
                  Set();
                  state = osContinue;
               }
            }
            break;

         default:
            break;
      }
   }

   if (HadSubMenu && !HasSubMenu())
   {
      Set();
   }
   Display();

   if (!HasSubMenu() && Key != kNone) {
      if ((Current() == c1) || (Current() == c2) || (Current() == c3) || (Current() == c4) || (Current() == c5)) Set();
      SetHelpKeys();
   }

   return state;
}


void cMenuSetupMapping::Store()  // required by inherited class cMenuSetupPage
{
   cEPGMapping *map = XMLTVConfig.EPGMappings()->GetMap(tmpEPGMapping->EPGChannelName());
   tmpEPGMapping->SetFlags(flags);
   if (!map) {  // create new mapping
      XMLTVConfig.EPGMappings()->Add(new cEPGMapping(tmpEPGMapping));
   }
   else {  // copy new mapping to exisitng one
      *map = *tmpEPGMapping;
   }
   XMLTVConfig.Save();
}


// --------------------------------------------------------------------------------------------------------
cMenuSetupMapping_AddChannel::cMenuSetupMapping_AddChannel(cEPGMapping *TmpEPGMapping)
:cOsdMenu("", CHNUMWIDTH)
{
   tmpEPGMapping = TmpEPGMapping;
   SetHelp(NULL, NULL, tr("Button$Choose"));
   cPlugin *plugin = cPluginManager::GetPlugin(PLUGIN_NAME_I18N);
   SetTitle(*cString::sprintf("%s - %s '%s' : %s", trVDR("Setup"), trVDR("Plugin"), plugin->Name(), TmpEPGMapping->EPGChannelName()));

   Set();
}

void cMenuSetupMapping_AddChannel::Set()
{
   LOCK_CHANNELS_READ
   for (const cChannel *channel = Channels->First(); channel; channel = Channels->Next(channel))
   {
      if (channel->GroupSep())
         Add(new cOsdItem(*cString::sprintf("---\t%s ---", channel->Name()), osUnknown, false));
      else
      {
         cString buf = cString::sprintf("%d\t%s", channel->Number(), channel->Name());
         if (epgMappingExists(channel->GetChannelID()))
            Add(new cOsdItem(buf, osUnknown, false));
         else
            Add(new cOsdItem(buf));
      }
   }
}

bool cMenuSetupMapping_AddChannel::epgMappingExists(tChannelID channelid)
{  // returns true if a mapping exists in any map list
   if (!XMLTVConfig.EPGMappings() || !XMLTVConfig.EPGMappings()->Count())
      return false;

   for (int i = 0; i < XMLTVConfig.EPGMappings()->Count(); i++)
   {
      cEPGMapping *mapping = XMLTVConfig.EPGMappings()->Get(i);
      for (int x = 0; x < mapping->ChannelMapList()->Size(); x++)
      {
         if (mapping->ChannelMapList()->At(x)->GetChannelID() == channelid)
            return true;
      }
   }

   for (int x = 0; x < tmpEPGMapping->ChannelMapList()->Size(); x++)
   {
      if (tmpEPGMapping->ChannelMapList()->At(x)->GetChannelID() == channelid)
         return true;
   }

   return false;
}

eOSState cMenuSetupMapping_AddChannel::ProcessKey(eKeys Key)
{
   cOsdItem *item = NULL;
   eOSState state = cOsdMenu::ProcessKey(Key);
   if (state == osUnknown)
   {
      switch (Key)
      {
         case kBack:
            state = osBack;
            break;
         case kYellow:
         case kOk:
            item = Get(Current());
            if (item)
            {
               LOCK_CHANNELS_READ
               tmpEPGMapping->AddChannel(Channels->GetByNumber( atoi(item->Text()) )->GetChannelID());
            }
            state = osBack;
            break;
         default:
            break;
      }
   }
   return state;
}

// --------------------------------------------------------------------------------------------------------

// LOG menu
cMenuSetupLog::cMenuSetupLog(cEPGSource *Source)
: cMenuText("Title", "Text")
{
   cPlugin *plugin = cPluginManager::GetPlugin(PLUGIN_NAME_I18N);
   if (!plugin) return;

   SetTitle(*cString::sprintf("%s - %s '%s' : %s Log", trVDR("Setup"), trVDR("Plugin"), plugin->Name(), Source->SourceName()));
   source = Source;

   level = VIEW_ERROR;
   width = 0;
   font = NULL;
   lastRefresh = (time_t) 0;
   nextrun_str[0]=0;

   cSkinDisplayMenu *menu = DisplayMenu();
   if (menu)
   {
      width = menu->GetTextAreaWidth();
      font  = menu->GetTextAreaFont(false);
   }
   if (!width) width = Setup.OSDWidth;
   if (!font) font = cFont::GetFont(fontOsd);

   SetHelpKeys();
   Set();
}

void cMenuSetupLog::SetHelpKeys(void)
{
   SetHelp(tr("Button$Errors"), tr("Button$Info"), tr("Button$Debug"));
}

eOSState cMenuSetupLog::ProcessKey(eKeys Key)
{
   eOSState state = cMenuText::ProcessKey(Key);
   if (state == osContinue)
   {
      switch (Key)
      {
         case kRed:
            level = VIEW_ERROR;
            Set();
            state = osContinue;
            break;
         case kGreen:
            level = VIEW_INFO;
            Set();
            state = osContinue;
            break;
         case kYellow:
            level = VIEW_DEBUG;
            Set();
            state = osContinue;
            break;
         case kBack:
         case kOk:
            state = osBack;
            break;
         case kNone:
            {
               time_t now = time(NULL);
               if (source->ImportActive()) {
                  if (now > (lastRefresh + 5)) {
                     Set();
                     lastRefresh = now;
                  }
               }
               else
               {
                  if (lastRefresh) {
                     if (now > (lastRefresh+5)) {
                        Set();
                        lastRefresh = 0;
                     }
                  }
               }
            }
            break;
         default:
            break;
      }
   }
   return state;
}

void cMenuSetupLog::Set(void)
{
   cString text;
   int current = Current();
   Clear();
   time_t nextrun = source->NextRunTime();
   if (nextrun)
   {
      struct tm tm;
      localtime_r(&nextrun, &tm);
      strftime(nextrun_str, sizeof(nextrun_str) - 1, "%d %b %H:%M:%S", &tm);
   }
   text = cString::sprintf("xmltv4vdr Plugin:  '%s'\n%s:   %s\n", source->SourceName(), tr("next execution"),
                           source->ImportActive() ? tr("active") : (nextrun ?  nextrun_str : "-"));
   text.Append(cString::sprintf("---- %s (%s) ----\n", tr("log"), level == VIEW_ERROR ? tr("Button$Errors") : 
                   level == VIEW_INFO ? tr("Button$Info") : tr("Button$Debug")));
   if (source && !isempty(*source->GetLog()))
   {
      char *log = strdup(*source->GetLog());
      if (log)
      {
         char *saveptr;
         char *line = strtok_r(log, "\n", &saveptr);
         while (line)
         {
            if ((level == VIEW_DEBUG) || 
                (line[0] == 'I' && (level == VIEW_INFO)) ||
                (line[0] == 'E' && (level == VIEW_INFO || level == VIEW_ERROR)))
            {
               text.Append(++line);
               text.Append("\n");
            }
            line = strtok_r(NULL, "\n", &saveptr);
         }
         free(log);
      }
   }
   SetText(text);
   if (current > Count()) current = Count();
   SetCurrent(Get(current));
   Display();
}

/*
 * config.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include "config.h"
#include "debug.h"

// -------------------------------------------------------------
cConfigLine::cConfigLine(void)
{
   section = name = source = value = NULL;
}


cConfigLine::cConfigLine(const char *Section, const char *Name, const char *Value)
{
   section = strreplace(strdup(Section), '\n', 0);
   name = strreplace(strdup(Name), '\n', 0);
   source = NULL;
   value = strreplace(strdup(Value), '\n', 0);
}

cConfigLine::cConfigLine(const char *Section, const char *Name, const char *Source, const char *Value)
{
   section = strreplace(strdup(Section), '\n', 0);
   name = strreplace(strdup(Name), '\n', 0);
   source = strreplace(strdup(Source), '\n', 0);
   value = strreplace(strdup(Value), '\n', 0);
}

cConfigLine::~cConfigLine()
{
   free(section);
   free(name);
   free(source);
   free(value);
}

int cConfigLine::Compare(const cListObject &ListObject) const
{
   const cConfigLine *sl = (cConfigLine *)&ListObject;
   if (!section && !sl->section)
      return strcasecmp(name, sl->name);
   if (!section)
      return -1;
   if (!sl->section)
      return 1;
   int result = strcasecmp(section, sl->section);
   if (result == 0) {
      if (!source)
         return -1;
      if (!sl->source)
         return 1;
      result = strcasecmp(source, sl->source);
      if (result == 0)
         result = strcasecmp(name, sl->name);
   }
   return result;
}

bool cConfigLine::Parse(char *s)
{
   char *p = strchr(s, '=');
   if (p) {
      *p = 0;
      char *Name  = compactspace(s);
      char *Value = compactspace(p + 1);
      if (!isempty(Name)) {
         p = strchr(Name, '.');
         if (p) {
            *p = 0;
            char *Section = compactspace(Name);
            Name = compactspace(p + 1);
            if (!(*Section && *Name))
               return false;
            section = strdup(Section);
            source = NULL;
            if (!strcasecmp(section, "source"))
            {
               p = strrchr(Name, '.');
               if (p) {
                  *p = 0;
                  char *Extension = compactspace(p + 1);
                  source = strdup(Name);
                  name = strdup(Extension);
               }
            }
            else
               name = strdup(Name);
            value = strdup(Value);
            return true;
         }
      }
   }
   return false;
}

bool cConfigLine::Save(FILE *f)
{
   if (source)
      return fprintf(f, "%s.%s.%s = %s\n", section, source, name, value) > 0;
   else
      return fprintf(f, "%s.%s = %s\n", section, name, value) > 0;
}

// -------------------------------------------------------------
cXMLTVConfig XMLTVConfig;

cXMLTVConfig::cXMLTVConfig(void)
{
   epgSources = new cEPGSources;
   epgChannels = new cEPGChannels;
   DB_initialized = false;
   episodesServerPort = 2006;
   wakeup  = false;
   fixDuplTitleInShortttext = false;
   fhLogfile = NULL;
   houseKeeping = NULL;

   epgSourcesDir = EPGSOURCESDIR;
   descrSequence = GetDefaultDescrSequence();
}

cXMLTVConfig::~cXMLTVConfig(void)
{
   for (cEPGSource *source = epgSources->First(); source; source = epgSources->Next(source)) {
      XMLTVConfig.StoreSourceParameter(source);
   }
   XMLTVConfig.Save();

   delete epgSources;
   delete epgChannels;
}

bool cXMLTVConfig::Load(const char *FileName)
{
  if (cConfig<cConfigLine>::Load(FileName)) {
     bool result = true;
     for (cConfigLine *l = First(); l; l = Next(l)) {
         bool error = false;
            {
            if (!Parse(l->Name(), l->Extension(), l->Value(), l->Section()))
               error = true;
            }
         if (error) {
            esyslog("ERROR: unknown config parameter: %s = %s", l->Name(), l->Value());
            result = false;
            }
         }
     return result;
     }
  return false;
}

cConfigLine *cXMLTVConfig::Get(const char *Name, const char *Section)
{
   for (cConfigLine *l = First(); l; l = Next(l)) {
      if ((l->Section() != NULL) && (Section != NULL)) {
         if ((strcasecmp(l->Section(), Section) == 0) && strcasecmp(l->Name(), Name) == 0)
            return l;
      }
   }
   return NULL;
}

cConfigLine *cXMLTVConfig::Get(const char *Name, const char *Extension, const char *Section)
{
   for (cConfigLine *l = First(); l; l = Next(l)) {
      if ((l->Section() != NULL) && (Section != NULL) && (l->Extension() != NULL) && (Extension != NULL)) {
         if ((strcasecmp(l->Section(), Section) == 0) && strcasecmp(l->Extension(), Extension) == 0 && strcasecmp(l->Name(), Name) == 0)
            return l;
      }
   }
   return NULL;
}

bool cXMLTVConfig::Parse(const char *Name, const char *Extension, const char *Value, const char *Section)
{
   if (!strcmp(Section, "options")) {
      if (!strcasecmp(Name,"wakeup")) {
         wakeup = atoi(Value) == 1;
      }
      else if (!strcasecmp(Name,"fixDuplTitleInShortttext")) {
         fixDuplTitleInShortttext = atoi(Value) == 1;
      }
      else if (!strcasecmp(Name,"order")) {
         XMLTVConfig.SetDescrSequence(Value);
      }
   }
   else if (!strcmp(Section, "source")) {
      cEPGSource *src = epgSources->GetSource(Extension);
      if (src) {
         if      (!strcasecmp(Name, "daysInAdvance"))      src->SetDaysInAdvance(atoi(Value));
         else if (!strcasecmp(Name, "usePics"))            src->SetUsePics(atoi(Value) == 1);
         else if (!strcasecmp(Name, "execDays"))           src->SetExecDays(atoi(Value));
         else if (!strcasecmp(Name, "execTime"))           src->SetExecTime(atoi(Value));
         else if (!strcasecmp(Name, "enabled"))            src->Enable(atoi(Value) == 1);
         else if (!strcasecmp(Name, "Pin"))                src->SetPin(Value);
         else if (!strcasecmp(Name, "lastEventStarttime")) src->SetLastEventStarttime(atoi(Value));
         else if (!strcasecmp(Name, "lastSuccessfulRun"))  src->SetLastSuccessfulRun(atoi(Value));
      }
   }
   else if (!strcmp(Section, "channel")) {
      epgChannels->Add(new cEPGChannel(Name, Value));
   }

   return true;
}

void cXMLTVConfig::Store(const char *Name, const char *Ext, const char *Value, const char *Section)
{
   if (Name && *Name) {
      cConfigLine *l = Get(Name, Ext, Section);
      if (l)
         Del(l);
      if (Value)
         Add(new cConfigLine(Section, Name, Ext, Value));
   }
}

void cXMLTVConfig::Store(const char *Name, const char *Value, const char *Section)
{
   if (Name && *Name) {
      cConfigLine *l = Get(Name, Section);
      if (l)
         Del(l);
      if (Value)
         Add(new cConfigLine(Section, Name, Value));
   }
}

void cXMLTVConfig::StoreEpgChannel(cEPGChannel *EpgChannel)
{
   if (EpgChannel)
      Store(EpgChannel->EPGChannelName(), *EpgChannel->ToString(), "channel");
}

void cXMLTVConfig::StoreSourceParameter(cEPGSource *Source)
{
   if (Source) {
      Store("daysInAdvance", Source->SourceName(), cString::sprintf("%d", Source->DaysInAdvance()), "source");
      Store("usePics",       Source->SourceName(), Source->UsePics() ? "1" : "0", "source");
      Store("execDays",      Source->SourceName(), cString::sprintf("%d", Source->ExecDays()), "source");
      Store("execTime",      Source->SourceName(), cString::sprintf("%d", Source->ExecTime()), "source");
      Store("enabled",       Source->SourceName(), cString::sprintf("%d", Source->Enabled()), "source");
      if (Source->Pin())
         Store("Pin",        Source->SourceName(), Source->Pin(), "source");
      Store("lastEventStarttime", Source->SourceName(), cString::sprintf("%lu", (long int)Source->LastEventStarttime()), "source");
      Store("lastSuccessfulRun", Source->SourceName(), cString::sprintf("%lu", (long int)Source->LastSuccessfulRun()), "source");
      Sort();
   }
}

bool cXMLTVConfig::Save(void)
{
   Store("wakeup", wakeup ? "1" : "0", "options");
   Store("fixDuplTitleInShortttext", fixDuplTitleInShortttext ? "1" : "0", "options");
   Store("order",  GetDescrSequenceString(), "options");

   for (cEPGChannel *ch = epgChannels->First(); ch; ch = epgChannels->Next(ch)) {
      Store(ch->EPGChannelName(), *ch->ToString(), "channel");
   }

   Sort();

   if (cConfig<cConfigLine>::Save()) {
      return true;
   }
   return false;
}

void cXMLTVConfig::SetDescrSequence(const char *NewSequence)
{
   if (strlen(NewSequence) != DESC_COUNT) {
      isyslog("Couldn't read description sequence, loading default");
      descrSequence = GetDefaultDescrSequence();
   }
   else {
      for (int i = 0; i < DESC_COUNT; i++)
         descrSequence.seq[i] = NewSequence[i] - 'A';
   }
}


const char *cXMLTVConfig::GetDescrSequenceString()
{
   static char sequenceString[DESC_COUNT];
   for (int i = 0; i < DESC_COUNT; i++) {
      sequenceString[i] = 'A' + descrSequence.seq[i];
   }

   return sequenceString;
}

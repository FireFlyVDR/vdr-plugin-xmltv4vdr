/*
 * maps.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <inttypes.h>
#include "maps.h"
#include "debug.h"

// --------------------------------------------------------------------------------------------------------
cChannelList::~cChannelList()
{
  Clear();
}

int cChannelList::IndexOf(tChannelID channelID) const
{
  for (int i = 0; i < Size(); i++) {
      if (channelID == At(i)->GetChannelID())
         return i;
      }
  return -1;
}

void cChannelList::Clear(void)
{
  for (int i = 0; i < Size(); i++)
      free(At(i));
  cVector<cChannelIdObject *>::Clear();
}

// --------------------------------------------------------------------------------------------------------
cEPGMapping::cEPGMapping(const char *EPGChannelName, const char *Flags_and_Channels)
{  // setup mapping from config entry
   epgChannelName = EPGChannelName;
   flags = USE_NOTHING;
   epgSource = NULL;

   if (Flags_and_Channels)
   {
      char *flagsChannels = strdup (Flags_and_Channels);
      if (flagsChannels) {
         int token = 0;
         char *sp, *strg;
         strg = strtok_r(flagsChannels, ";", &sp);
         while (strg) {
            if (token == 0) {  // source
               if (cEPGSource *src = XMLTVConfig.EPGSources()->GetSource(strg))
                  epgSource = src;
               token++;
            }
            else if (token == 1)  // read flags
            {
               flags = strtoul(strg, NULL, 0);
               token++;
            }
            else {  // channels
               channelList.AppendUnique(new cChannelIdObject(strg));
            }
            strg = strtok_r(NULL, ";", &sp);
         }
         free(flagsChannels);
      }
   }

   tsyslog("added mapping for '%s' %s", *epgChannelName, epgSource?epgSource->SourceName() : "No Source");
   for (int i=0; i < channelList.Size(); i++) {
      tsyslog(" %2d: '%s'", i, *channelList.At(i)->GetChannelIDString());
   }
}

cEPGMapping::cEPGMapping(void)
{
   epgChannelName = NULL;
   flags = USE_SHORTTEXT;
   epgSource = NULL;
}


cEPGMapping::cEPGMapping(cEPGMapping *Mapping)
{
   epgChannelName = Mapping->epgChannelName;
   SetChannelList(&Mapping->channelList);
   epgSource = Mapping->EPGSource();
   flags = Mapping->flags;
}

cEPGMapping::~cEPGMapping()
{
   channelList.Clear();
}

cEPGMapping &cEPGMapping::operator= (const cEPGMapping &EpgMapping)
{
   channelList.Clear();
   for(int i = 0; i < EpgMapping.channelList.Size(); i++)
      channelList.AppendUnique(new cChannelIdObject(EpgMapping.channelList.At(i)->GetChannelID()));

   epgSource = EpgMapping.epgSource;
   flags = EpgMapping.flags;

   return *this;
}

cString cEPGMapping::ToString(void)
{
   cString text = cString::sprintf("%s;0x%" PRIX64, epgSource ? epgSource->SourceName() : "NULL", flags);

   for (int i = 0; i < channelList.Size(); i++)
   {
      text.Append(";");
      text.Append(channelList.At(i)->GetChannelIDString());
   }
   return text;
}


void cEPGMapping::AddChannel(tChannelID ChannelID)
{
   channelList.AppendUnique(new cChannelIdObject(ChannelID));
}

void cEPGMapping::SetChannelList(cChannelList *ChannelList)
{
   channelList.Clear();
   for(int i = 0; i < ChannelList->Size(); i++)
      channelList.AppendUnique(new cChannelIdObject(ChannelList->At(i)->GetChannelID()));
}

void cEPGMapping::RemoveChannel(tChannelID ChannelID, bool MarkOnlyInvalid)
{
   int ndx = channelList.IndexOf(ChannelID);
   if (ndx >= 0)
      channelList.Remove(ndx);
}

// --------------------------------------------------------------------------------------------------------
void cEPGMappings::Remove()
{
   cEPGMapping *map;
   while ((map = Last()) != NULL)
      Del(map);
}

cEPGSource *cEPGMappings::GetSource(const char *EPGchannelName)
{
   cEPGMapping *map = NULL;
   if (EPGchannelName) {
      map = First();
      while ((map != NULL) && (strcasecmp(map->EPGChannelName(), EPGchannelName))) {
         map = Next(map);
      }
   }

   return map ? map->EPGSource() : NULL;
}

cString cEPGMappings::GetActiveEPGChannels(const char *SourceName)
{
   cString epgChannels = "";
   if (SourceName) {
      cEPGMapping *map = First();
      while (map != NULL) {
         if (map->EPGSource() && !strcmp(SourceName, map->EPGSource()->SourceName())) {
            if (!isempty(epgChannels)) epgChannels.Append(" ");
            epgChannels.Append(map->EPGChannelName());
         }
         map = Next(map);
      }
   }
   return epgChannels;
}

cEPGMapping* cEPGMappings::GetMap(const char* EpgChannelName)
{
   if (EpgChannelName && Count())
   {
      for (cEPGMapping *map = First(); map; map = Next(map))
      {
         if (!strcmp(map->EPGChannelName(), EpgChannelName))
            return map;
      }
   }
   return NULL;
}

cEPGMapping *cEPGMappings::GetMap(tChannelID ChannelID)
{
   if (Count())
   {
      for (cEPGMapping *map = First(); map; map = Next(map))
      {
         for (int x = 0; x < map->ChannelMapList()->Size(); x++)
         {
            if (map->ChannelMapList()->At(x)->GetChannelID() == ChannelID) 
               return map;
         }
      }
   }
   return NULL;
}

void cEPGMappings::SetAllFlags(uint64_t flags)
{
   if (Count())
   {
      for (cEPGMapping *map = First(); map; map = Next(map))
      {
         map->SetFlags(flags);
      }
   }
}

bool cEPGMappings::ProcessChannel(const tChannelID ChannelID)
{
   if (Count()) {
      for (int i = 0; i < Count(); i++) {
         for (int x = 0; x < Get(i)->ChannelMapList()->Size(); x++) {
            if (Get(i)->ChannelMapList()->At(x)->GetChannelID() == ChannelID) 
               return Get(i)->EPGSource() != NULL;
         }
      }
   }
   return false;
}

/*
 * maps.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <inttypes.h>
#include "maps.h"
#include "debug.h"

// ================================ cChannelIDList ==============================================
cChannelIDList::~cChannelIDList()
{
  Clear();
}

int cChannelIDList::IndexOf(tChannelID channelID) const
{
  for (int i = 0; i < Size(); i++) {
      if (channelID == At(i)->GetChannelID())
         return i;
      }
  return -1;
}

void cChannelIDList::Clear(void)
{
  for (int i = 0; i < Size(); i++)
      free(At(i));
  cVector<cChannelIDObject *>::Clear();
}

// ================================ cEPGChannel ==============================================
cEPGChannel::cEPGChannel(const char *EPGChannelName, const char *Flags_and_Channels)
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
            else if (token == 1) { // read flags
               flags = strtoul(strg, NULL, 0);
               token++;
            }
            else {  // channels
               channelIDList.AppendUnique(new cChannelIDObject(strg));
            }
            strg = strtok_r(NULL, ";", &sp);
         }
         free(flagsChannels);
      }
   }

   tsyslog("added mapping for '%s' %s", *epgChannelName, epgSource?epgSource->SourceName() : "No Source");
   for (int i = 0; i < channelIDList.Size(); i++) {
      tsyslog(" %2d: '%s'", i, *channelIDList.At(i)->GetChannelIDString());
   }
}

cEPGChannel::cEPGChannel(void)
{
   epgChannelName = NULL;
   flags = USE_SHORTTEXT;
   epgSource = NULL;
}


cEPGChannel::cEPGChannel(cEPGChannel *NewEpgChannel)
{
   epgChannelName = NewEpgChannel->epgChannelName;
   SetChannelIDList(&NewEpgChannel->channelIDList);
   epgSource = NewEpgChannel->EPGSource();
   flags = NewEpgChannel->flags;
}

cEPGChannel::~cEPGChannel()
{
   channelIDList.Clear();
}

cEPGChannel &cEPGChannel::operator= (const cEPGChannel &EpgChannel)
{
   channelIDList.Clear();
   for(int i = 0; i < EpgChannel.channelIDList.Size(); i++)
      channelIDList.AppendUnique(new cChannelIDObject(EpgChannel.channelIDList.At(i)->GetChannelID()));

   flags = EpgChannel.flags;
   epgSource = EpgChannel.epgSource;

   return *this;
}

cString cEPGChannel::ToString(void)
{
   cString text = cString::sprintf("%s;0x%" PRIX64, epgSource ? epgSource->SourceName() : "NULL", flags);

   for (int i = 0; i < channelIDList.Size(); i++) {
      text.Append(";");
      text.Append(channelIDList.At(i)->GetChannelIDString());
   }
   return text;
}


void cEPGChannel::AddChannel(tChannelID ChannelID)
{
   channelIDList.AppendUnique(new cChannelIDObject(ChannelID));
}

void cEPGChannel::SetChannelIDList(cChannelIDList *ChannelIDList)
{
   channelIDList.Clear();
   for(int i = 0; i < ChannelIDList->Size(); i++)
      channelIDList.AppendUnique(new cChannelIDObject(ChannelIDList->At(i)->GetChannelID()));
}

void cEPGChannel::RemoveChannel(tChannelID ChannelID, bool MarkOnlyInvalid)
{
   int ndx = channelIDList.IndexOf(ChannelID);
   if (ndx >= 0)
      channelIDList.Remove(ndx);
}

// ================================ cEPGChannels ==============================================
void cEPGChannels::RemoveAll()
{
   cEPGChannel *epgChannel;
   while ((epgChannel = Last()) != NULL)
      Del(epgChannel);
}

cEPGSource *cEPGChannels::GetEpgSource(const char *EPGchannelName)
{
   cEPGChannel *epgChannel = NULL;
   if (EPGchannelName) {
      epgChannel = First();
      while ((epgChannel != NULL) && (strcasecmp(epgChannel->EPGChannelName(), EPGchannelName))) {
         epgChannel = Next(epgChannel);
      }
   }

   return epgChannel ? epgChannel->EPGSource() : NULL;
}

bool cEPGChannels::HasActiveEPGChannels(const char *SourceName)
{
   cString epgChannelNames = "";
   if (SourceName) {
      cEPGChannel *epgChannel = First();
      while (epgChannel != NULL) {
         if (epgChannel->EPGSource() && !strcmp(SourceName, epgChannel->EPGSource()->SourceName())) {
            return true;
         }
         epgChannel = Next(epgChannel);
      }
   }
   return false;
}

cString cEPGChannels::GetActiveEPGChannels(const char *SourceName)
{
   cString epgChannelNames = "";
   if (!isempty(SourceName)) {
      cEPGChannel *epgChannel = First();
      while (epgChannel != NULL) {
         if (epgChannel->EPGSource() && !strcmp(SourceName, epgChannel->EPGSource()->SourceName())) {
            if (!isempty(*epgChannelNames)) epgChannelNames.Append(" ");
            epgChannelNames.Append(epgChannel->EPGChannelName());
         }
         epgChannel = Next(epgChannel);
      }
   }

   return epgChannelNames;
}

bool cEPGChannels::IsActiveEPGChannel(const char *EpgChannelName, const char *SourceName)
{
   bool isActive = false;
   if (!isempty(EpgChannelName) && !isempty(SourceName)) {
      cEPGChannel *epgChannel = First();
      while (epgChannel != NULL && !isActive) {
         if (epgChannel->EPGChannelName() && !strcmp(EpgChannelName, epgChannel->EPGChannelName()))
            isActive = (epgChannel->EPGSource() && !strcmp(SourceName, epgChannel->EPGSource()->SourceName()));
         epgChannel = Next(epgChannel);
      }
   }

   return isActive;
}


cEPGChannel* cEPGChannels::GetEpgChannel(const char* EpgChannelName)
{
   if (EpgChannelName && Count()) {
      for (cEPGChannel *epgChannel = First(); epgChannel; epgChannel = Next(epgChannel)) {
         if (!strcmp(epgChannel->EPGChannelName(), EpgChannelName))
            return epgChannel;
      }
   }
   return NULL;
}

cEPGChannel *cEPGChannels::GetEpgChannel(tChannelID ChannelID)
{
   if (Count()) {
      for (cEPGChannel *epgChannel = First(); epgChannel; epgChannel = Next(epgChannel)) {
         for (int x = 0; x < epgChannel->ChannelIDList().Size(); x++) {
            if (epgChannel->ChannelIDList().At(x)->GetChannelID() == ChannelID)
               return epgChannel;
         }
      }
   }
   return NULL;
}

void cEPGChannels::SetAllFlags(uint64_t flags)
{
   if (Count()) {
      for (cEPGChannel *epgChannel = First(); epgChannel; epgChannel = Next(epgChannel)) {
         epgChannel->SetFlags(flags);
      }
   }
}

bool cEPGChannels::ProcessChannel(const tChannelID ChannelID)
{
   if (Count()) {
      for (int i = 0; i < Count(); i++) {
         for (int x = 0; x < Get(i)->ChannelIDList().Size(); x++) {
            if (Get(i)->ChannelIDList().At(x)->GetChannelID() == ChannelID)
               return Get(i)->EPGSource() != NULL;
         }
      }
   }
   return false;
}

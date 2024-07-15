/*
 * event.cpp: XmlTV4VDR plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <string>
#include "event.h"
#include "debug.h"


const char *cXMLTVStringList::ToString(const char *Delimiter)
{  /// convert String List to one cString with elements separated by delimiter, default ='@'
   toString = "";
   for (int i = 0; i < Size(); i++) {
      toString.Append(At(i));
      if (i < Size()-1) toString.Append(Delimiter);
   }
   return *toString;
}

int cXMLTVStringList::SetStringList(const char *StringList, const char *Delimiter)
{  /// convert cString to String List, split at Delimiter
   int added = 0;
   cStringList::Clear();
   if (!isempty(StringList)) {
      char *list = strdup(StringList);
      char *strtok_next;
      char *p = strtok_r(list, Delimiter, &strtok_next);
      while (p) {
         Append(strdup(p));
         added++;
         p = strtok_r(NULL, Delimiter, &strtok_next);
      }
      free(list);
   }
   return added;
}

bool cXMLTVStringList::AppendUnique(const char* String)
{  // Append a string which is not yet present
   bool added = false;
   if (!isempty(String)) {
      char *newString = strdup(String);
      compactspace(newString);
      if ((added = (-1 == Find(newString))))
         Append(newString);
      else
         free(newString);
   }
   return added;
}

int cXMLTVStringList::FindStr(const char *s) const
{  // return position of string s if found, else -1
   for (int i = 0; i < Size(); i++) {
      if (strstr(s, At(i)))
         return i;
      }
   return -1;
}

// -------------------------------------------------------------
cXMLTVEvent::cXMLTVEvent()
{
   Clear();
}

cXMLTVEvent::~cXMLTVEvent()
{
   Clear(); 
}

void cXMLTVEvent::Clear()
{
   source = NULL;
   channelID = tChannelID::InvalidID;
   starttime = 0;
   duration = 0;
   title = NULL;
   origTitle = NULL;
   shortText = NULL;
   description = NULL;
   country = NULL;
   xtEventID = eventid = 0;
   year = 0;
   tableID = 0xFF;
   version = 0xFF;
   credits.Clear();
   category.Clear();
   review.Clear();
   parentalRating.Clear();
   starRating.Clear();
   pics.Clear();
   season = 0;
   episode = 0;
   episodeOverall = 0;
}

void cXMLTVEvent::SetSource(const char *Source)
{
   source = Source;
   source.CompactChars(' ');
}

void cXMLTVEvent::SetChannelID(const char *ChannelID)
{
   channelID = tChannelID::FromString(ChannelID);
}

void cXMLTVEvent::SetTitle(const char *Title)
{
   title = Title;
}

void cXMLTVEvent::SetOrigTitle(const char *OrigTitle)
{
   origTitle = OrigTitle;
}

void cXMLTVEvent::SetShortText(const char *ShortText)
{
   shortText = ShortText;
}

void cXMLTVEvent::SetDescription(const char *Description)
{
   description = Description;
}

void cXMLTVEvent::SetXTEventID(time_t StartTime)
{  /// create and set extEventID from start time
   /// channel is not taken into account, only start time
   /// but needs only to be uniqe per channel

   if (xtEventID) return;
   // create own 16bit eventid
   struct tm tm;
   if (!localtime_r(&StartTime, &tm)) return;

   // this id cycles every 31 days, so if we have
   // 4 weeks programme in advance the id will
   // occupy already existing entries
   // till now, I'm only aware of 2 weeks
   // programme in advance
   int newid = ((tm.tm_mday) & 0x1F) << 11;
   newid |= ((tm.tm_hour & 0x1F) << 6);
   newid |= (tm.tm_min & 0x3F);

   xtEventID = newid & 0xFFFF;
}

void cXMLTVEvent::SetCountry(const char *Country)
{
    country = Country;
}

void cXMLTVEvent::SetCredits(const char *Credits)
{
   credits.SetStringList(Credits);
}

void cXMLTVEvent::AddCredits(const char *CreditType, const char *Credit, const char *Addendum)
{
   cString credit;
   if (Addendum)
      credit = cString::sprintf("%s%c%s%c%s", CreditType, TOKEN_DELIMITER, Credit, TOKEN_DELIMITER, Addendum);
   else
      credit = cString::sprintf("%s%c%s", CreditType, TOKEN_DELIMITER, Credit);
   credits.Append(strdup(compactspace((char*)*credit)));
}

void cXMLTVEvent::SetCategories(const char *Categories)
{
   category.SetStringList(Categories);
}

void cXMLTVEvent::AddCategory(const char *Category)
{
   category.AppendUnique(Category);
}

void cXMLTVEvent::SetReviews(const char *Reviews)
{
   review.SetStringList(Reviews);
}

void cXMLTVEvent::AddReview(const char *Review)
{
   review.AppendUnique(Review);
}

void cXMLTVEvent::SetParentalRating(const char *ParentalRating)
{  // Example:   "FSK|6" or "FSK|12" "FSK~6"
   parentalRating.SetStringList(ParentalRating);
}

void cXMLTVEvent::AddParentalRating(const char *System, const char *ParentalRating)
{  // set parental rating system and age e.g. "FSK|12"
   parentalRating.AppendUnique(strdup(compactspace((char*)*cString::sprintf("%s%c%s", System, TOKEN_DELIMITER, ParentalRating))));
   parentalRating.Sort();
}

void cXMLTVEvent::SetPics(const char* Pics)
{
   pics.SetStringList(Pics);
}

void cXMLTVEvent::AddPic(const char* Pic)
{
   pics.AppendUnique(Pic);
}

void cXMLTVEvent::SetStarRating(const char *StarRatings)
{
   starRating.SetStringList(StarRatings);
}

void cXMLTVEvent::AddStarRating(const char *System, const char *StarRating)
{
   starRating.Append(strdup(compactspace((char*)*cString::sprintf("%s%c%s", isempty(System) ? "*": System, TOKEN_DELIMITER, StarRating))));
}

void cXMLTVEvent::LinkPictures(bool LinkToEventID)
{  /// create links to the pictures of the event for the supplied channel and event ID
   //  LinkToEventID: also create links for eventID only (without ChannelID)
   if (eventid > 0 && !isempty(XMLTVConfig.ImageDir())) {
      for (int i = 0; i < pics.Size(); i++)
      {
         char *pic = pics.At(i);

         char *ext = strrchr(pics.At(i), '.');
         if (!ext) continue;

         ext++;

         cString lnk;
         struct stat statbuf;
         cString tgt = cString::sprintf("%s/%s-img/%s", XMLTVConfig.EPGSourcesDir(), *source, pics[i]);

         // link to EventID.jpg without channelID
         if (LinkToEventID)
         {
            lnk = cString::sprintf("%s/%u_%u.%s", XMLTVConfig.ImageDir(), eventid, i, ext);
            if (stat(*lnk, &statbuf) != -1)
               unlink(*lnk);

            if (symlink(*tgt, *lnk) == -1)
               esyslog("Could not create link, errno %d: %s -> %s", errno, lnk, *tgt);
         }

         // link to chanelID_EventID.jpg
         lnk = cString::sprintf("%s/%s_%u_%u.%s", XMLTVConfig.ImageDir(), *channelID.ToString(), eventid, i, ext);

         char buff[PATH_MAX];
         ssize_t len = readlink(*lnk, buff, sizeof(buff) - 1);
         if (len != -1)
         {  // link exists and target was successfully read
            buff[len] = '\0';
            if (strcmp(buff, *tgt))
            {  // target is not identical, unlink and create new link
               unlink(*lnk);
               if (symlink(*tgt, lnk) == -1)
                  esyslog("Could not create link, errno %d: %s -> %s", errno, lnk, *tgt);
            }
         }
         else {
            if (symlink(*tgt, lnk) == -1)
               esyslog("Could not create link errno %d: %s -> %s", errno, lnk, *tgt);
         }
      }
   }
}

bool cXMLTVEvent::evaluateFlags(uint64_t Flags, bool isMovie, bool isSeries)
{
   return (Flags == USE_ALWAYS) || (Flags == USE_MOVIES_ONLY && isMovie) || (Flags == USE_SERIES_ONLY && isSeries) || (Flags == USE_MOVIES_SERIES && (isMovie || isSeries));
}

#define USE_FLAGS(Type) ((Flags & USE_##Type) >> SHIFT_##Type)

bool cXMLTVEvent::FillEventFromXTEvent(cEvent *Event, uint64_t Flags)
{  /// modififies existing VDR Event
   /// return true if Event was modified

   bool modified = false;
   const char *creditTypes[10] = {  // dummy array for gettext translations
      trNOOP("director"),
      trNOOP("actor"),
      trNOOP("writer"),
      trNOOP("adapter"),
      trNOOP("producer"),
      trNOOP("composer"),
      trNOOP("editor"),
      trNOOP("presenter"),
      trNOOP("commentator"),
      trNOOP("guest")
   };

   if (Event)
   {
      time_t start, end;
      cString xmlAuxOthers;
      cString xmlAux = "";
      if (Event->Aux()) {
         char *xmlBegin = strstr((char *)Event->Aux(), xmlTagStart);
         if (xmlBegin) {
            xmlAuxOthers = cString(Event->Aux(), xmlBegin);
            char *xmlEnd = strstr((char *)Event->Aux(), xmlTagEnd);
            if (xmlEnd)
               xmlAuxOthers.Append(cString(xmlEnd + strlen(xmlTagEnd)));
         } else
            xmlAuxOthers = Event->Aux();
      }
      if (!isempty(*xmlAuxOthers)) {
         char *xmlBegin = strstr((char *)*xmlAuxOthers, xmlTagTVStart);
         if (xmlBegin) {
            xmlAuxOthers = cString(*xmlAuxOthers, xmlBegin);
            char *xmlEnd = strstr((char *)*xmlAuxOthers, xmlTagTVEnd);
            if (xmlEnd)
               xmlAuxOthers.Append(cString(xmlEnd + strlen(xmlTagTVEnd)));
         }
      }

      if ((Flags & USE_MASK) > 0)
      {
         uint64_t flag = 0;

         bool isMovie = false;
         for (int i = 0; !isMovie && i < MaxEventContents; i++) {   //TODO check shorttext for "Spielfilm" etc.
            isMovie |= (Event->Contents(i) & 0xF0) == ecgMovieDrama;
         }
         isMovie |= (category.Find("movie") >= 0);
         isMovie = isMovie && (Event->Duration() > 59*60); // movies must last longer than 59min

         bool isSeries = false;
         isSeries |= (category.Find("series") >= 0);
         for (int i = 0; !isSeries && i < category.Size(); i++) {
            isSeries |= (strstr(category.At(i), "Serie") != NULL);
         }

         // replace title
         if (evaluateFlags(USE_FLAGS(TITLE), isMovie, isSeries)) {
            if (!isempty(title)) {
               if (!Event->Title() || strcmp(Event->Title(), title)) {  // VDR Title empty
                  tsyslog("changing Title from \"%s\" to \"%s\"", Event->Title(), title);
                  Event->SetTitle(title);
                  modified = true;
               }
            }
         }

         // replace short text
         if (evaluateFlags(USE_FLAGS(SHORTTEXT), isMovie, isSeries)) {
            if (!isempty(shortText)) {
               if (!strcasecmp(shortText, Event->Title())) { // remove Shorttext if identical to Title
                  Event->SetShortText(NULL);
               }
               else {
                  if (!Event->ShortText() || strcmp(Event->ShortText(), shortText)) {
                     tsyslog("changing Shorttext2 from \"%s\" to \"%s\"", Event->ShortText(), shortText);
                     Event->SetShortText(shortText);
                     modified = true;
                  }
               }
            }
         }

         // construct description according to flags and order of fields
#define EPG_BUFFER_SIZE (KILOBYTE(4))
         char *buffer = MALLOC(char, EPG_BUFFER_SIZE);
         *buffer = 0;
         bool nonEmpty = false;
         cString desc = cString(buffer, true);
         //desc.Append(*cString::sprintf("XMLTV: %s%s%s\n", isMovie?"Movie":"", (isMovie && isSeries) ? ", " : "", isSeries?"Serie":""));
         struct descriptionSeq sequence = XMLTVConfig.GetDescrSequence();

         //if (Flags & USE_IN_DESC)
         {
            for (int i = 0; i < DESC_COUNT; i++) {
               switch (sequence.seq[i]) {
                  case DESC_DESC:   if (evaluateFlags(USE_FLAGS(DESCRIPTION), isMovie, isSeries) && !isempty(description)) {
                                       //desc.Append(*cString::sprintf("%s%s", nonEmpty ? "\nX:" : "X:", *description));
                                       if (nonEmpty) desc.Append("\n");
                                       desc.Append(*description);
                                    }
                                    else {  // no xt description => take DVB Description (if any)
                                       if (!isempty(Event->Description())) {
                                          //desc.Append(*cString::sprintf("%s%s", nonEmpty ? "\n" : "", Event->Description()));
                                          if (nonEmpty) desc.Append("\n");
                                          desc.Append(Event->Description());
                                       }
                                    }
                                    nonEmpty = true;
                                    break;
                  case DESC_CRED:   if (evaluateFlags(USE_FLAGS(CREDITS), isMovie, isSeries)) {
                                       bool isActor = false;
                                       const char *mline = ((Flags & CREDITS_ACTORS) == CREDITS_ACTORS_MULTILINE) ? "\n   " : ", ";
                                       for (int i = 0; i < credits.Size(); i++) {
                                          cString before, middle, after;
                                          int parts = strsplit(credits.At(i), '~', before, middle, after);
                                          if (!strcasecmp(*before, "director")) {
                                             if (Flags & CREDITS_DIRECTORS) {
                                                desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr("director"), *after));
                                                isActor = false;
                                                nonEmpty = true;
                                             }
                                          }
                                          else if(!strcasecmp(*before, "actor")) {
                                             if (Flags & CREDITS_ACTORS) {
                                                if (!isActor) {
                                                   desc.Append(*cString::sprintf("%s%s: ", nonEmpty ? "\n" : "", tr("actor")));
                                                }
                                                if (parts == 3)
                                                   desc.Append(*cString::sprintf("%s%s (%s)", isActor ? mline : "", *middle, *after));
                                                else
                                                   desc.Append(*cString::sprintf("%s%s", isActor ? mline : "", *after));
                                                isActor = true;
                                                nonEmpty = true;
                                             }
                                          }
                                          else {
                                             if (Flags & CREDITS_OTHERS) {
                                                desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr(*before), *after));
                                                isActor = false;
                                                nonEmpty = true;
                                             }
                                          }
                                       }
                                    }
                                    break;
                  case DESC_COYR:   if (evaluateFlags(USE_FLAGS(COUNTRYYEAR), isMovie, isSeries)) {
                                       if (!isempty(*country)) {
                                          desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr("country"), *country));
                                          nonEmpty = true;
                                       }

                                       if (year) {
                                          desc.Append(*cString::sprintf("%s%s: %d", nonEmpty ? (!isempty(*country)?", " : "\n") : "", tr("year"), year));
                                          nonEmpty = true;
                                       }
                                    }
                                    break;
                  case DESC_ORGT:   if (evaluateFlags(USE_FLAGS(ORIGTITLE), isMovie, isSeries)) {
                                       if (!isempty(*origTitle)) {
                                          desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr("original title"), *origTitle));
                                          nonEmpty = true;
                                       }
                                    }
                                    break;
                  case DESC_CATG:   if (evaluateFlags(USE_FLAGS(CATEGORIES), isMovie, isSeries)) {
                                       if (category.Size()) {
                                          desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr("category"), category.ToString(", ")));
                                          nonEmpty = true;
                                       }
                                    }
                                    break;
                  case DESC_SEAS:   if (evaluateFlags(USE_FLAGS(SEASON_EPISODE), isMovie, isSeries)) {
                                       bool seasonLine = false;
                                       if (season) {
                                          desc.Append(*cString::sprintf("%s%s: %d", nonEmpty ? "\n" : "", tr("season"), season));
                                          //desc.Append(*cString::sprintf("%s: %d\n", tr("season"), season));
                                          nonEmpty = true;
                                          seasonLine = (Flags & USE_SEASON_EPISODE_MULTILINE) == 0;
                                          xmlAux.Append(cString::sprintf("<season>%d</season>", season));
                                       }

                                       if (episode) {
                                          desc.Append(*cString::sprintf("%s%s: %d", nonEmpty ? (seasonLine?", " : "\n") : "", tr("episode"), episode));
                                          //desc.Append(*cString::sprintf("%s: %d\n", tr("episode"), episode));
                                          nonEmpty = true;
                                          //seasonLine = true;
                                          seasonLine = (Flags & USE_SEASON_EPISODE_MULTILINE) == 0;
                                          xmlAux.Append(cString::sprintf("<episode>%d</episode>", episode));
                                       }

                                       if (episodeOverall) {
                                          desc.Append(*cString::sprintf("%s%s: %d", nonEmpty ? (seasonLine?", " : "\n") : "", tr("overall episode"), episodeOverall));
                                          //desc.Append(*cString::sprintf("%s: %d\n", tr("overall episode"), episodeOverall));
                                          nonEmpty = true;
                                          xmlAux.Append(cString::sprintf("<episodeOverall>%d</episodeOverall>", episodeOverall));
                                       }
                                    }
                                    break;
                  case DESC_PRAT:   if (evaluateFlags(USE_FLAGS(PARENTAL_RATING), isMovie, isSeries)) {
                                       if (parentalRating.Size()) {
                                          bool ratingText = (Flags & PARENTAL_RATING_TEXT) == PARENTAL_RATING_TEXT;
                                          if (ratingText)
                                          {
                                             desc.Append(*cString::sprintf("%s%s: ", nonEmpty ? "\n" : "", tr("parental rating")));
                                             nonEmpty = true;
                                          }
                                          int xtAge = 19;
                                          for (int i = 0; i < parentalRating.Size(); i++) {
                                             cString before, middle, after;
                                             strsplit(parentalRating.At(i), '~', before, middle, after);
                                             if (ratingText)
                                                desc.Append(*cString::sprintf("%s%s: %s", i > 0 ? ", " : "", *before, *after));
                                             int pr = atoi(*after);
                                             if (pr > 0 && pr < xtAge) xtAge = pr;
                                          }
                                          int dvbAge = Event->ParentalRating();
                                          if (xtAge < 19 && ((dvbAge > 0 && dvbAge > xtAge) || dvbAge == 0))
                                             Event->SetParentalRating(xtAge);
                                       }
                                    }
                                    break;
                  case DESC_SRAT:   if (evaluateFlags(USE_FLAGS(STAR_RATING), isMovie, isSeries)) {
                                       if (starRating.Size()) {
                                          desc.Append(*cString::sprintf("%s%s: ", nonEmpty ? "\n" : "", tr("star rating")));
                                          nonEmpty = true;
                                          for (int i = 0; i < starRating.Size(); i++) {
                                             cString before, middle, after;
                                             strsplit(starRating.At(i), '~', before, middle, after);
                                             desc.Append(*cString::sprintf("%s%s", i > 0 ? ", " : "", *after));
                                          }
                                       }
                                    }
                                    break;
                  case DESC_REVW:   if (evaluateFlags(USE_FLAGS(REVIEW), isMovie, isSeries)) {
                                       if (review.Size()) {
                                          desc.Append(*cString::sprintf("%s%s: %s", nonEmpty ? "\n" : "", tr("review"), review.ToString("\n")));
                                          nonEmpty = true;
                                       }
                                    }
                                    break;
               }
            }
            if (nonEmpty) {
               Event->SetDescription(desc);
               modified = true;
            }
         } // end of USE_IN_DESC

         // write back Aux
         if (!isempty(*xmlAux)) {
            xmlAuxOthers.Append(xmlTagStart);
            xmlAuxOthers.Append(xmlAux);
            xmlAuxOthers.Append(xmlTagEnd);
         }
         Event->SetAux(*xmlAuxOthers);
      }

      SetEventID(Event->EventID());
      LinkPictures();
   }
   return modified;
}

#define MAXSTARTTIMEDELTA (16*60)

int cXMLTVEvent::CompareEvent(cEvent *DvbEvent, int matchOffset)
{  // compare this xtEvent with given VDR Event
   int match = 0;

   // abort if one title is missing
   if (isempty(DvbEvent->Title()) || isempty(*title))
      return 0;

   // calculate start time difference
   int starttimeDelta = abs(DvbEvent->StartTime() - starttime);

   // strip episode from DVB Title, e.g  "(912)" or "(1/2)"
   cString dvbTitle = DvbEvent->Title();
   if (strlen(*dvbTitle) > 2) {
      char *s = (char *)*dvbTitle;
      char *p = s + strlen(s) - 1;
      if (*p-- == ')') {
         while (strchr("0123456789/( ", *p) && p > s)
            p--;
         *++p = '\0';
      }
   }

   // when shorttext is missing use short description
   cString dvbShortText = "";
   if (!isempty(DvbEvent->ShortText()))
      dvbShortText = DvbEvent->ShortText();
   else if (!isempty(DvbEvent->Description()) && !isempty(*description)) {
      uint lenDescr = strlen(DvbEvent->Description());
      if (lenDescr < 110 && lenDescr != strlen(*description)) // avoid taking over ext description
         dvbShortText = DvbEvent->Description();
   }

   // strip episode from XML Title
   cString xmlTitle = title;
   if (strlen(*xmlTitle) > 2) {
      char *s = (char *)*xmlTitle;
      char *p = s + strlen(s) - 1;  //TODO Utf8
      if (*p-- == ')') {
         while (strchr("0123456789/( ", *p) && p > s)
            p--;
         *++p = 0;
      }
   }
   cString xmlShortText = *shortText;

   bool isemptyDVBShortText = isempty(*dvbShortText);
   bool isemptyXMLShortText = isempty(*xmlShortText);

   if (!isemptyDVBShortText)
   {  // DVB Event has ShortText
      if (!isemptyXMLShortText)
      {  // XML Event has ShortText, compare Titles and Shorttexts
         if (!strcasecmp(*dvbTitle, *xmlTitle))
         {  // titles match exactly
            if (!strcasecmp(*dvbShortText, *xmlShortText))
            {  // shorttexts match exactly
               match = matchOffset | 3;
            }
            else
               if (strstr(*xmlShortText, *dvbShortText) == *xmlShortText)
               {  // xml shorttext begins with dvb shorttext (private channels)
                  match = matchOffset | 4;
               }
         }
      }
   }

   if (isemptyDVBShortText || isemptyXMLShortText)
   {   // no ShortText, just compare Titles
      if (!strcasecmp(*dvbTitle, *xmlTitle)) {
         match = matchOffset | 6;
      }
      else
      {  // try with cleaned up titles
         cString clnDvbTitle = StringCleanup(*dvbTitle, true);
         cString clnXmlTitle = StringCleanup(*xmlTitle, true);
         if (!strcasecmp(*clnDvbTitle, *clnXmlTitle))
            match = matchOffset | 7;
      }
   }

   if (!match && !isemptyDVBShortText)
   {  // DVB Event has ShortText
      {  // check starttime and search xtEvent->Title in DvbEvent->Shorttext
         if ((starttimeDelta < MAXSTARTTIMEDELTA) && (strcasestr(dvbShortText, *xmlTitle) != NULL)) {
            match = matchOffset | 8;
         }
      }

      if (!match)
      {  // search DvbEvent substrings in xtEvent
         cString title_shorttext = cString::sprintf("%s %s", *xmlTitle, *xmlShortText);
         if ((strcasestr(*title_shorttext, dvbTitle) != NULL) && (strcasestr(*title_shorttext, dvbShortText) != NULL)) {
            match = matchOffset | 9;
         }
      }

      if (!match && !isemptyXMLShortText)
      {  // search xtEvent substrings in DvbEvent
         cString title_shorttext = cString::sprintf("%s %s", *dvbTitle, *dvbShortText);
         if ((strcasestr(*title_shorttext, *xmlTitle) != NULL) && (strcasestr(*title_shorttext, *xmlShortText) != NULL)) {
            match = matchOffset | 10;
         }

      }
   }  // END DVB event has shorttext

   if (!match)
   {  // check if nearly all words in XML event appear in DVB Event
      const char *delim = " .,;:-+!?&/()\"'`|";
      cString dvbTitleShorttext = *dvbTitle;
      if (!isemptyDVBShortText)
         dvbTitleShorttext = cString::sprintf("%s %s", *dvbTitle, *dvbShortText);
      dvbTitleShorttext = StringCleanup(*dvbTitleShorttext, false);

      cString xmlTitleShorttext = *xmlTitle;
      if (!isemptyXMLShortText)
         xmlTitleShorttext = cString::sprintf("%s %s", *xmlTitle, *xmlShortText);
      xmlTitleShorttext = StringCleanup(*xmlTitleShorttext, true);

      char xmlBuf[strlen(*xmlTitleShorttext) + 1];
      strcpy(xmlBuf, *xmlTitleShorttext);
      char *strtok_next;

      // search XML event words in DVB event
      int wordsDVB = 0;
      int wordsDVBmatch = 0;
      char *word = strtok_r(xmlBuf, delim, &strtok_next);
      while (word) {
         wordsDVB++;
         if (strcasestr(*dvbTitleShorttext, word) != NULL)
            wordsDVBmatch++;
         word = strtok_r(NULL, delim, &strtok_next);
      }

      if ((starttimeDelta < MAXSTARTTIMEDELTA) && (wordsDVBmatch >= min(wordsDVB, 3)) && ((wordsDVB*8/10) <= wordsDVBmatch))
      {
         match = matchOffset | 11;
         tsyslog(" - CmpEvents10: XinV %02X %d X:%2d-%2d E:%s X:%s E:%s~%s X:%s~%s", matchOffset, starttimeDelta, wordsDVB, wordsDVBmatch, *TimeToString(DvbEvent->StartTime()), *TimeToString(starttime),
                 DvbEvent->Title(), DvbEvent->ShortText(), *title, *shortText);
      }

      if (!match)
      {  // check if nearly all words in DVB event appear in XML Event
         int wordsXML = 0;
         int wordsXMLmatch = 0;

         // search DVB event words in XML event
         char dvbBuf[strlen(*dvbTitleShorttext) + 1];
         strcpy(dvbBuf, *dvbTitleShorttext);
         word = strtok_r(dvbBuf, delim, &strtok_next);
         while (word) {
            wordsXML++;
            if (strcasestr(*xmlTitleShorttext, word) != NULL)
               wordsXMLmatch++;
            word = strtok_r(NULL, delim, &strtok_next);
         }

         if ((starttimeDelta < MAXSTARTTIMEDELTA) && (wordsXMLmatch >= min(wordsXML, 3)) && ((wordsXML*8/10) <= wordsXMLmatch))
         {
            match = matchOffset | 12;
            tsyslog(" - CmpEvents20: VinX %02X %d X:%2d-%2d E:%s X:%s E:%s~%s X:%s~%s", matchOffset, starttimeDelta, wordsXML, wordsXMLmatch, *TimeToString(DvbEvent->StartTime()), *TimeToString(starttime),
                 DvbEvent->Title(), DvbEvent->ShortText(), *title, *shortText);
         }
      }
   }

   if (!match && isemptyDVBShortText) {  // Event has no ShortText
      // check if only starttime and titles match
      if ((starttimeDelta < MAXSTARTTIMEDELTA) && !strcasecmp(*dvbTitle, *xmlTitle)) {
         match = matchOffset | 13;
      }
   }

   if (!match) {  // Event has no ShortText
      // check if only starttime and titles match
      if ((starttimeDelta < MAXSTARTTIMEDELTA) && !strcasecmp(*dvbTitle, *xmlTitle)) {
         match = matchOffset | 14;
      }
   }

   if (match && starttimeDelta >= MAXSTARTTIMEDELTA)
      match |= 0x10;
   return match;
}


#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
uint LevenshteinDistance(uint *s1, uint l1, uint *s2, uint l2) {
   uint x, y, prevDiag, oldDiag;
   uint column[l1 + 1];

   for (y = 1; y <= l1; y++)
      column[y] = y;
   for (x = 1; x <= l2; x++) {
      column[0] = x;
      for (y = 1, prevDiag = x - 1; y <= l1; y++) {
         oldDiag = column[y];
         column[y] = MIN3(column[y] + 1, column[y - 1] + 1, prevDiag + (s1[y-1] == s2[x - 1] ? 0 : 1));
         prevDiag = oldDiag;
      }
   }
   return column[l1];
}

bool cXMLTVEvent::FetchSeasonEpisode()
{  /// fetch Season / Episode / EpisodeOverall from EpisodesDir (if available)
   /// return true if found

   // Title and ShortText are always UTF8 !
   if (isempty(*title)) return false;

   cString eventTitle = title;

   // if shorttext is missing use short description
   cString eventShortText = shortText;
   if (isempty(*shortText) & !isempty(*description) && strlen(*description) < 100) {
      eventShortText = description;
   }

   int rc = -1;
   cString episodesFile;
   struct stat statbuf;
   if (!isempty(eventShortText)) {
      episodesFile = cString::sprintf("%s/%s.episodes", XMLTVConfig.EpisodesDir(), *eventTitle);
      rc = stat(episodesFile, &statbuf);
#ifdef DBG_EPISODES2
      tsyslog("Episodefile1: %d %s", rc, *episodesFile);
#endif
      if (rc != 0) {
         episodesFile = cString::sprintf("%s/%s.episodes", XMLTVConfig.EpisodesDir(), strreplace((char *)*eventTitle, '*', '.'));
         rc = stat(episodesFile, &statbuf);
#ifdef DBG_EPISODES2
         tsyslog("Episodefile2: %d %s", rc, *episodesFile);
#endif
      }
   }

   char *p = (char *)strchrn(*eventTitle, ':', 1);
   if (rc != 0 && p) {
      *p = '\0';
      episodesFile = cString::sprintf("%s/%s.episodes", XMLTVConfig.EpisodesDir(), skipspace((char *)*eventTitle));
      rc = stat(episodesFile, &statbuf);
#ifdef DBG_EPISODES2
      tsyslog("Episodefile3: %d %s", rc, *episodesFile);
#endif
      if (rc != 0) {
         return false;
      }
      eventShortText = compactspace(p+1);
   }

#ifdef DBG_EPISODES
   tsyslog("using Episodefile %s", *episodesFile);
#endif

   bool found = false;
   if (!isempty(*eventShortText)) {
      FILE *f = fopen(episodesFile, "r");
      if (!f)
      {
         esyslog("opening %s failed", *episodesFile);
         return false;
      }

      int newSeason, newEpisode, newEpisodeOverall;
      cReadLine ReadLine;
      char *line;
      char episodeName[201];
      char extraCols[201];
      eventShortText = StringCleanup((char*)*eventShortText, true);
      while (((line = ReadLine.Read(f)) != NULL) && !found) {
         if (line[0] == '#') continue;
         if (sscanf(line, "%d\t%d\t%d\t%200[^\t\n]\t%200[^]\n]", &newSeason, &newEpisode, &newEpisodeOverall, episodeName, extraCols) >= 4)
         {
            cString cleanEpisodeName = StringCleanup(episodeName, true);
            if (!strcasecmp(*eventShortText, *cleanEpisodeName) ||       // match completely
               !strncasecmp(*eventShortText, *cleanEpisodeName, strlen(*cleanEpisodeName))) { // match complete in episodefile
               found = true;
               season = newSeason;
               episode = newEpisode;
               episodeOverall = newEpisodeOverall;
            }
         }
      }
      if (found) {  // set series category if an episode was found
         AddCategory("series");
#ifdef DBG_EPISODES
         tsyslog("found Episode: S:%d E:%d O:%d %s~%s", season, episode, episodeOverall, *title, *shortText);
#endif
      }
      else
         tsyslog("Episode not found: ->%s~%s<-", *eventTitle, *eventShortText);
      fclose(f);
   }

   return found;
}

int strsplit(const char *Source, const char delimiter, cString &Before, cString &Middle, cString &After)
{
   const char *split1 = strchr(Source, delimiter);
   if (split1) {
      Before = cString(Source, split1);
      const char *split2 = strchr(split1 + 1, delimiter);
      if (split2) {
         Middle = cString(split1 + 1, split2);
         After = cString(split2 + 1);
         return 3;
      }
      else {
         After = cString(split1 + 1);
         return 2;
      }
   }
   return 1;
}

cString StringNormalize(const char *String)
{
   cString src = String;

   struct sNormalize {
      const char *cSymbol;
      int lSymbol;
      const char rep;
   };

   const struct sNormalize normalizeTable[]= {   //  "-.,;:/!?()\''\"\n\r");
      {"-", strlen("-"), 0}, {".", strlen("."), 0}, {",", strlen(","), 0}, {";", strlen(";"), 0}, {":", strlen(":"), 0}, {"/", strlen("/"), 0}, {"!", strlen("!"), 0}, {"?", strlen("?"), 0},
      {"(", strlen("("), 0}, {")", strlen(")"), 0}, {"'", strlen("'"), 0}, {"’", strlen("’"), 0}, {"\"", strlen("\""), 0}, {"&", strlen("&"), 0}, {"\n", strlen("\n"), 0}, {"\r", strlen("\r"), 0},
      {"–", strlen("–"), 0}, {"´", strlen("´"), 0}, {"…", strlen("…"), 0}, {"„", strlen("„"), 0}, {"“", strlen("“"), 0}, {" ", strlen(" "), ' '}, {"‚", strlen("‚"), 0}, {"‘", strlen("‘"), 0},
      {"À", strlen("À"), 'a'}, {"Â", strlen("Â"), 'a'}, {"Æ", strlen("Æ"), 'a'}, {"Ç", strlen("Ç"), 'c'}, {"È", strlen("È"), 'e'}, {"É", strlen("É"), 'e'}, {"Ê", strlen("Ê"), 'e'},
      {"Ë", strlen("Ë"), 'e'}, {"Î", strlen("Î"), 'i'}, {"Ï", strlen("Ï"), 'i'}, {"Ô", strlen("Ô"), 'o'}, {"Œ", strlen("Œ"), 'a'}, {"Ù", strlen("Ù"), 'u'}, {"Û", strlen("Û"), 'u'}, {"Ÿ", strlen("Ÿ"), 'y'},
      {"à", strlen("à"), 'a'}, {"â", strlen("â"), 'a'}, {"æ", strlen("æ"), 'a'}, {"ç", strlen("ç"), 'c'}, {"è", strlen("è"), 'e'}, {"é", strlen("é"), 'e'}, {"ê", strlen("ê"), 'e'},
      {"ë", strlen("à"), 'e'}, {"î", strlen("î"), 'i'}, {"ï", strlen("ï"), 'i'}, {"ô", strlen("ô"), 'o'}, {"œ", strlen("œ"), 'a'}, {"ù", strlen("ù"), 'u'}, {"û", strlen("û"), 'u'}, {"ÿ", strlen("ÿ"), 'y'} };

   char *s = (char *)*src;
   char *d = (char *)*src;
   while (*s) {
      int l = Utf8CharLen(s);
      bool found = false;
      for (uint c = 0; !found && c < sizeof(normalizeTable)/sizeof(struct sNormalize); c++) {
         if ((found = 0 == strncmp(s, normalizeTable[c].cSymbol, normalizeTable[c].lSymbol))) {
            if (normalizeTable[c].rep != 0)
               *d++ = normalizeTable[c].rep;
         }
      }
      if (!found) {
         strncpy(d, s, l);
         d += l;
      }
      s += l;
   }
   *d = 0;

   struct sUmlaut {
      const char *cSymbol;
      const char *replacement;
   };

   const struct sUmlaut umlautTable[] = {{"ß", "ss"}, {"Ä", "ae"}, {"ä", "ae"}, {"Ö", "oe"}, {"ö", "oe"}, {"Ü", "ue"}, {"ü", "ue"}};

   s = (char *)*src;
   while(*s) {
      bool found = false;
      int l = Utf8CharLen(s);
      if ( l == 1) {
         *s = tolower(*s);
         s++;
      }
      else
      {
         for (uint c = 0; !found && c < sizeof(umlautTable)/sizeof(struct sUmlaut); c++) {
            if (!strncmp(s, umlautTable[c].cSymbol, 2)) {
               memcpy(s, umlautTable[c].replacement, 2);
            }
         }
         s += l;
      }
   }

   compactspace((char *)*src);
   return src;
}

cString StringCleanup(const char *String, bool ExtendedRemove, bool ExtendedRemoveWithSpace)
{
   cString s;

   if (!isempty((const char *)String))
   {
      s = String;
      const char *toBeRemoved = "\n\r";
      if (ExtendedRemove) {
         if (ExtendedRemoveWithSpace)
            toBeRemoved = " .,;:-+!?&/()\"'`|\n\r";
         else
            toBeRemoved = ".,;:-+!?&/()\"'`|\n\r";
      }

      const char em_dash[4]     = {(char)0xE2, (char)0x80, (char)0x93 , (char)0x00};
      const char apostroph[4]   = {(char)0xE2, (char)0x80, (char)0x99 , (char)0x00};
      const char ellipsis[4]    = {(char)0xE2, (char)0x80, (char)0xA6 , (char)0x00};
      const char nbsp[3]        = {(char)0xC2, (char)0xA0, (char)0x00};
      const char lower_quote[4] = {(char)0xE2, (char)0x80, (char)0x9E , (char)0x00};
      const char upper_quote[4] = {(char)0xE2, (char)0x80, (char)0x9C , (char)0x00};
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, em_dash))
#endif
         s = strreplace((char *)*s, em_dash, "-");
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, apostroph))
#endif
         s = strreplace((char *)*s, apostroph, "'");
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, ellipsis))
#endif
         s = strreplace((char *)*s, ellipsis, "...");
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, nbsp))
#endif
         s = strreplace((char *)*s, nbsp, " ");
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, lower_quote))
#endif
         s = strreplace((char *)*s, lower_quote, "\"");
#if defined(APIVERSNUM) && APIVERSNUM < 20608
      while (strstr(*s, upper_quote))
#endif
         s = strreplace((char *)*s, upper_quote, "\"");

      char *src = (char *)*s;
      char *dst = src;
      while (*src) {
         uint l = Utf8CharLen(src);
         if (l > 1 || !strchr(toBeRemoved, *src)) {
            memcpy(dst, src, l);
            dst += l;
         }
         src += l;
      }
      *dst = 0;

      // remove multiple and trailing spaces
      s = compactspace((char *)*s);
   }
   return s;
}

#ifndef __AVAHI4VDR_HELPER_H
#define __AVAHI4VDR_HELPER_H

#include <vdr/tools.h>


class cAvahiHelper
{
private:
  cStringList  _list;

public:
  // instantiate with an option string like "key1=value1,key2=value2,key2=value3,key3=value4,..."
  // comma and backslashes in values must be escaped with a backslash
  cAvahiHelper(const char *Options)
  {
    if (Options != NULL) {
       char *s = strdup(Options);
       size_t len = strlen(Options);
       size_t pos1 = 0;
       //dsyslog("avahi4vdr-helper: options = '%s'", s);
       // skip initial commas
       while ((pos1 < len) && (s[pos1] == ','))
             pos1++;
       size_t pos2 = pos1;
       while (pos2 < len) {
             // de-escape backslashed characters
             if ((s[pos2] == '\\') && (pos2 < (len - 1))) {
                memmove(s + pos2, s + pos2 + 1, len - pos2 - 1);
                len--;
                s[len] = 0;
                pos2++;
                continue;
                }
             if ((s[pos2] == ',') && (s[pos2 - 1] != '\\')) {
                s[pos2] = 0;
                //dsyslog("avahi4vdr-helper: add '%s'", s + pos1);
                _list.Append(strdup(s + pos1));
                pos1 = pos2 + 1;
                }
             pos2++;
             }
       if (pos2 > pos1) {
          //dsyslog("avahi4vdr-helper: add '%s'", s + pos1);
          _list.Append(strdup(s + pos1));
          }
       free(s);
       }
  };

  // if Number is greater than zero, returns the n'th appearance of the parameter
  const char *Get(const char *Name, int Number = 0)
  {
    if (Name == NULL)
       return NULL;
    size_t name_len = strlen(Name);
    if (name_len == 0)
       return NULL;

    for (int i = 0; i < _list.Size(); i++) {
        const char *text = _list[i];
        if (text == NULL)
           continue;
        size_t len = strlen(text);
        if (len > name_len) {
           if ((strncmp(text, Name, name_len) == 0) && (text[name_len] == '=')) {
              Number--;
              if (Number < 0) {
                 //dsyslog("avahi4vdr-helper: found parameter '%s'", text);
                 return text + name_len + 1;
                 }
              }
           }
        }
    return NULL;
  };

  // get the number of appearances of parameter "Name"
  int Count(const char *Name)
  {
    if (Name == NULL)
       return 0;
    size_t name_len = strlen(Name);
    if (name_len == 0)
       return 0;

    int count = 0;
    for (int i = 0; i < _list.Size(); i++) {
        const char *text = _list[i];
        if (text == NULL)
           continue;
        size_t len = strlen(text);
        if (len > name_len) {
           if ((strncmp(text, Name, name_len) == 0) && (text[name_len] == '='))
              count++;
           }
        }
    return count;
  };
};

#endif


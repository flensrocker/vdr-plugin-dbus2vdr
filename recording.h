#ifndef __DBUS2VDR_RECORDING_H
#define __DBUS2VDR_RECORDING_H

#include "object.h"


class cDBusRecordings;

class cDBusRecordingsConst : public cDBusObject
{
friend class cDBusRecordings;

private:
  cDBusRecordingsConst(const char *NodeInfo);

public:
  cDBusRecordingsConst(void);
  virtual ~cDBusRecordingsConst(void);
};

class cDBusRecordings : public cDBusRecordingsConst
{
public:
  cDBusRecordings(void);
  virtual ~cDBusRecordings(void);
};

#endif

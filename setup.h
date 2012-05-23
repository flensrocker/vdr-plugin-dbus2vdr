#ifndef __DBUS2VDR_SETUP_H
#define __DBUS2VDR_SETUP_H

#include <limits.h>

#include "message.h"


class cDBusMessageSetup : public cDBusMessage
{
friend class cDBusDispatcherSetup;

private:
  class cSetupBinding : public cListObject
  {
  private:
    cSetupBinding() {}

  public:
    enum eType { dstString, dstInt32 };

    const char *Name;
    eType Type;
    void *Value;
    // String
    int StrMaxLength;
    // Int32
    int Int32MinValue;
    int Int32MaxValue;

    static cSetupBinding *NewString(void* value, const char *name, int maxLength)
    {
      cSetupBinding *b = new cSetupBinding();
      b->Name = name;
      b->Type = dstString;
      b->Value = value;
      b->StrMaxLength = maxLength;
      return b;
    }

    static cSetupBinding *NewInt32(void* value, const char *name, int minValue = 0, int maxValue = INT_MAX)
    {
      cSetupBinding *b = new cSetupBinding();
      b->Name = name;
      b->Type = dstInt32;
      b->Value = value;
      b->Int32MinValue = minValue;
      b->Int32MaxValue = maxValue;
      return b;
    }
  };
  static cList<cSetupBinding> _bindings;

public:
  enum eAction { dmsList, dmsGet, dmsSet, dmsDel };

  virtual ~cDBusMessageSetup(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageSetup(eAction action, DBusConnection* conn, DBusMessage* msg);
  void List(void);
  void Get(void);
  void Set(void);
  void Del(void);

  eAction _action;
};

class cDBusDispatcherSetup : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherSetup(void);
  virtual ~cDBusDispatcherSetup(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif

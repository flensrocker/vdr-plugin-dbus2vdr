#ifndef __DBUS2VDR_MAINLOOP_H
#define __DBUS2VDR_MAINLOOP_H

#include <gio/gio.h>


class cDBusMainLoop
{
private:
  GMainContext    *_context;
  GMainLoop       *_loop;
  GThread         *_thread;
  
  GMutex           _started_mutex;
  GCond            _started_cond;
  bool             _started;

  static gpointer  do_loop(gpointer data);

public:
  cDBusMainLoop(GMainContext *Context);
  virtual ~cDBusMainLoop(void);
};

#endif

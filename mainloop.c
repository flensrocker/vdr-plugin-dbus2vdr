#include "mainloop.h"

#include <vdr/tools.h>


cDBusMainLoop::cDBusMainLoop(GMainContext *Context)
{
  _context = Context;
  _loop = NULL;
  _started = false;
  g_mutex_init(&_started_mutex);
  g_cond_init(&_started_cond);
  g_mutex_lock(&_started_mutex);
  _thread = g_thread_new("mainloop", do_loop, this);
  while (!_started)
        g_cond_wait(&_started_cond, &_started_mutex);
  g_mutex_unlock(&_started_mutex);
  g_mutex_clear(&_started_mutex);
  g_cond_clear(&_started_cond);
}

cDBusMainLoop::~cDBusMainLoop(void)
{
  if (_loop != NULL)
     g_main_loop_quit(_loop);
  if (_thread != NULL) {
     g_thread_join(_thread);
     g_thread_unref(_thread);
     _thread = NULL;
     }
}

gpointer  cDBusMainLoop::do_loop(gpointer data)
{
  dsyslog("dbus2vdr: mainloop started");
  if (data != NULL) {
     cDBusMainLoop *mainloop = (cDBusMainLoop*)data;
     g_mutex_lock(&mainloop->_started_mutex);
     mainloop->_started = true;
     g_cond_signal(&mainloop->_started_cond);
     g_mutex_unlock(&mainloop->_started_mutex);
     mainloop->_loop = g_main_loop_new(mainloop->_context, FALSE);
     if (mainloop->_loop != NULL) {
        g_main_loop_run(mainloop->_loop);
        g_main_loop_unref(mainloop->_loop);
        mainloop->_loop = NULL;
        }
     }
  dsyslog("dbus2vdr: mainloop stopped");
  return NULL;
}

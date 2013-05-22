#include "timer.h"
#include "common.h"
#include "helper.h"

#include <vdr/timers.h>


namespace cDBusTimersHelper
{
  static const char *_xmlNodeInfoConst =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_TIMER_INTERFACE"\">\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"timer\"     type=\"as\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Next\">\n"
    "      <arg name=\"replycode\" type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"number\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"seconds\"   type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"start\"     type=\"t\" direction=\"out\"/>\n"
    "      <arg name=\"stop\"      type=\"t\" direction=\"out\"/>\n"
    "      <arg name=\"title\"     type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static const char *_xmlNodeInfo =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_TIMER_INTERFACE"\">\n"
    "    <method name=\"New\">\n"
    "      <arg name=\"timer\"          type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Delete\">\n"
    "      <arg name=\"number\"         type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"timer\"     type=\"as\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Next\">\n"
    "      <arg name=\"replycode\" type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"number\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"seconds\"   type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"start\"     type=\"t\" direction=\"out\"/>\n"
    "      <arg name=\"stop\"      type=\"t\" direction=\"out\"/>\n"
    "      <arg name=\"title\"     type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static void New(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);
    if (*option) {
       cTimer *timer = new cTimer;
       if (timer->Parse(option)) {
          Timers.Add(timer);
          Timers.SetModified();
          isyslog("timer %s added", *timer->ToDescr());
          cDBusHelper::SendReply(Invocation, 250, *cString::sprintf("%d %s", timer->Index() + 1, *timer->ToText()));
          return;
          }
       else
          cDBusHelper::SendReply(Invocation, 501, "Error in timer settings");
       delete timer;
       }
    else
       cDBusHelper::SendReply(Invocation, 501, "Missing timer settings");
  };

  static void Delete(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int number = 0;
    g_variant_get(Parameters, "(i)", &number);
    if (!Timers.BeingEdited()) {
       cTimer *timer = Timers.Get(number - 1);
       if (timer) {
          if (!timer->Recording()) {
             isyslog("deleting timer %s", *timer->ToDescr());
             Timers.Del(timer);
             Timers.SetModified();
             cDBusHelper::SendReply(Invocation, 250, *cString::sprintf("Timer \"%d\" deleted", number));
             }
          else
             cDBusHelper::SendReply(Invocation, 550, *cString::sprintf("Timer \"%d\" is recording", number));
          }
       else
          cDBusHelper::SendReply(Invocation, 501, *cString::sprintf("Timer \"%d\" not defined", number));
       }
    else
       cDBusHelper::SendReply(Invocation, 550, "Timers are being edited - try again later");
  }

  static void List(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(as)"));
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("as"));

    cString text;
    const char *tmp;
    for (int i = 0; i < Timers.Count(); i++) {
        cTimer *timer = Timers.Get(i);
        if (timer) {
           text = timer->ToText(true);
           tmp = stripspace((char*)*text);
           g_variant_builder_add(array, "s", tmp);
           }
        }
    
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  };

  static void Next(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int returncode = 250;
    int number = -1;
    int seconds = 0;
    time_t start = 0;
    time_t stop = 0;
    const char *title = "";
    cTimer *t = Timers.GetNextActiveTimer();
    const cEvent *e;
    if (t) {
       start = t->StartTime();
       number = t->Index() + 1;
       seconds = start - time(NULL);
       stop = t->StopTime();
       e = t->Event();
       if ((e != NULL) && (e->Title() != NULL))
          title = e->Title();
       }
    else
       returncode = 550;

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(iiitts)"));
    g_variant_builder_add(builder, "i", returncode);
    g_variant_builder_add(builder, "i", number);
    g_variant_builder_add(builder, "i", seconds);
    g_variant_builder_add(builder, "t", start);
    g_variant_builder_add(builder, "t", stop);
    g_variant_builder_add(builder, "s", title);
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  };
}


cDBusTimersConst::cDBusTimersConst(const char *NodeInfo)
:cDBusObject("/Timers", NodeInfo)
{
  AddMethod("List", cDBusTimersHelper::List);
  AddMethod("Next", cDBusTimersHelper::Next);
}

cDBusTimersConst::cDBusTimersConst(void)
:cDBusObject("/Timers", cDBusTimersHelper::_xmlNodeInfoConst)
{
  AddMethod("List", cDBusTimersHelper::List);
  AddMethod("Next", cDBusTimersHelper::Next);
}

cDBusTimersConst::~cDBusTimersConst(void)
{
}

cDBusTimers::cDBusTimers(void)
:cDBusTimersConst(cDBusTimersHelper::_xmlNodeInfo)
{
  AddMethod("New", cDBusTimersHelper::New);
  AddMethod("Delete", cDBusTimersHelper::Delete);
}

cDBusTimers::~cDBusTimers(void)
{
}

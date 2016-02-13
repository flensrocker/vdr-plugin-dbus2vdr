#include "device.h"
#include "common.h"
#include "helper.h"

#include <vdr/device.h>


int cDBusDevices::_requestedPrimaryDevice = -1;
cMutex cDBusDevices::_requestedPrimaryDeviceMutex;

void cDBusDevices::SetPrimaryDeviceRequest(int index)
{
  cMutexLock MutexLock(&_requestedPrimaryDeviceMutex);
  _requestedPrimaryDevice = index;
}

int cDBusDevices::RequestedPrimaryDevice(bool clear)
{
  cMutexLock MutexLock(&_requestedPrimaryDeviceMutex);
  int ret = _requestedPrimaryDevice;
  if (clear)
     _requestedPrimaryDevice = -1;
  return ret;
}

namespace cDBusDevicesHelper
{
  static const char *_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_DEVICE_INTERFACE"\">\n"
    "    <method name=\"GetPrimary\">\n"
    "      <arg name=\"index\"        type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"number\"       type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"hasDecoder\"   type=\"b\" direction=\"out\"/>\n"
    "      <arg name=\"isPrimary\"    type=\"b\" direction=\"out\"/>\n"
    "      <arg name=\"name\"         type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"RequestPrimary\">\n"
    "      <arg name=\"index\"        type=\"i\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"device\"       type=\"a(iibbs)\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static void AddDevice(GVariantBuilder *Variant, const cDevice *Device, bool AsStruct)
  {
    gint32 index = -1;
    gint32 number = -1;
    gboolean hasDecoder = FALSE;
    gboolean isPrimary = FALSE;
    cString name = "";
    if (Device != NULL) {
       index = Device->CardIndex();
       number = Device->DeviceNumber();
       hasDecoder = Device->HasDecoder();
       isPrimary = Device->IsPrimaryDevice();
       name = Device->DeviceName();
       cDBusHelper::ToUtf8(name);
       }

    if (AsStruct)
       g_variant_builder_add(Variant, "(iibbs)", index, number, hasDecoder, isPrimary, *name);
    else {
       g_variant_builder_add(Variant, "i", index);
       g_variant_builder_add(Variant, "i", number);
       g_variant_builder_add(Variant, "b", hasDecoder);
       g_variant_builder_add(Variant, "b", isPrimary);
       g_variant_builder_add(Variant, "s", *name);
       }
  }

  static void GetPrimary(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(iibbs)"));
    const cDevice *dev = cDevice::PrimaryDevice();
    AddDevice(builder, dev, false);
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  }

  static void RequestPrimary(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gint32 index = -1;
    g_variant_get(Parameters, "(i)", &index);
    cDBusDevices::SetPrimaryDeviceRequest(index);
    g_dbus_method_invocation_return_value(Invocation, NULL);
  }

  static void List(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(iibbs)"));

    for (int i = 0; i < cDevice::NumDevices(); i++) {
       const cDevice *dev = cDevice::GetDevice(i);
       if (dev != NULL)
          AddDevice(array, dev, true);
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a(iibbs))"));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  }
}


cDBusDevices::cDBusDevices(void)
:cDBusObject("/Devices", cDBusDevicesHelper::_xmlNodeInfo)
{
  AddMethod("GetPrimary", cDBusDevicesHelper::GetPrimary);
  AddMethod("RequestPrimary", cDBusDevicesHelper::RequestPrimary);
  AddMethod("List", cDBusDevicesHelper::List);
}

cDBusDevices::~cDBusDevices(void)
{
}

#ifndef __DBUS2VDR_OSD_H
#define __DBUS2VDR_OSD_H

#include "message.h"

#include <vdr/osd.h>


class cDBusOsdProvider;

class cDBusOsd : public cOsd
{
private:
  friend class cDBusOsdProvider;

  static int   osd_number;

  cDBusOsdProvider& provider;

  int          osd_index;
  cString      osd_dir;
  int          counter;

protected:
  cDBusOsd(cDBusOsdProvider& Provider, int Left, int Top, uint Level);
  // virtual void SetActive(bool On) { cOsd::SetActive(On); }

public:
  virtual ~cDBusOsd();

  // virtual cPixmap *CreatePixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort = cRect::Null);
  // virtual void DestroyPixmap(cPixmap *Pixmap);
  // virtual void DrawImage(const cPoint &Point, const cImage &Image);
  // virtual void DrawImage(const cPoint &Point, int ImageHandle);
  // virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas) { return cOsd::CanHandleAreas(Areas, NumAreas); }
  // virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);

  // virtual void SaveRegion(int x1, int y1, int x2, int y2);
  // virtual void RestoreRegion(void);
  // virtual eOsdError SetPalette(const cPalette &Palette, int Area);

  // virtual void DrawPixel(int x, int y, tColor Color);
  // virtual void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false);
  // virtual void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault);
  // virtual void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color);
  // virtual void DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants = 0);
  // virtual void DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type);

  virtual void Flush(void);
       ///< Actually commits all data to the OSD hardware.
       ///< Flush() should return as soon as possible.
       ///< For a true color OSD using the default implementation with in memory
       ///< pixmaps, the Flush() function should basically do something like this:
       ///<
       ///<  LOCK_PIXMAPS;
       ///<  while (cPixmapMemory *pm = RenderPixmaps()) {
       ///<        int w = pm->ViewPort().Width();
       ///<        int h = pm->ViewPort().Height();
       ///<        int d = w * sizeof(tColor);
       ///<        MyOsdDrawPixmap(Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y(), pm->Data(), w, h, h * d);
       ///<        delete pm;
       ///<        }
};

class cDbusOsdMsg : public cListObject
{
public:
  const char *action;
  cString     file;
  int         left, top, vx, vy;

  cDbusOsdMsg(const char *Action, const cString& File, int Left, int Top, int Vx, int Vy)
   :action(Action),file(File),left(Left),top(Top),vx(Vx),vy(Vy)
  {
  }

  virtual ~cDbusOsdMsg(void);
};

class cDBusOsdProvider : public cOsdProvider, public cThread
{
private:
  cMutex             msgMutex;
  cCondVar           msgCond;
  cList<cDbusOsdMsg> msgQueue;

protected:
  virtual cOsd *CreateOsd(int Left, int Top, uint Level);
  virtual bool ProvidesTrueColor(void) { return true; }

  // virtual int StoreImageData(const cImage &Image);
  // virtual void DropImageData(int ImageHandle);

  virtual void Action(void);

public:
  cDBusOsdProvider(void);
  virtual ~cDBusOsdProvider();

  void SendMessage(cDbusOsdMsg *Msg);
};

class cDBusDispatcherOSD : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherOSD(void);
  virtual ~cDBusDispatcherOSD(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif

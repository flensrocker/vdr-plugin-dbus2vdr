#include "osd.h"

#include <png++/image.hpp>
#include <png++/rgba_pixel.hpp>

#include <vdr/videodir.h>


cDBusOsd::cDBusOsd(int Left, int Top, uint Level)
 : cOsd(Left, Top, Level)
{
  isyslog("dbus2vdr: new osd %d, %d, %d", Left, Top, Level);
}

cDBusOsd::~cDBusOsd()
{
  isyslog("dbus2vdr: delete osd");
}

eOsdError cDBusOsd::SetAreas(const tArea *Areas, int NumAreas)
{
  for (int i = 0; i < NumAreas; i++)
      isyslog("dbus2vdr: osd area %d = %d %d %d %d %d", i, Areas[i].x1, Areas[i].y1, Areas[i].x2, Areas[i].y2, Areas[i].bpp);
  return cOsd::SetAreas(Areas, NumAreas);
}

static png::rgba_pixel tColor2png_pixel(tColor Color)
{
  return png::rgba_pixel((Color >> 16) & 0xff, (Color >> 8) & 0xff, Color & 0xff, (Color >> 24) & 0xff);
}

void cDBusOsd::Flush(void)
{
  static int counter = 0;
  png::image<png::rgba_pixel> result(Left() + Width() + 1, Top() + Height() + 1);

  bool write = false;
  LOCK_PIXMAPS;
  while (cPixmapMemory *pm = RenderPixmaps()) {
        write = true;
        int w = pm->ViewPort().Width();
        int h = pm->ViewPort().Height();
        // int d = w * sizeof(tColor);
        // MyOsdDrawPixmap(Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y(), pm->Data(), w, h, h * d);
        const tColor *data = (const tColor *)pm->Data();
        for (int y = 0; y < h ; y++) {
            int py = Top() + pm->ViewPort().Y() + y;
            for (int x = 0; x < w; x++) {
                int px = Left() + pm->ViewPort().X() + x;
                result[py][px] = tColor2png_pixel(data[x + y * w]);
                }
            }
        delete pm;
        }

  if (write) {
     cString filename = cString::sprintf("%s/dbusosd-%08x.png", VideoDirectory, counter);
     isyslog("dbus2vdr: flush osd to %s", *filename);
     result.write(*filename);
     counter++;
     }
}


cDBusOsdProvider::cDBusOsdProvider(void)
{
  isyslog("dbus2vdr: new osd provider");
}

cDBusOsdProvider::~cDBusOsdProvider()
{
  isyslog("dbus2vdr: delete osd provider");
}

cOsd *cDBusOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
  return new cDBusOsd(Left, Top, Level);
}

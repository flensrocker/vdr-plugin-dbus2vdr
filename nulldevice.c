#include "nulldevice.h"

#include <vdr/osd.h>
#include <vdr/player.h>

class cNullOsd : public cOsd {
  public:
    cNullOsd(int Left, int Top, uint Level) : cOsd(Left, Top, Level) {}
    virtual ~cNullOsd() {}

    virtual cPixmap *CreatePixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort = cRect::Null) { return NULL; }
    virtual void DestroyPixmap(cPixmap *Pixmap) {}
    virtual void DrawImage(const cPoint &Point, const cImage &Image) {}
    virtual void DrawImage(const cPoint &Point, int ImageHandle) {}
    virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas) { return oeOk; }
    virtual eOsdError SetAreas(const tArea *Areas, int NumAreas) { return oeOk; }
    virtual void SaveRegion(int x1, int y1, int x2, int y2) {}
    virtual void RestoreRegion(void) {}
    virtual eOsdError SetPalette(const cPalette &Palette, int Area) { return oeOk; }
    virtual void DrawPixel(int x, int y, tColor Color) {}
    virtual void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false) {}
    virtual void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault) {}
    virtual void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color) {}
    virtual void DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants = 0) {}
    virtual void DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type) {}
    virtual void Flush(void) {}
};

class cNullOsdProvider : public cOsdProvider {
  protected:
    virtual cOsd *CreateOsd(int Left, int Top, uint Level) { return new cNullOsd(Left, Top, Level); }
    virtual bool ProvidesTrueColor(void) { return true; }
    virtual int StoreImageData(const cImage &Image) { return 0; }
    virtual void DropImageData(int ImageHandle) {}

  public:
    cNullOsdProvider() : cOsdProvider() {}
    virtual ~cNullOsdProvider() {}
};


class cNullPlayer : public cPlayer {
public:
  cNullPlayer(void);
  virtual ~cNullPlayer(void);
};

cNullPlayer::cNullPlayer(void)
{
}

cNullPlayer::~cNullPlayer()
{
  Detach();
}


class cNullControl : public cControl {
public:
  static cNullPlayer *Player;

  virtual void Hide(void) {}

  cNullControl(void);

  virtual ~cNullControl(void);
};

cNullPlayer *cNullControl::Player;

cNullControl::cNullControl(void)
 :cControl(Player = new cNullPlayer)
{
}

cNullControl::~cNullControl()
{
  delete Player;
  Player = NULL;
}


void cNullDevice::MakePrimaryDevice(bool On)
{
  cDevice::MakePrimaryDevice(On);
  if (On) {
     new cNullOsdProvider();
     if (cNullControl::Player != NULL) {
        cControl::Launch(new cNullControl());
        cControl::Attach();
        }
     }
  else if (cNullControl::Player)
     cControl::Shutdown();
}

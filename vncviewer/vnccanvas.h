#ifndef VNCCANVAS_H
#define VNCCANVAS_H

#include <QQuickPaintedItem>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include "rdr/types.h"

class QPainter;
class Surface;
class PlatformPixelBuffer;

namespace rfb {
  class Point;
}

class VncCanvas : public QQuickPaintedItem
{
  Q_OBJECT

public:
  VncCanvas(QQuickItem *parent = nullptr);
  virtual ~VncCanvas();

protected:
  void paint(QPainter *painter) override;
//  QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;

private:
  QSGGeometry m_geometry;
  QSGFlatColorMaterial m_material;


  // DesktopWindow.h
  unsigned char overlayAlpha;
  struct timeval overlayStart;

  bool firstUpdate;
  bool delayedFullscreen;
  bool delayedDesktopSize;

  bool keyboardGrabbed;
  bool mouseGrabbed;

  struct statsEntry {
    unsigned ups;
    unsigned pps;
    unsigned bps;
  };
  struct statsEntry stats[100];

  struct timeval statsLastTime;
  unsigned statsLastUpdates;
  unsigned statsLastPixels;
  unsigned statsLastPosition;

  Surface *statsGraph;


    
  // Viewport.h
  PlatformPixelBuffer* frameBuffer;

  rfb::Point *lastPointerPos;
  int lastButtonMask;

  typedef std::map<int, rdr::U32> DownMap;
  DownMap downKeySym;

#ifdef WIN32
  bool altGrArmed;
  unsigned int altGrCtrlTime;
#endif

  bool firstLEDState;

  bool pendingServerClipboard;
  bool pendingClientClipboard;

  int clipboardSource;

  rdr::U32 menuKeySym;
  int menuKeyCode, menuKeyFLTK;
//  Fl_Menu_Button *contextMenu;

  bool menuCtrlKey;
  bool menuAltKey;

//  Fl_RGB_Image *cursor;
  rfb::Point *cursorHotspot;
};

#endif // VNCCANVAS_H

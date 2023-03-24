#ifndef VNCCANVAS_H
#define VNCCANVAS_H

#include <QQuickItem>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>

class VncCanvas : public QQuickItem
{
  Q_OBJECT

public:
  VncCanvas(QQuickItem *parent = nullptr);
  virtual ~VncCanvas();

protected:
  QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;

private:
  QSGGeometry m_geometry;
  QSGFlatColorMaterial m_material;
};

#endif // VNCCANVAS_H

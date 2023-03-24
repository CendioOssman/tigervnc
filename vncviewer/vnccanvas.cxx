#include <QQuickItem>
#include <QSGGeometryNode>
#include "vnccanvas.h"

VncCanvas::VncCanvas(QQuickItem *parent)
  : QQuickItem(parent)
  , m_geometry(QSGGeometry::defaultAttributes_Point2D(), 3)
{
  setFlag(ItemHasContents);
  m_material.setColor(Qt::red);
}

VncCanvas::~VncCanvas()
{
}

QSGNode *VncCanvas::updatePaintNode(QSGNode *anode, UpdatePaintNodeData *)
{
  QSGGeometryNode *node = dynamic_cast<QSGGeometryNode*>(anode);
  if (!node) {
    node = new QSGGeometryNode;
  }
  QSGGeometry::Point2D *v = m_geometry.vertexDataAsPoint2D();
  const QRectF rect = boundingRect();
  v[0].x = rect.left();
  v[0].y = rect.bottom();
  v[1].x = rect.left() + rect.width() / 2;
  v[1].y = rect.top();
  v[2].x = rect.right();
  v[2].y = rect.bottom();
  node->setGeometry(&m_geometry);
  node->setMaterial(&m_material);
  return node;
}

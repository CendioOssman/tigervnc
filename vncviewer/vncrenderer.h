#ifndef VNCRENDERER_H
#define VNCRENDERER_H

#include <QQuickFramebufferObject>

#define LOGO

class VNCFramebuffer : public QQuickFramebufferObject
{
    Q_OBJECT
public:
    Renderer *createRenderer() const override;
};

#endif // VNCRENDERER_H

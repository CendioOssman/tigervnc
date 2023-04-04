#ifndef LOGORENDERER_H
#define LOGORENDERER_H

#include <QOpenGLFunctions>
#include <QtGui/qvector3d.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qopenglshaderprogram.h>

#include <QTime>
#include <QVector>

class LogoRenderer
{

public:
    LogoRenderer();
    ~LogoRenderer();

    void render();
    void initialize();

private:

    qreal   m_fAngle;
    qreal   m_fScale;

    void paintContent();
    void createGeometry();

    QVector<QVector3D> m_vertices;
    QVector<QVector3D> m_normals;
    QOpenGLShaderProgram m_program1;
    int m_vertexAttr1;
    int m_normalAttr1;
    int m_matrixUniform1;
    int m_textureLocation1;
};
#endif

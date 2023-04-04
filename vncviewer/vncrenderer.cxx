#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QtGui/qopenglshaderprogram.h>
#include <QtWin>
#include <QVector>
#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"
#include "vncrenderer.h"

class VNCRenderer : public QQuickFramebufferObject::Renderer
{
public:
  VNCRenderer()
  {
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0, 0, 0, 1);

    QOpenGLShader *vshader1 = new QOpenGLShader(QOpenGLShader::Vertex, &m_program1);
    const char *vsrc1 =
        "#version 330 core\n"
        "layout(location=0) in vec2 vertex;\n"
        "smooth out vec2 vUV;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vec4(vertex*2.0-1,0,1);\n"
        "    vUV = vertex;\n"
        "}\n";
    vshader1->compileSourceCode(vsrc1);

    QOpenGLShader *fshader1 = new QOpenGLShader(QOpenGLShader::Fragment, &m_program1);
    const char *fsrc1 =
        "#version 330 core\n"
        "layout (location=0) out vec4 vFragColor;\n"
        "smooth in vec2 vUV;\n"
        "uniform sampler2D textureMap;\n"
        "void main(void)\n"
        "{\n"
        "    vFragColor = texture(textureMap, vUV);\n"
        "}\n";
    fshader1->compileSourceCode(fsrc1);

    m_program1.addShader(vshader1);
    m_program1.addShader(fshader1);
    m_program1.link();

    m_vertexAttr1 = m_program1.attributeLocation("vertex");
    m_textureLocation1 = m_program1.uniformLocation("textureMap");

    createGeometry();

    f->glClearColor(0, 0, 0, 1.0f);

    f->glGenTextures(1, &m_texture);
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, m_texture);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer->width(), framebuffer->height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
  }

  void render() override {
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    QImage pixels = textureData();
    f->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixels.width(), pixels.height(), GL_BGRA, GL_UNSIGNED_BYTE, pixels.bits());
    //f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixels.width(), pixels.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.bits());
//    PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
//    f->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, framebuffer->width(), framebuffer->height(), GL_BGRA, GL_UNSIGNED_BYTE, framebuffer->bits());

    f->glFrontFace(GL_CW);
    f->glCullFace(GL_FRONT);
    f->glEnable(GL_CULL_FACE);
    f->glEnable(GL_DEPTH_TEST);

    m_program1.bind();
    m_program1.setUniformValue(m_textureLocation1, 0);
    m_program1.enableAttributeArray(m_vertexAttr1);
    m_program1.setAttributeArray(m_vertexAttr1, m_vertices.constData());
    f->glDrawArrays(GL_QUADS, 0, m_vertices.size());
    m_program1.disableAttributeArray(m_vertexAttr1);
    m_program1.release();

    f->glDisable(GL_DEPTH_TEST);
    f->glDisable(GL_CULL_FACE);

    update();
  }

  QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    return new QOpenGLFramebufferObject(size, format);
  }

private:
  QVector<QVector3D> m_vertices;
  QOpenGLShaderProgram m_program1;
  GLuint m_texture;
  int m_vertexAttr1;
  int m_textureLocation1;

  void createGeometry()
  {
    m_vertices.clear();

    m_vertices << QVector3D( 0,  0,  0);
    m_vertices << QVector3D( 1,  0,  0);
    m_vertices << QVector3D( 1,  1,  0);
    m_vertices << QVector3D( 0,  1,  0);
    m_vertices << QVector3D( 0,  0,  0);
  }

  QImage textureData()
  {
#if 1

#if defined(WIN32)
    PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
    HBITMAP hBitmap = framebuffer->hbitmap();
    HDC hdc = GetDC(nullptr);
    QImage image = QtWin::imageFromHBITMAP(hdc, hBitmap, framebuffer->width(), framebuffer->height());
    ReleaseDC(nullptr, hdc);
    return image;
#elif defined(__APPLE__)
    // TODO
    return QImage();
#else
    // TODO
    return QImage();
#endif

#else
    QImage image("C:\\Temp\\sample-large.jpg");
    return image;
#endif
  }
};

QQuickFramebufferObject::Renderer *VNCFramebuffer::createRenderer() const
{
  return new VNCRenderer();
}


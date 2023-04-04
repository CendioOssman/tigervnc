#include "logorenderer.h"
#include <QPainter>
#include <QPaintEngine>
#include <QGLWidget>
#include <QtWin>
#include <math.h>

#include "appmanager.h"
#include "vncconnection.h"
#include "PlatformPixelBuffer.h"

bool first = true;
GLuint texture;

LogoRenderer::LogoRenderer()
{
}

LogoRenderer::~LogoRenderer()
{
}

void LogoRenderer::paintContent()
{
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  m_program1.enableAttributeArray(m_vertexAttr1);
  m_program1.setAttributeArray(m_vertexAttr1, m_vertices.constData());
  f->glDrawArrays(GL_QUADS, 0, m_vertices.size());
  m_program1.disableAttributeArray(m_normalAttr1);
  m_program1.disableAttributeArray(m_vertexAttr1);
}

void LogoRenderer::initialize()
{
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  f->glClearColor(0, 0, 0, 1.0f);

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
//      "    highp vec3 color = texture2D(textureMap, vUV.st).rgb;\n"
//      "    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);\n"
      "}\n";
  fshader1->compileSourceCode(fsrc1);

  m_program1.addShader(vshader1);
  m_program1.addShader(fshader1);
  m_program1.link();

  m_vertexAttr1 = m_program1.attributeLocation("vertex");
  m_textureLocation1 = m_program1.uniformLocation("textureMap");

  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

  m_fAngle = 0;
  m_fScale = 1;
  createGeometry();
#if 0
  QImage image("C:\\Temp\\sample-large.jpg");
  f->glGenTextures(1, &texture);
  f->glActiveTexture(GL_TEXTURE0);
  f->glBindTexture(GL_TEXTURE_2D, texture);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
#endif
}

void LogoRenderer::render()
{
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  f->glDepthMask(true);

  //f->glClearColor(0.5f, 0.5f, 0.7f, 1.0f);
  f->glClearColor(0, 0, 0, 1.0f);
  f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 1
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  //QImage tmpImage("C:\\Temp\\sample-large.jpg");
  //QImage image = QGLWidget::convertToGLFormat(tmpImage);
  //QImage image = tmpImage.mirrored();
  //QImage image = tmpImage.convertToFormat(QImage::Format_RGBA8888);

  PlatformPixelBuffer *framebuffer = (PlatformPixelBuffer *)AppManager::instance()->connection()->framebuffer();
  HBITMAP hBitmap = framebuffer->hbitmap();
  HDC hdc = GetDC(NULL);
  QImage image = QtWin::imageFromHBITMAP(hdc, hBitmap, framebuffer->width(), framebuffer->height());

  //QImage image("C:\\Temp\\sample-large.jpg");
//  if (!first) {
//    f->glDeleteTextures(1, &texture);
//    first = false;
//  }
  f->glGenTextures(1, &texture);
  f->glActiveTexture(GL_TEXTURE0);
#endif
  f->glBindTexture(GL_TEXTURE_2D, texture);
#if 1
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
  //f->glTexSubImage2D(texture, 0, 0, 0, image.width(), image.height(), GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
#endif

  f->glFrontFace(GL_CW);
  f->glCullFace(GL_FRONT);
  f->glEnable(GL_CULL_FACE);
  f->glEnable(GL_DEPTH_TEST);

  m_program1.bind();
  m_program1.setUniformValue(m_textureLocation1, 0);
  paintContent();
  m_program1.release();

  f->glDisable(GL_DEPTH_TEST);
  f->glDisable(GL_CULL_FACE);

  m_fAngle += 1.0f;
}

void LogoRenderer::createGeometry()
{
  m_vertices.clear();

  m_vertices << QVector3D( 0,  0,  0);
  m_vertices << QVector3D( 1,  0,  0);
  m_vertices << QVector3D( 1,  1,  0);
  m_vertices << QVector3D( 0,  1,  0);
  m_vertices << QVector3D( 0,  0,  0);
}

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Ruslan Baratov
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Multimedia module.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "VideoFilterRunnable.hpp"

#include <cassert> // assert

#include <graphics/GLExtra.h> // GATHERER_OPENGL_DEBUG

#include "VideoFilter.hpp"
#include "TextureBuffer.hpp"

#include "libyuv.h"

#include "OGLESGPGPUTest.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

struct QVideoFrameScopeMap
{
    QVideoFrameScopeMap(QVideoFrame *frame, QAbstractVideoBuffer::MapMode mode) : frame(frame)
    {
        status = frame->map(mode);
        if (!status)
        {
            qWarning("Can't map!");
        }
    }
    ~QVideoFrameScopeMap()
    {
        frame->unmap();
    }
    operator bool() const { return status; }
    QVideoFrame *frame = nullptr;
    bool status = false;
};

static cv::Mat QVideoFrameToCV(QVideoFrame *input);

VideoFilterRunnable::VideoFilterRunnable(VideoFilter *filter) :
    m_filter(filter),
    m_tempTexture(0),
    m_outTexture(0),
    m_lastInputTexture(0) {
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

  const char *vendor = (const char *) f->glGetString(GL_VENDOR);
  qDebug("GL_VENDOR: %s", vendor);
}

VideoFilterRunnable::~VideoFilterRunnable() {
  releaseTextures();
}

QVideoFrame VideoFilterRunnable::run(
    QVideoFrame *input, const QVideoSurfaceFormat &surfaceFormat, RunFlags flags
)
{
  Q_UNUSED(surfaceFormat);
  Q_UNUSED(flags);
    
    float resolution = 1.0f;
    void* glContext = 0;
#if GATHERER_IOS
    glContext = ogles_gpgpu::Core::getCurrentEAGLContext();
    resolution = 2.0f;
#else
    glContext = QOpenGLContext::currentContext();
#endif
    if(!m_pipeline)
    {
        GLint backingWidth, backingHeight;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
        cv::Size screenSize(backingWidth, backingHeight);
        
        m_pipeline = std::make_shared<gatherer::graphics::OEGLGPGPUTest>(glContext, screenSize, resolution);
    }

  // This example supports RGB data only, either in system memory (typical with
  // cameras on all platforms) or as an OpenGL texture (e.g. video playback on
  // OS X).  The latter is the fast path where everything happens on GPU. The
  // former involves a texture upload.

  if (!isFrameValid(*input)) {
    qWarning("Invalid input format");
    return *input;
  }

  if (isFrameFormatYUV(*input)) {
    qWarning("YUV data is not supported");
    return *input;
  }

  if (m_size != input->size()) {
    releaseTextures();
    m_size = input->size();
  }

  // Image objects cannot be read and written at the same time. So use
  // a separate texture for the result.
  // TODO: do something with texture
  //if (!m_outTexture)
  //    m_outTexture = newTexture();
  m_outTexture = createTextureForFrame(input);

  // Accessing dynamic properties on the filter element is simple:
  qreal factor = m_filter->factor();
   return TextureBuffer::createVideoFrame(m_outTexture, m_size);
}

void VideoFilterRunnable::releaseTextures() {
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  if (m_tempTexture) {
    f->glDeleteTextures(1, &m_tempTexture);
  }
  if (m_outTexture) {
    f->glDeleteTextures(1, &m_outTexture);
  }
  m_tempTexture = m_outTexture = m_lastInputTexture = 0;
}

uint VideoFilterRunnable::newTexture() {
  QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
  GLuint texture;
  f->glGenTextures(1, &texture);
  f->glBindTexture(GL_TEXTURE_2D, texture);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // http://stackoverflow.com/a/9617429/2288008
  f->glTexImage2D(
      GL_TEXTURE_2D, // target
      0, // level
      GL_RGBA, // internalformat
      m_size.width(),
      m_size.height(),
      0, // border
      TextureBuffer::openglTextureFormat(), // format
      GL_UNSIGNED_BYTE, // type
      0 // data
  );
  GATHERER_OPENGL_DEBUG;
  assert(texture != 0);
  return texture;
}

bool VideoFilterRunnable::isFrameValid(const QVideoFrame& frame) {
  if (!frame.isValid()) {
    return false;
  }
  if (frame.handleType() == QAbstractVideoBuffer::NoHandle) {
    return true;
  }
  if (frame.handleType() == QAbstractVideoBuffer::GLTextureHandle) {
    return true;
  }

  return false;
}

bool VideoFilterRunnable::isFrameFormatYUV(const QVideoFrame& frame) {
  if (frame.pixelFormat() == QVideoFrame::Format_YUV420P) {
    return true;
  }
  if (frame.pixelFormat() == QVideoFrame::Format_YV12) {
    return true;
  }
  return false;
}

// Create a texture from the image data.
GLuint VideoFilterRunnable::createTextureForFrame(QVideoFrame* input) {
  QOpenGLContext* openglContext = QOpenGLContext::currentContext();
  if (!openglContext) {
    qWarning("Can't get context!");
    return 0;
  }
  assert(openglContext->isValid());

  QOpenGLFunctions *f = openglContext->functions();
  assert(f != 0);

  // Already an OpenGL texture.
  if (input->handleType() == QAbstractVideoBuffer::GLTextureHandle) {
    assert(input->pixelFormat() == TextureBuffer::qtTextureFormat());
    GLuint texture = input->handle().toUInt(); 
    assert(texture != 0);
    f->glBindTexture(GL_TEXTURE_2D, texture);
    m_lastInputTexture = texture;

    const cv::Size size(input->width(), input->height());
    void* pixelBuffer = nullptr; //  we are using texture
    const bool useRawPixels = false; //  - // -
    m_pipeline->captureOutput(size, pixelBuffer, useRawPixels, texture);

    glActiveTexture(GL_TEXTURE0);
    GLuint outputTexture = m_pipeline->getLastShaderOutputTexture();
    f->glBindTexture(GL_TEXTURE_2D, outputTexture);

    return outputTexture;
  }
    
#if GATHERER_IOS
    {
        QVideoFrameScopeMap scopeMap(input, QAbstractVideoBuffer::ReadOnly);
        if(scopeMap)
        {
            cv::Mat frame = QVideoFrameToCV(input);
            m_pipeline->captureOutput(frame.size(), frame.ptr(), true);
        
            // QT is expecting GL_TEXTURE0 to be active
            glActiveTexture(GL_TEXTURE0);
            GLuint texture = m_pipeline->getLastShaderOutputTexture();
            //GLuint texture = m_pipeline->getDisplayTexture();
            f->glBindTexture(GL_TEXTURE_2D, texture);
            return texture;
        }
    }
#else
    
    assert(input->handleType() == QAbstractVideoBuffer::NoHandle);
    
    glActiveTexture(GL_TEXTURE0);
    
  // Upload.
  if (m_tempTexture) {
    f->glBindTexture(GL_TEXTURE_2D, m_tempTexture);
      GATHERER_OPENGL_DEBUG;
  }
  else {
    m_tempTexture = newTexture();
  }
 
  // Convert NV12 TO BGRA format:
  // TODO: Handle other formats

  QVideoFrameScopeMap scopeMap(input, QAbstractVideoBuffer::ReadOnly);
  cv::Mat frame = QVideoFrameToCV(input);

  // glTexImage2D only once and use TexSubImage later on. This avoids the need
  // to recreate the CL image object on every frame.
  f->glTexSubImage2D(
     GL_TEXTURE_2D, // target
     0, // level
     0, // xoffset
     0, // yoffset
     m_size.width(),
     m_size.height(),
     TextureBuffer::openglTextureFormat(), // format
     GL_UNSIGNED_BYTE, // type
     frame.ptr<uint8_t>()); // pixels

  GATHERER_OPENGL_DEBUG;
  return m_tempTexture;
#endif
}

static cv::Mat QVideoFrameToCV(QVideoFrame *input)
{
    cv::Mat frame;
    switch(input->pixelFormat())
    {
        case QVideoFrame::Format_ARGB32:
        case QVideoFrame::Format_BGRA32:
            frame = cv::Mat(input->height(), input->width(), CV_8UC4, input->bits());
            break;
        case QVideoFrame::Format_NV21:
            frame.create(input->height(), input->width(), CV_8UC4);
            libyuv::NV21ToARGB(input->bits(),
                               input->bytesPerLine(),
                               input->bits(1),
                               input->bytesPerLine(1),
                               frame.ptr<uint8_t>(),
                               int(frame.step1()),
                               frame.cols,
                               frame.rows);
            break;
        case QVideoFrame::Format_NV12:
            frame.create(input->height(), input->width(), CV_8UC4);
            libyuv::NV12ToARGB(input->bits(),
                               input->bytesPerLine(),
                               input->bits(1),
                               input->bytesPerLine(1),
                               frame.ptr<uint8_t>(),
                               int(frame.step1()),
                               frame.cols,
                               frame.rows);
            break;
        default: CV_Assert(false);
    }
    return frame;
}
//
//  GLTexture.h
//  gatherer
//
//  Created by David Hirvonen on 12/11/12.
//  Copyright (c) 2012 David Hirvonen. All rights reserved.
//

#ifndef gatherer_GLTexture_h
#define gatherer_GLTexture_h

#include "graphics/gatherer_graphics.h"
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

_GATHERER_GRAPHICS_BEGIN


/**
 * \class GLTexture
 *
 * \brief Simple C++ wrapper to create a new OpenGL texture
 *
 * This class creates and manages OpenGL texture memory.
 *
 * @code
 *
 * cv::Mat image(100, 100, CV_8UC3);
 * GLTexture texture(image);
 *
 * @endcode
 */

// TODO: comments

class GLTexture
{
public:

    /// Constructor (empty)
    GLTexture() { init(); }

    /// Constructor from OpenCV cv::Mat
    GLTexture(const cv::Mat &image) { init(); load(image); }

    /// Initialization
    void init()
    {
        glGenTextures(1,&m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        //std::cout << "make texture: " << int(glGetError()) << std::endl;
    }
    virtual ~GLTexture() { glDeleteTextures((GLsizei)1, (GLuint *)&m_texture); }
    virtual operator unsigned int() const { return m_texture; }
    unsigned int & get() { return m_texture; }
    void load( const cv::Mat &image )
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, m_texture);
#if defined(GATHERER_OPENGL_ES)
#if __ANDROID__
        GLenum format = GL_RGBA;
#else
        GLenum format = GL_BGRA;
#endif
        cv::Mat image_;
        if(image.channels() == 3)
        {
            cv::cvtColor(image, image_, cv::COLOR_BGR2BGRA);
        }
        else
        {
            image_ = image;
        }
#else
        GLenum format = GL_BGR;
        cv::Mat image_ = image;
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_.cols, image_.rows, 0, format, GL_UNSIGNED_BYTE, image_.ptr());
        
        //std::cout << "glTexImage2D: " << int(glGetError()) << std::endl;
        
        glFlush();
    }

protected:

    /// OpenGL texture ID
    unsigned int m_texture;
};


// Source: dhirvonen@elucideye.com
// drishti/lib/graphics/graphics/MosaicRenderGL.cpp: GLTexRect
class GLTexRect : public cv::Rect
{
public:
    GLTexRect(const cv::Rect &roi) : cv::Rect(roi) {}
    GLTexRect(const cv::Size &size) : cv::Rect({0,0}, size) {}
    GLTexRect(int x, int y, int w, int h) : cv::Rect(x, y, w, h) { }
    const cv::Point tr() { return cv::Point(br().x, tl().y); };
    const cv::Point bl() { return cv::Point(tl().x, br().y); };
    std::vector<cv::Point2f> GetVertices() { return std::vector<cv::Point2f>{ tl(), tr(), bl(), br() }; }
    std::vector<cv::Point2f> GetTextureCoordinates(const cv::Rect &roi)
    {
        GLTexRect roi_(roi);
        std::vector<cv::Point2f> coords{ roi_.tl(), roi_.tr(), roi_.bl(), roi_.br() };
        for(auto &p : coords)
        {
            p.x *= (1.0 / width);
            p.y *= (1.0 / height);
        }
        return coords;
    }

    static std::vector<float> GetTextureCoordinates() { return std::vector<float> { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f }; }
};



_GATHERER_GRAPHICS_END

#endif

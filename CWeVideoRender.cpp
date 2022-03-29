#include "CWeVideoRender.h"
#include <QTimer>
#include <QPaintEngine>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>
#include <QDebug>
#include <QOpenGLPixelTransferOptions>
#include "libyuv.h"

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

CSoftRgbRender::CSoftRgbRender(QWidget *parent):QWidget(parent)
{
    m_RenderImage = NULL;
    m_RenderData = NULL;

    connect(this, SIGNAL(NotifyUIMsg(int)), this, SLOT(on_notify_ui(int)));

    //鼠标穿透
    this->setAttribute(Qt::WA_TranslucentBackground, true);
    this->setAttribute(Qt::WA_TransparentForMouseEvents,true);
}

CSoftRgbRender::~CSoftRgbRender()
{
    m_mutex.lock();
    if (m_RenderImage)
    {
        delete m_RenderImage;
        m_RenderImage = NULL;
    }

    if (m_RenderData)
    {
        free(m_RenderData);
        m_RenderData = NULL;
    }
    m_mutex.unlock();
}

int CSoftRgbRender::PlayOneFrame(unsigned char* RgbData, int LineSize, int Width, int Height)
{
    m_mutex.lock();
    if (m_RenderImage)
    {
        if ((Width != m_RenderImage->width()) || (Height != m_RenderImage->height())|| (m_RenderImage->bytesPerLine() != LineSize))
        {
            delete m_RenderImage;
            m_RenderImage = NULL;

            if (m_RenderData)
            {
                free(m_RenderData);
                m_RenderData = NULL;
            }
        }
    }

    if (m_RenderImage == NULL)
    {
        if (m_RenderData)
        {
            free(m_RenderData);
            m_RenderData = NULL;
        }

        m_RenderData = (unsigned char*)malloc(LineSize * Height);
        if (m_RenderData)
        {
            m_RenderImage = new QImage(m_RenderData, Width, Height, LineSize, QImage::Format_RGB32);
        }
    }

    if (m_RenderImage)
    {
        memcpy(m_RenderImage->bits(), RgbData, LineSize * Height);
    }
    m_mutex.unlock();

    emit NotifyUIMsg(0);

    return 0;
}

void CSoftRgbRender::ResetDisplay()
{
    m_mutex.lock();
    if (m_RenderImage)
    {
        delete m_RenderImage;
        m_RenderImage = NULL;

        if (m_RenderData)
        {
            free(m_RenderData);
            m_RenderData = NULL;
        }
    }
    m_mutex.unlock();

    emit NotifyUIMsg(0);
}

void CSoftRgbRender::on_notify_ui(int MsgCode)
{
    if (MsgCode == 0)
        update();
}

void CSoftRgbRender::paintEvent(QPaintEvent* Evt)
{
    QPainter paint(this);
    QRect rc = this->rect();
    paint.setPen(QColor(0, 0, 0, 0));
    paint.setBrush(QBrush(QColor(0, 0, 0, 0)));

    m_mutex.lock();
    if (m_RenderImage == NULL)
    {
        //paint.drawRect(0, 0, rc.width(), rc.height());
        paint.fillRect(rc, Qt::black);
    }
    else
    {
        int picWidth = m_RenderImage->width();
        int picHeight = m_RenderImage->height();
        int width = this->width();
        int height = this->height();

        if ((picWidth > 0) && (picHeight > 0))
        {
            int PaintHeight = (width * picHeight)/picWidth;

            if (PaintHeight > height)
            {
                int PaintWidth = (height * picWidth)/picHeight;
                //左右黑边
                paint.fillRect(0, 0, (width - PaintWidth)/2, height, Qt::black);
                paint.fillRect(width - (width - PaintWidth)/2, 0, (width - PaintWidth)/2, height, Qt::black);
                paint.drawImage(QRect((width - PaintWidth)/2, 0, PaintWidth, height), *m_RenderImage, m_RenderImage->rect());

            }
            else
            {
                //上下黑边
                paint.fillRect(0, 0, width, (height - PaintHeight)/2, Qt::black);
                paint.fillRect(0, height - (height - PaintHeight)/2, width, (height - PaintHeight)/2, Qt::black);
                paint.drawImage(QRect(0, (height - PaintHeight)/2, width, PaintHeight), *m_RenderImage, m_RenderImage->rect());
            }
        }
    }
    m_mutex.unlock();
    return;
}

CGLI420Render::CGLI420Render(QWidget *parent) :QOpenGLWidget(parent)
{
   m_pBufYuv420p = NULL;
   m_pVSHader = NULL;
   m_pFSHader = NULL;
   m_pShaderProgram = NULL;
   m_pTextureY = NULL;
   m_pTextureU = NULL;
   m_pTextureV = NULL;
   m_nVideoH = 0;
   m_nVideoW = 0;
   m_pBufYuv420p_Len = 0;

   //鼠标穿透
    //this->setAttribute(Qt::WA_TranslucentBackground, true);
    this->setAttribute(Qt::WA_TransparentForMouseEvents,true);
   connect(this, SIGNAL(NotifyUIMsg(int)), this, SLOT(on_notify_ui(int)));
}

void CGLI420Render::on_notify_ui(int MsgCode)
{
   update();
}

CGLI420Render::~CGLI420Render()
{
    if (m_pBufYuv420p)
    {
        delete m_pBufYuv420p;
        m_pBufYuv420p = NULL;
        m_pBufYuv420p_Len = 0;
    }

    if (m_pShaderProgram)
    {
        delete m_pShaderProgram;
        m_pShaderProgram = NULL;
    }

    if (m_pFSHader)
    {
        delete m_pFSHader;
        m_pFSHader = NULL;
    }

}
void CGLI420Render::ResetDisplay()
{
    m_mutex.lock();

    DestoryTuxtureYuv();

    m_nVideoW = 0;
    m_nVideoH = 0;

    if (m_pBufYuv420p)
    {
       delete m_pBufYuv420p;
       m_pBufYuv420p = NULL;
       m_pBufYuv420p_Len = 0;
    }


    m_mutex.unlock();

    emit NotifyUIMsg(0);
}

void CGLI420Render::PlayOneFrame(unsigned char* YData, int YPitch, unsigned char* UData, int UPitch, unsigned char* VData, int VPitch, int Width, int Height)
{
   if ((YData == NULL) || (UData == NULL) || (VData == NULL) || (YPitch == 0)|| (UPitch == 0)|| (VPitch == 0) || (Width == 0)|| (Height== 0))
   {
      return;
   }

   if ((m_pBufYuv420p_Len != Width*Height * 3 / 2) || (m_pBufYuv420p == NULL))
   {
      m_mutex.lock();

      if ((m_pBufYuv420p_Len != Width*Height * 3 / 2) || (m_pBufYuv420p == NULL))
      {
         if (m_pBufYuv420p)
         {
            delete m_pBufYuv420p;
            m_pBufYuv420p = NULL;
            m_pBufYuv420p_Len = 0;
         }

         m_pBufYuv420p_Len = Width*Height * 3 / 2;
         m_pBufYuv420p = new unsigned char[m_pBufYuv420p_Len];
         m_nVideoW = Width;
         m_nVideoH = Height;
      }

      m_mutex.unlock();
   }

   if (m_pBufYuv420p)
   {
      libyuv::I420Copy(YData, YPitch, \
                       UData, UPitch,\
                       VData, VPitch,\
                       m_pBufYuv420p, m_nVideoW,\
                       m_pBufYuv420p + m_nVideoW*m_nVideoH, m_nVideoW/2,\
                       m_pBufYuv420p + (m_nVideoW*m_nVideoH*5)/4, m_nVideoW/2,\
                       m_nVideoW, m_nVideoH);
   }

  emit NotifyUIMsg(0);
}
void CGLI420Render::initializeGL()
{
   initializeOpenGLFunctions();

   GLfloat vbo_textureVertices[] = {
      -1.0, -1.0, 0.5,   0.0f,  1.0f,
       1.0, -1.0, 0.5,   1.0f,  1.0f,
       1.0,  1.0, 0.5,   1.0f,  0.0f,
      -1.0,  1.0, 0.5,   0.0f,  0.0f,
   };
   memcpy(m_vertexVertices, vbo_textureVertices, sizeof(vbo_textureVertices));

   vbo.create();
   vbo.bind();
   vbo.allocate(vbo_textureVertices, sizeof(vbo_textureVertices));

   glEnable(GL_DEPTH_TEST);
   glEnable(GL_CULL_FACE);

   //现代opengl渲染管线依赖着色器来处理传入的数据
   //着色器：就是使用openGL着色语言(OpenGL Shading Language, GLSL)编写的一个小函数,
   //       GLSL是构成所有OpenGL着色器的语言,具体的GLSL语言的语法需要读者查找相关资料
   //初始化顶点着色器 对象
   m_pVSHader = new QOpenGLShader(QOpenGLShader::Vertex, this);
   //顶点着色器源码

   const char *vsrc = "attribute vec4 vertexIn; \
    attribute vec2 textureIn; \
    varying vec2 textureOut;  \
    void main(void)           \
    {                         \
        gl_Position = vertexIn; \
        textureOut = textureIn; \
    }";

   //编译顶点着色器程序
   bool bCompile = m_pVSHader->compileSourceCode(vsrc);
   if (!bCompile)
   {
       qDebug() << "--------compileSourceCode error-----------";
   }

   //https://www.cnblogs.com/George1994/p/6418013.html 这个页面指出使用的是2.0 opengl
   //
   //初始化片段着色器 功能gpu中yuv转换成rgb
   //        //gl_FragColor = vec4(rgb, 1);
   m_pFSHader = new QOpenGLShader(QOpenGLShader::Fragment, this);

   const char *fsrc = "varying vec2 textureOut; \
   uniform sampler2D tex_y; \
    uniform sampler2D tex_u; \
    uniform sampler2D tex_v; \
    void main(void) \
    { \
        vec3 yuv; \
        vec3 rgb; \
        yuv.x = texture2D(tex_y, textureOut).r; \
        yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
        yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
        rgb = mat3( 1,       1,         1, \
                    0,       -0.39465,  2.03211, \
                    1.13983, -0.58060,  0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
    }";

  //将glsl源码送入编译器编译着色器程序
   bCompile = m_pFSHader->compileSourceCode(fsrc);
   if (!bCompile)
   {
      //https://stackoverflow.com/questions/28540290/why-it-is-necessary-to-set-precision-for-the-fragment-shader
      //https://stackoverflow.com/questions/45877877/angle-use-in-qt-5-9-1
      const char *fsrc_new = "precision mediump float; varying vec2 textureOut; \
      uniform sampler2D tex_y; \
       uniform sampler2D tex_u; \
       uniform sampler2D tex_v; \
       void main(void) \
       { \
           vec3 yuv; \
           vec3 rgb; \
           yuv.x = texture2D(tex_y, textureOut).r; \
           yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
           yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
           rgb = mat3( 1,       1,         1, \
                       0,       -0.39465,  2.03211, \
                       1.13983, -0.58060,  0) * yuv; \
           gl_FragColor = vec4(rgb, 1); \
       }";
      //将glsl源码送入编译器编译着色器程序
      bCompile = m_pFSHader->compileSourceCode(fsrc_new);


      qDebug() << "--------compileSourceCode error11-----------";
   }

#define PROGRAM_VERTEX_ATTRIBUTE 0
#define PROGRAM_TEXCOORD_ATTRIBUTE 1
   //创建着色器程序容器
   m_pShaderProgram = new QOpenGLShaderProgram;
   //将片段着色器添加到程序容器
   m_pShaderProgram->addShader(m_pFSHader);
   //将顶点着色器添加到程序容器
   m_pShaderProgram->addShader(m_pVSHader);
   //绑定属性vertexIn到指定位置ATTRIB_VERTEX,该属性在顶点着色源码其中有声明
   m_pShaderProgram->bindAttributeLocation("vertexIn", PROGRAM_VERTEX_ATTRIBUTE);
   //绑定属性textureIn到指定位置ATTRIB_TEXTURE,该属性在顶点着色源码其中有声明
   m_pShaderProgram->bindAttributeLocation("textureIn", PROGRAM_TEXCOORD_ATTRIBUTE);
   //链接所有所有添入到的着色器程序
   m_pShaderProgram->link();
   //激活所有链接
   m_pShaderProgram->bind();

   m_pShaderProgram->setUniformValue("tex_y", 0);
   m_pShaderProgram->setUniformValue("tex_u", 1);
   m_pShaderProgram->setUniformValue("tex_v", 2);

   m_pTextureY = NULL;
   m_pTextureU = NULL;
   m_pTextureV = NULL;

}
void CGLI420Render::resizeGL(int w, int h)
{
   if (h == 0)// 防止被零除
   {
      h = 1;// 将高设为1
   }

   if ((m_nVideoH>0)&&(m_nVideoW>0))
   {
       int width = w;
       int height = h;
       int PaintHeight = (width * m_nVideoH)/m_nVideoW;
       if (PaintHeight > height)
       {
           //左右黑边
           GLfloat xValue;
           int PaintWidth = (height * m_nVideoW)/m_nVideoH;

           xValue = (GLfloat)PaintWidth / (GLfloat)width;

           m_vertexVertices[0] = -xValue;
           m_vertexVertices[1] = -1.0;

           m_vertexVertices[5] = xValue;
           m_vertexVertices[6] = -1.0;

           m_vertexVertices[10] = xValue;
           m_vertexVertices[11] = 1.0;

           m_vertexVertices[15] = -xValue;
           m_vertexVertices[16] = 1.0;
       }
       else
       {
           //上下黑边
           GLfloat xValue;

           xValue = (GLfloat)PaintHeight / (GLfloat)height;

           m_vertexVertices[0] = -1.0;
           m_vertexVertices[1] = -xValue;

           m_vertexVertices[5] = 1.0;
           m_vertexVertices[6] = -xValue;

           m_vertexVertices[10] = 1.0;
           m_vertexVertices[11] = xValue;

           m_vertexVertices[15] = -1.0;
           m_vertexVertices[16] = xValue;
       }

       vbo.write(0, m_vertexVertices, sizeof(m_vertexVertices));
   }


   //设置视口
   glViewport(0, 0, w, h);
}

void CGLI420Render::CreateTuxtureYuv(int Width, int Height)
{
    m_pTextureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_pTextureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    m_pTextureV = new QOpenGLTexture(QOpenGLTexture::Target2D);

    m_pTextureY->setFormat(QOpenGLTexture::R8_UNorm);
    m_pTextureU->setFormat(QOpenGLTexture::R8_UNorm);
    m_pTextureV->setFormat(QOpenGLTexture::R8_UNorm);

    m_pTextureY->setSize(Width, Height);
    m_pTextureU->setSize(Width/2, Height/2);
    m_pTextureV->setSize(Width/2, Height/2);

    m_pTextureY->setMipLevels(m_pTextureY->maximumMipLevels());
    m_pTextureU->setMipLevels(m_pTextureU->maximumMipLevels());
    m_pTextureV->setMipLevels(m_pTextureV->maximumMipLevels());

    m_pTextureY->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    m_pTextureU->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    m_pTextureV->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);

    int winSizeX = this->width();
    int winSizeY = this->height();
    int PaintHeight = (winSizeX * Height)/Width;
    if (PaintHeight > winSizeY)
    {
        //左右黑边
        GLfloat xValue;
        int PaintWidth = (winSizeY * Width)/Height;

        xValue = (GLfloat)PaintWidth / (GLfloat)winSizeX;

        m_vertexVertices[0] = -xValue;
        m_vertexVertices[1] = -1.0;

        m_vertexVertices[5] = xValue;
        m_vertexVertices[6] = -1.0;

        m_vertexVertices[10] = xValue;
        m_vertexVertices[11] = 1.0;

        m_vertexVertices[15] = -xValue;
        m_vertexVertices[16] = 1.0;
    }
    else
    {
        //上下黑边
        GLfloat xValue;

        xValue = (GLfloat)PaintHeight / (GLfloat)winSizeY;

        m_vertexVertices[0] = -1.0;
        m_vertexVertices[1] = -xValue;

        m_vertexVertices[5] = 1.0;
        m_vertexVertices[6] = -xValue;

        m_vertexVertices[10] = 1.0;
        m_vertexVertices[11] = xValue;

        m_vertexVertices[15] = -1.0;
        m_vertexVertices[16] = xValue;
    }

    vbo.write(0, m_vertexVertices, sizeof(m_vertexVertices));
}

void CGLI420Render::DestoryTuxtureYuv()
{
    if (m_pTextureY)
    {
        m_pTextureY->destroy();
        delete m_pTextureY;
        m_pTextureY = NULL;
    }
    if (m_pTextureU)
    {
        m_pTextureU->destroy();
        delete m_pTextureU;
        m_pTextureU = NULL;
    }

    if (m_pTextureV)
    {
        m_pTextureV->destroy();
        delete m_pTextureV;
        m_pTextureV = NULL;
    }
}

void CGLI420Render::paintGL()
{
    if ((m_nVideoW > 0) && (m_nVideoH > 0))
    {
        m_pShaderProgram->enableAttributeArray(PROGRAM_VERTEX_ATTRIBUTE);
        m_pShaderProgram->enableAttributeArray(PROGRAM_TEXCOORD_ATTRIBUTE);

        m_pShaderProgram->setAttributeBuffer(PROGRAM_VERTEX_ATTRIBUTE, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
        m_pShaderProgram->setAttributeBuffer(PROGRAM_TEXCOORD_ATTRIBUTE, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));

        m_mutex.lock();

        if (m_pTextureY == NULL)
        {
            //分别创建y,u,v纹理对象
            CreateTuxtureYuv(m_nVideoW, m_nVideoH);
        }

        if (m_pTextureY && ((m_pTextureY->width() != m_nVideoW)||(m_pTextureY->height() != m_nVideoH)))
        {
            DestoryTuxtureYuv();
            CreateTuxtureYuv(m_nVideoW, m_nVideoH);
        }

        if (m_pTextureY && m_pTextureU && m_pTextureV)
        {
            QOpenGLPixelTransferOptions option;
            option.setAlignment(1);

            m_pTextureY->setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, m_pBufYuv420p, &option);
            m_pTextureU->setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, m_pBufYuv420p + m_nVideoW*m_nVideoH, &option);
            m_pTextureV->setData(0, QOpenGLTexture::Red, QOpenGLTexture::UInt8, m_pBufYuv420p+ (m_nVideoW*m_nVideoH * 5) / 4, &option);

            m_pTextureY->bind(0);
            m_pTextureU->bind(1);
            m_pTextureV->bind(2);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        else
        {
            DestoryTuxtureYuv();
        }

        m_mutex.unlock();
    }
    else
    {
        m_vertexVertices[0] = -1.0;
        m_vertexVertices[1] = -1.0;

        m_vertexVertices[5] = 1.0;
        m_vertexVertices[6] = -1.0;

        m_vertexVertices[10] = 1.0;
        m_vertexVertices[11] = 1.0;

        m_vertexVertices[15] = -1.0;
        m_vertexVertices[16] = 1.0;

        glClearColor(0.0, 0.0, 0.0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

   return;
}

CGLRGBARender::CGLRGBARender(QWidget *parent):QOpenGLWidget(parent)
{
    m_Image = NULL;
    m_RgbBuf = NULL;

    connect(this, SIGNAL(NotifyUIMsg(int)), this, SLOT(on_notify_ui(int)));

}
CGLRGBARender::~CGLRGBARender()
{
    m_mutex.lock();
    ClearBuf();
    m_mutex.unlock();
}

void CGLRGBARender::PlayOneFrame(unsigned char* RgbData, int LineSize, int Width, int Height)
{
    m_mutex.lock();
    if (m_Image == NULL)
    {
        NewBuf(RgbData, Width, Height, LineSize);
    }
    else
    {
        if ((m_Image->width() != Width) || (m_Image->height() != Height)|| (m_Image->bytesPerLine() != LineSize))
        {
            ClearBuf();

            if (m_Image == NULL)
            {
                NewBuf(RgbData, Width, Height, LineSize);
            }
        }
    }

    if (m_Image)
    {
        libyuv::ARGBCopy(RgbData, LineSize,\
                         (uint8*)m_Image->bits(),Width*4,\
                         Width, Height);

        //memcpy(m_Image->bits(), RgbData, LineSize * Height);
    }

    m_mutex.unlock();

    emit NotifyUIMsg(0);
}
void CGLRGBARender::ResetDisplay()
{
    m_mutex.lock();
    ClearBuf();
    m_mutex.unlock();
    emit NotifyUIMsg(0);
}
void CGLRGBARender::paintEvent(QPaintEvent *event)
{
    m_mutex.lock();
    if (m_Image)
    {
        int picWidth = m_Image->width();
        int picHeight = m_Image->height();
        int width = this->width();
        int height = this->height();

        int PaintHeight = (width * picHeight)/picWidth;
        QPainter painter;
        painter.begin(this);

        if (PaintHeight > height)
        {
            int PaintWidth = (height * picWidth)/picHeight;
            //左右黑边
            painter.fillRect(0, 0, (width - PaintWidth)/2, height, Qt::black);
            painter.fillRect(width - (width - PaintWidth)/2, 0, (width - PaintWidth)/2, height, Qt::black);
            painter.drawImage(QRect((width - PaintWidth)/2, 0, PaintWidth, height), *m_Image, m_Image->rect());

        }
        else
        {
            //上下黑边
            painter.fillRect(0, 0, width, (height - PaintHeight)/2, Qt::black);
            painter.fillRect(0, height - (height - PaintHeight)/2, width, (height - PaintHeight)/2, Qt::black);
            painter.drawImage(QRect(0, (height - PaintHeight)/2, width, PaintHeight), *m_Image, m_Image->rect());
        }

        painter.end();
    }
    else
    {
        QPainter painter;
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(0, 0, this->width(), this->height(),QBrush(Qt::black));
        painter.end();
    }
    m_mutex.unlock();
}
int CGLRGBARender::ClearBuf()
{
    if (m_Image)
    {
        delete m_Image;
        m_Image = NULL;
    }

    if (m_RgbBuf)
    {
        free(m_RgbBuf);
        m_RgbBuf = NULL;
    }

    return 0;
}
int CGLRGBARender::NewBuf(unsigned char* data, int width, int height, int linesize)
{
    ClearBuf();

    m_RgbBuf = (uchar *)malloc(linesize * height);
    if (m_RgbBuf)
    {
        m_Image = new QImage(m_RgbBuf, width, height, linesize, QImage::Format_RGBA8888);
    }

    if (m_Image == NULL)
    {
        ClearBuf();
    }

    return 0;
}
void CGLRGBARender::on_notify_ui(int MsgCode)
{
    if (MsgCode == 0)
    {
        update();
    }
}

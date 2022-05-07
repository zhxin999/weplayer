#ifndef QVIDEORENDER_H
#define QVIDEORENDER_H

#include <QWidget>
#include <qpushbutton.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPaintEngine>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <qmutex.h>
#include <QLabel>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram);
QT_FORWARD_DECLARE_CLASS(QOpenGLTexture)

class CSoftRgbRender : public QWidget
{
    Q_OBJECT

public:
    CSoftRgbRender(QWidget *parent = 0);
    ~CSoftRgbRender();

    int PlayOneFrame(unsigned char* RgbData, int LineSize, int Width, int Height);
    void ResetDisplay();
    void SetKeepAspect(bool bKeep);
protected:
    virtual void paintEvent(QPaintEvent*);
signals:
   void NotifyUIMsg(int MsgCode);

private slots:
   void on_notify_ui(int MsgCode);

private:
    QImage *m_RenderImage;
    unsigned char* m_RenderData;
    QMutex m_mutex;
    bool m_KeepAspect;
};


class CGLI420Render :public QOpenGLWidget, protected QOpenGLFunctions
{
   Q_OBJECT
public:
   CGLI420Render(QWidget* parent);
   ~CGLI420Render();

  void PlayOneFrame(unsigned char* YData, int YPitch,unsigned char* UData, int UPitch, unsigned char* VData, int VPitch, int Width, int Height);
  void ResetDisplay();
  void SetKeepAspect(bool bKeep);

private:
   void CreateTuxtureYuv(int Width, int Height);
   void DestoryTuxtureYuv();

signals:
   void NotifyUIMsg(int MsgCode);

private slots:
   void on_notify_ui(int MsgCode);

protected:
   void initializeGL() Q_DECL_OVERRIDE;
   void resizeGL(int w, int h) Q_DECL_OVERRIDE;
   void paintGL() Q_DECL_OVERRIDE;

private:
   QOpenGLTexture* m_pTextureY;  //y纹理对象
   QOpenGLTexture* m_pTextureU;  //u纹理对象
   QOpenGLTexture* m_pTextureV;  //v纹理对象
   QOpenGLShader *m_pVSHader;  //顶点着色器程序对象
   QOpenGLShader *m_pFSHader;  //片段着色器对象
   QOpenGLShaderProgram *m_pShaderProgram; //着色器程序容器
   int m_nVideoW; //视频分辨率宽
   int m_nVideoH; //视频分辨率高
   unsigned char* m_pBufYuv420p;
   int  m_pBufYuv420p_Len;

   QMutex m_mutex;

   GLfloat m_vertexVertices[20];

   QOpenGLBuffer vbo;

   bool m_KeepAspect;
};

class CGLRGBARender : public QOpenGLWidget
{
    Q_OBJECT

public:
    CGLRGBARender(QWidget *parent);
    ~CGLRGBARender();

    void PlayOneFrame(unsigned char* RgbData, int LineSize, int Width, int Height);
    void ResetDisplay();
    void SetKeepAspect(bool bKeep);

private:
    int ClearBuf();
    int NewBuf(unsigned char* data, int width, int height, int linesize);

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
   void NotifyUIMsg(int MsgCode);

private slots:
   void on_notify_ui(int MsgCode);

private:
    QImage *m_Image;

    uchar* m_RgbBuf;
    QMutex m_mutex;
    bool m_KeepAspect;
};

#endif // QVIDEORENDER_H

﻿#include "quickosgrenderer.h"
#include "quickosgviewer.h"
#include <QtDebug>
#include <QQuickWindow>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/LineWidth>
#include <osgViewer/GraphicsWindow>
#include <osgDB/ReadFile>
#include <osgGA/TrackballManipulator>
#include <QtMath>
#include<osgShadow/SoftShadowMap>
#include <osg/LineWidth>
#include <osgText/Text>
#include <osg/BoundingSphere>
#include <osg/AutoTransform>
#include <osgViewer/Viewer>
#include "viewerdatum.h"

QuickOSGRenderer::QuickOSGRenderer(QuickOSGViewer* viewer)
    : m_quickViewer(viewer)
{
    m_eventQueue = viewer->m_eventQueue;
    m_operationQueue = viewer->m_operationQueue;

    initOSG();
    viewer->m_camera = m_osgViewer->getCamera();
    viewer->m_viewer = m_osgViewer.get();
}

QuickOSGRenderer::~QuickOSGRenderer()
{
    qDebug()<<"Renderer destroyed.";
}


void QuickOSGRenderer::initOSG()
{
    m_osgViewer = new osgViewer::Viewer();
    auto manipulator = new osgGA::TrackballManipulator;
    manipulator->setAllowThrow(false);
    m_osgViewer->setCameraManipulator(manipulator);
    osg::ref_ptr<osg::Camera> camera = m_osgViewer->getCamera();
    camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    //camera->setCullingMode(osg::CullSettings::NO_CULLING);
    camera->setProjectionMatrixAsOrtho(0.0, 1.0, 0.0, 1.0, 0.1, 10000);
    camera->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);


    m_osgViewer->setUpViewerAsEmbeddedInWindow(0, 0, 300, 300);
    m_quickViewer->resetWidth();  // 触发geometryChanged，改变渲染窗口大小
    m_quickViewer->setCamera(camera);

    //camera->setClearColor(QColor2OSGColor(m_clearColor));
    camera->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    camera->setClearDepth(1);

    m_graphicsWindow = dynamic_cast<osgViewer::GraphicsWindow*>(m_osgViewer->getCamera()->getGraphicsContext());
    //auto gc = m_osgViewer->getCamera()->getGraphicsContext();

    osg::State* state = m_graphicsWindow->getState();
    state->setUseModelViewAndProjectionUniforms(true);
    //state->setUseVertexAttributeAliasing(true);

    m_graphicsWindow->setEventQueue(m_eventQueue);
    m_osgViewer->setUpdateOperations(m_operationQueue);

    m_rootNode = new osg::Group;
    m_osgViewer->setSceneData(m_rootNode.get());

    auto viewerDatum = new ViewerDatum(2, camera.get());
    viewerDatum->setViewport(30.0, 0.0, 170.0, 170.0);
    viewerDatum->setProjectionMatrixAsPerspective(45, 1.0, 0.001, 100000);
    viewerDatum->setComputeNearFarMode(osg::CullSettings::COMPUTE_NEAR_FAR_USING_BOUNDING_VOLUMES);
    viewerDatum->setViewMatrixAsLookAt(osg::Vec3d(0, 0, 20), osg::Vec3d(0, 0, 0), osg::Vec3d(0, 1, 0));
    viewerDatum->setRenderOrder(osg::Camera::POST_RENDER, 1000);
    viewerDatum->setAllowEventFocus(false);
    viewerDatum->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    viewerDatum->getOrCreateStateSet()->setMode(GL_LIGHTING,
                                                  osg::StateAttribute::OFF);
    viewerDatum->getOrCreateStateSet()->setMode(GL_BLEND,
                                                  osg::StateAttribute::ON);
    viewerDatum->setClearMask(GL_DEPTH_BUFFER_BIT);
    m_rootNode->addChild(viewerDatum);

    osg::ref_ptr<osg::Camera> leftCamera = new osg::Camera;
}

// 此处不做互斥同步处理是因为Qt保证在调用该sync函数时GUI线程是阻塞的
// 看Qt源代码也可以发现，在进入该函数之前，renderer线程使用mutex进行了互斥操作
void QuickOSGRenderer::synchronize(QQuickFramebufferObject* item)
{
    m_quickViewer = qobject_cast<QuickOSGViewer*>(item);

    if(m_sceneNode != m_quickViewer->scene()){
        if(m_sceneNode.valid())
            m_rootNode->replaceChild(m_sceneNode.get(), m_quickViewer->scene());
        else
            m_rootNode->addChild(m_quickViewer->scene());
        m_sceneNode = m_quickViewer->scene();
    }
}

void QuickOSGRenderer::render()
{
    QOpenGLContext::currentContext()->functions()->glUseProgram(0);

    m_osgViewer->frame();

    m_quickViewer->window()->resetOpenGLState();
}

QOpenGLFramebufferObject* QuickOSGRenderer::createFramebufferObject(const QSize& size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    auto fbo = new QOpenGLFramebufferObject(size, format);
    m_quickViewer->setDefaultFbo(fbo);
    return fbo;
}

/***************************************************************************
     testqgs3dutils.cpp
     ----------------------
    Date                 : November 2017
    Copyright            : (C) 2017 by Martin Dobias
    Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"

#include "qgs3d.h"
#include "qgs3dutils.h"

#include "qgsbox3d.h"
#include "qgsray3d.h"

#include "qgs3dexportobject.h"
#include "qgs3dmapscene.h"
#include "qgscameracontroller.h"
#include "qgsflatterrainsettings.h"
#include "qgsoffscreen3dengine.h"
#include "qgspolygon3dsymbol.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"
#include "qgsvectorlayer3drenderer.h"

#include <QSize>
#include <QtMath>

static bool qgsVectorNear( const QVector3D &v1, const QVector3D &v2, double eps )
{
  return qgsDoubleNear( v1.x(), v2.x(), eps ) && qgsDoubleNear( v1.y(), v2.y(), eps ) && qgsDoubleNear( v1.z(), v2.z(), eps );
}

static bool qgsQuaternionNear( const QQuaternion &q1, const QQuaternion &q2, double eps )
{
  return qgsDoubleNear( q1.x(), q2.x(), eps ) && qgsDoubleNear( q1.y(), q2.y(), eps ) && qgsDoubleNear( q1.z(), q2.z(), eps ) && qgsDoubleNear( q1.scalar(), q2.scalar(), eps );
}

static bool isPointInFrontOfPlane( const QgsVector3D &point, const QgsVector3D &planePoint, const QgsVector3D &planeNormal )
{
  return QgsVector3D::dotProduct( point - planePoint, planeNormal ) > 0;
}

static bool isPointOutsideClippingPlanes( const QgsVector3D &point, const QVector<QgsVector3D> &planePoints, const QList<QVector4D> &clippingPlanes )
{
  for ( int i = 0; i < clippingPlanes.size(); i++ )
  {
    if ( !isPointInFrontOfPlane( point, planePoints.at( i ), clippingPlanes.at( i ).toVector3D() ) )
    {
      return true;
    };
  }
  return false;
}

/**
 * \ingroup UnitTests
 * This is a unit test for the vertex tool
 */
class TestQgs3DUtils : public QgsTest
{
    Q_OBJECT
  public:
    TestQgs3DUtils()
      : QgsTest( QStringLiteral( "3D Utils" ), QStringLiteral( "3d" ) ) {}

  private slots:
    void initTestCase();    // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.

    void testTransforms();
    void testRayFromScreenPoint();
    void testQgsBox3DDistanceTo();
    void testQgsRay3D();
    void testExportToObj();
    void testDefinesToShaderCode();
    void testDecomposeTransformMatrix();
    void testScreenPointToMapCoordinates();
    void testLineSegmentToClippingPlanes();
    void testLineSegmentToCameraPose();
    void test3DSceneRay3D();

  private:
    QgsRasterLayer *mLayerRgb;
    QgsVectorLayer *mLayerBuildings;
};

//runs before all tests
void TestQgs3DUtils::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  Qgs3D::initialize();

  mLayerRgb = new QgsRasterLayer( testDataPath( "/3d/rgb.tif" ), "rgb", "gdal" );
  QVERIFY( mLayerRgb->isValid() );

  mLayerBuildings = new QgsVectorLayer( testDataPath( "/3d/buildings.shp" ), "buildings", "ogr" );
  QVERIFY( mLayerBuildings->isValid() );
}

//runs after all tests
void TestQgs3DUtils::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgs3DUtils::testTransforms()
{
  const QgsVector3D map123( 1, 2, 3 );

  const QgsVector3D world123 = Qgs3DUtils::mapToWorldCoordinates( map123, QgsVector3D() );
  QCOMPARE( world123, QgsVector3D( 1, 2, 3 ) );

  const QgsVector3D world123map = Qgs3DUtils::worldToMapCoordinates( world123, QgsVector3D() );
  QCOMPARE( world123map, map123 );

  // now with non-zero origin

  const QgsVector3D origin( -10, -20, -30 );

  const QgsVector3D world123x = Qgs3DUtils::mapToWorldCoordinates( map123, origin );
  QCOMPARE( world123x, QgsVector3D( 11, 22, 33 ) );

  const QgsVector3D world123xmap = Qgs3DUtils::worldToMapCoordinates( world123x, origin );
  QCOMPARE( world123xmap, map123 );

  //
  // transform world point from one system to another
  //

  const QgsVector3D worldPoint1( 5, 6, 7 );
  const QgsVector3D origin1( 10, 20, 30 );
  const QgsVector3D origin2( 1, 2, 3 );
  const QgsVector3D worldPoint2 = Qgs3DUtils::transformWorldCoordinates( worldPoint1, origin1, QgsCoordinateReferenceSystem(), origin2, QgsCoordinateReferenceSystem(), QgsCoordinateTransformContext() );
  QCOMPARE( worldPoint2, QgsVector3D( 14, 24, 34 ) );
  // verify that both are the same map point
  const QgsVector3D mapPoint1 = Qgs3DUtils::worldToMapCoordinates( worldPoint1, origin1 );
  const QgsVector3D mapPoint2 = Qgs3DUtils::worldToMapCoordinates( worldPoint2, origin2 );
  QCOMPARE( mapPoint1, mapPoint2 );
}

void TestQgs3DUtils::testRayFromScreenPoint()
{
  Qt3DRender::QCamera camera;
  {
    camera.setFieldOfView( 45.0f );
    camera.setNearPlane( 10.0f );
    camera.setFarPlane( 100.0f );
    camera.setAspectRatio( 2 );
    camera.setPosition( QVector3D( 9.0f, 15.0f, 30.0f ) );
    camera.setUpVector( QVector3D( 0.0f, 1.0f, 0.0f ) );
    camera.setViewCenter( QVector3D( 0.0f, 0.0f, 0.0f ) );

    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 50, 50 ), QSize( 100, 100 ), &camera );
      const QgsRay3D ray2( QVector3D( 8.99999904632568f, 14.9999980926514f, 29.9999980926514f ), QVector3D( -0.25916051864624f, -0.431934207677841f, -0.863868415355682f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 0, 0 ), QSize( 100, 100 ), &camera );
      const QgsRay3D ray2( QVector3D( 8.99999904632568f, 14.9999980926514f, 29.9999980926514f ), QVector3D( -0.810001313686371f, -0.0428109727799892f, -0.584863305091858f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 100, 100 ), QSize( 100, 100 ), &camera );
      const QgsRay3D ray2( QVector3D( 8.99999904632568f, 14.9999980926514f, 29.9999980926514f ), QVector3D( 0.429731547832489f, -0.590972006320953f, -0.682702660560608f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
  }

  {
    camera.setFieldOfView( 60.0f );
    camera.setNearPlane( 1.0f );
    camera.setFarPlane( 1000.0f );
    camera.setAspectRatio( 2 );
    camera.setPosition( QVector3D( 0.0f, 0.0f, 0.0f ) );
    camera.setUpVector( QVector3D( 0.0f, 1.0f, 0.0f ) );
    camera.setViewCenter( QVector3D( 0.0f, 100.0f, -100.0f ) );

    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 500, 500 ), QSize( 1000, 1000 ), &camera );
      const QgsRay3D ray2( QVector3D( 0, 0, 0 ), QVector3D( 0, 0.70710676908493f, -0.70710676908493f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 0, 0 ), QSize( 1000, 1000 ), &camera );
      const QgsRay3D ray2( QVector3D( 0, 0, 0 ), QVector3D( -0.70710676908493f, 0.683012664318085f, -0.183012709021568f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
    {
      const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( QPoint( 500, 1000 ), QSize( 1000, 1000 ), &camera );
      const QgsRay3D ray2( QVector3D( 0, 0, 0 ), QVector3D( 0, 0.258819073438644f, -0.965925812721252f ) );
      QGSCOMPARENEARVECTOR3D( ray1.origin(), ray2.origin(), 0.00001 );
      QGSCOMPARENEARVECTOR3D( ray1.direction(), ray2.direction(), 0.00001 );
    }
  }
}

void TestQgs3DUtils::testQgsBox3DDistanceTo()
{
  {
    const QgsBox3D box( -1, -1, -1, 1, 1, 1 );
    QCOMPARE( box.distanceTo( QgsVector3D( 0, 0, 0 ) ), 0.0 );
    QCOMPARE( box.distanceTo( QgsVector3D( 2, 2, 2 ) ), qSqrt( 3.0 ) );
  }
  {
    const QgsBox3D box( 1, 2, 1, 4, 3, 3 );
    QCOMPARE( box.distanceTo( QgsVector3D( 1, 2, 1 ) ), 0.0 );
    QCOMPARE( box.distanceTo( QgsVector3D( 0, 0, 0 ) ), qSqrt( 6.0 ) );
  }
}

void TestQgs3DUtils::testQgsRay3D()
{
  {
    const QgsRay3D ray( QVector3D( 0, 0, 0 ), QVector3D( 1, 1, 1 ) );
    const QVector3D p1( 0.5f, 0.5f, 0.5f );
    const QVector3D p2( -0.5f, -0.5f, -0.5f );
    // points are already on the ray
    const QVector3D projP1 = ray.projectedPoint( p1 );
    const QVector3D projP2 = ray.projectedPoint( p2 );

    QCOMPARE( p1.x(), projP1.x() );
    QCOMPARE( p1.y(), projP1.y() );
    QCOMPARE( p1.z(), projP1.z() );

    QCOMPARE( p2.x(), projP2.x() );
    QCOMPARE( p2.y(), projP2.y() );
    QCOMPARE( p2.z(), projP2.z() );

    QVERIFY( qFuzzyIsNull( ( float ) ray.angleToPoint( p1 ) ) );
    QVERIFY( qFuzzyIsNull( ( float ) ray.angleToPoint( p2 ) ) );

    QVERIFY( ray.isInFront( p1 ) );
    QVERIFY( !ray.isInFront( p2 ) );
  }

  {
    const QgsRay3D ray( QVector3D( 0, 0, 0 ), QVector3D( 1, 1, 1 ) );
    const QVector3D p1( 0, 1, 1 );
    const QVector3D p2( 0, -1, -1 );
    const QVector3D expectedProjP1( 0.666667f, 0.666667f, 0.666667f );
    const QVector3D expectedProjP2( -0.666667f, -0.666667f, -0.666667f );

    const QVector3D projP1 = ray.projectedPoint( p1 );
    QCOMPARE( projP1.x(), expectedProjP1.x() );
    QCOMPARE( projP1.y(), expectedProjP1.y() );
    QCOMPARE( projP1.z(), expectedProjP1.z() );

    const QVector3D projP2 = ray.projectedPoint( p2 );
    QCOMPARE( projP2.x(), expectedProjP2.x() );
    QCOMPARE( projP2.y(), expectedProjP2.y() );
    QCOMPARE( projP2.z(), expectedProjP2.z() );

    QVERIFY( ray.isInFront( p1 ) );
    QVERIFY( !ray.isInFront( p2 ) );
  }
}

void TestQgs3DUtils::testExportToObj()
{
  // all vertice positions
  QVector<float> positionData = {
    -0.456616, 0.00187836, -0.413774,
    -0.4718, 0.00187836, -0.0764642,
    -0.25705, 0.00187836, -0.230477,
    -0.25705, 0.00187836, -0.230477,
    -0.4718, 0.00187836, -0.0764642,
    0.0184382, 0.00187836, 0.177332,
    -0.25705, 0.00187836, -0.230477,
    0.0184382, 0.00187836, 0.177332,
    -0.25705, -0.00187836, -0.230477,
    -0.25705, -0.00187836, -0.230477,
    0.0184382, 0.00187836, 0.177332,
    0.0184382, -0.00187836, 0.177332,
    0.0184382, 0.00187836, 0.177332,
    -0.4718, 0.00187836, -0.0764642,
    0.0184382, -0.00187836, 0.177332,
    0.0184382, -0.00187836, 0.177332,
    -0.4718, 0.00187836, -0.0764642,
    -0.4718, -0.00187836, -0.0764642,
    -0.4718, 0.00187836, -0.0764642,
    -0.456616, 0.00187836, -0.413774,
    -0.4718, -0.00187836, -0.0764642,
    -0.4718, -0.00187836, -0.0764642,
    -0.456616, 0.00187836, -0.413774,
    -0.456616, -0.00187836, -0.413774,
    -0.456616, 0.00187836, -0.413774,
    -0.25705, 0.00187836, -0.230477,
    -0.456616, -0.00187836, -0.413774,
    -0.456616, -0.00187836, -0.413774,
    -0.25705, 0.00187836, -0.230477,
    -0.25705, -0.00187836, -0.230477
  };

  // all vertice normals
  QVector<float> normalsData = {
    0, 1, 0,
    0, 1, 0,
    0, 1, 0,
    0, 1, 0,
    0, 1, 0,
    0, 1, 0,
    0.828644, 0, -0.559776,
    0.828644, 0, -0.559776,
    0.828644, 0, -0.559776,
    0.828644, 0, -0.559776,
    0.828644, 0, -0.559776,
    0.828644, 0, -0.559776,
    -0.459744, 0, 0.888052,
    -0.459744, 0, 0.888052,
    -0.459744, 0, 0.888052,
    -0.459744, 0, 0.888052,
    -0.459744, 0, 0.888052,
    -0.459744, 0, 0.888052,
    -0.998988, 0, -0.0449705,
    -0.998988, 0, -0.0449705,
    -0.998988, 0, -0.0449705,
    -0.998988, 0, -0.0449705,
    -0.998988, 0, -0.0449705,
    -0.998988, 0, -0.0449705,
    0.676449, 0, -0.736489,
    0.676449, 0, -0.736489,
    0.676449, 0, -0.736489,
    0.676449, 0, -0.736489,
    0.676449, 0, -0.736489,
    0.676449, 0, -0.736489
  };

  const QString myTmpDir = QDir::tempPath() + '/';

  // case where all vertices are used
  {
    Qgs3DExportObject object( "all_faces" );
    object.setupPositionCoordinates( positionData, QMatrix4x4() );
    QCOMPARE( object.vertexPosition().size(), positionData.size() );

    // exported vertice indexes
    QVector<uint> indexData = {
      0,
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15,
      16,
      17,
      18,
      19,
      20,
      21,
      22,
      23,
      24,
      25,
      26,
      27,
      28,
      29,
    };

    object.setupFaces( indexData );
    QCOMPARE( object.indexes().size(), indexData.size() );

    object.setupNormalCoordinates( normalsData, QMatrix4x4() );
    QCOMPARE( object.normals().size(), normalsData.size() );


    QFile file( myTmpDir + "all_faces.obj" );
    file.open( QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate );
    QTextStream out( &file );

    out << "o " << object.name() << "\n";
    object.saveTo( out, 1.0, QVector3D( 0, 0, 0 ), 3 );

    out.flush();
    out.seek( 0 );

    QString actual = out.readAll();
    QGSCOMPARELONGSTR( "export_obj", "all_faces.obj", actual.toUtf8() );
  }

  // case where only a subset of vertices are used
  {
    // exported vertice indexes
    QVector<uint> indexData = {
      // 0, 1, 2,
      // 3, 4, 5,
      6, 7, 8,
      9, 10, 11,
      12, 13, 14,
      15, 16, 17,
      // 18, 19, 20,
      21, 22, 23,
      // 24, 25, 26,
      // 27, 28, 29,
    };

    Qgs3DExportObject object( "sparse_faces" );
    object.setupPositionCoordinates( positionData, QMatrix4x4() );
    QCOMPARE( object.vertexPosition().size(), positionData.size() );

    object.setupFaces( indexData );
    QCOMPARE( object.indexes().size(), indexData.size() );

    object.setupNormalCoordinates( normalsData, QMatrix4x4() );
    QCOMPARE( object.normals().size(), normalsData.size() );

    QFile file( myTmpDir + "sparse_faces.obj" );
    file.open( QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate );
    QTextStream out( &file );
    out << "o " << object.name() << "\n";
    object.saveTo( out, 1.0, QVector3D( 0, 0, 0 ), 3 );

    out.flush();
    out.seek( 0 );

    QString actual = out.readAll();
    QGSCOMPARELONGSTR( "export_obj", "sparse_faces.obj", actual.toUtf8() );
  }
}

void TestQgs3DUtils::testDefinesToShaderCode()
{
  // load the different files
  const QByteArray shaderCode = Qt3DRender::QShaderProgram::loadSource( QUrl::fromLocalFile( testDataPath( "/3d/shader/sample.frag" ) ) );
  QVERIFY( !shaderCode.isEmpty() );

  const QByteArray shaderCodeWithDefines = Qt3DRender::QShaderProgram::loadSource( QUrl::fromLocalFile( testDataPath( "/3d/shader/sample_with_defines.frag" ) ) );
  QVERIFY( !shaderCodeWithDefines.isEmpty() );

  const QByteArray shaderCodeNoVersion = Qt3DRender::QShaderProgram::loadSource( QUrl::fromLocalFile( testDataPath( "/3d/shader/sample_no_version.frag" ) ) );
  QVERIFY( !shaderCodeNoVersion.isEmpty() );

  const QByteArray shaderCodeNoVersionWithDefines = Qt3DRender::QShaderProgram::loadSource( QUrl::fromLocalFile( testDataPath( "/3d/shader/sample_no_version_with_defines.frag" ) ) );
  QVERIFY( !shaderCodeNoVersionWithDefines.isEmpty() );

  const QByteArray shaderCodeWithBaseColorMap = Qt3DRender::QShaderProgram::loadSource( QUrl::fromLocalFile( testDataPath( "/3d/shader/sample_with_basecolormap.frag" ) ) );
  QVERIFY( !shaderCodeWithBaseColorMap.isEmpty() );

  const QStringList definesList( { "BASE_COLOR_MAP", "ROUGHNESS_MAP" } );


  // =============================================
  // =========== test addDefinesToShaderCode

  // test a shader code
  const QByteArray actualShaderCodeWithDefines = Qgs3DUtils::addDefinesToShaderCode( shaderCode, definesList );
  QCOMPARE( actualShaderCodeWithDefines, shaderCodeWithDefines );

  // shader code without a #version - this should not happen
  // in that case the #define is inserted at the beginning
  const QByteArray actualShaderCodeNoVersionWithDefines = Qgs3DUtils::addDefinesToShaderCode( shaderCodeNoVersion, definesList );
  QCOMPARE( actualShaderCodeNoVersionWithDefines, shaderCodeNoVersionWithDefines );

  // input is empty
  // the result only contains the #define code
  const QByteArray onlyDefines = Qgs3DUtils::addDefinesToShaderCode( QByteArray(), definesList );
  QCOMPARE( onlyDefines, QByteArray( "#define BASE_COLOR_MAP\n#define ROUGHNESS_MAP\n" ) );


  // =============================================
  // =========== test removeDefinesFromShaderCode

  // test a shader code
  const QByteArray actualShaderCodeWithoutDefines = Qgs3DUtils::removeDefinesFromShaderCode( shaderCodeWithDefines, definesList );
  QCOMPARE( actualShaderCodeWithoutDefines, shaderCode );

  // remove defines one by one
  const QByteArray actualShaderCodeWithBaseColorMap = Qgs3DUtils::removeDefinesFromShaderCode( shaderCodeWithDefines, QStringList { "ROUGHNESS_MAP" } );
  QCOMPARE( actualShaderCodeWithBaseColorMap, shaderCodeWithBaseColorMap );

  const QByteArray actualShaderCode = Qgs3DUtils::removeDefinesFromShaderCode( actualShaderCodeWithBaseColorMap, QStringList { "BASE_COLOR_MAP" } );
  QCOMPARE( actualShaderCode, shaderCode );

  // shader code without a #version - this should not happen
  // define macros should be removed
  const QByteArray actualShaderCodeNoVersionWithoutDefines = Qgs3DUtils::removeDefinesFromShaderCode( shaderCodeNoVersionWithDefines, definesList );
  QCOMPARE( actualShaderCodeNoVersionWithoutDefines, shaderCodeNoVersion );

  // input is empty
  // result should be empty
  const QByteArray actualEmpty = Qgs3DUtils::removeDefinesFromShaderCode( QByteArray(), definesList );
  QCOMPARE( actualEmpty, QByteArray() );
}

void TestQgs3DUtils::testDecomposeTransformMatrix()
{
  QMatrix4x4 m1;
  m1.translate( QVector3D( 100, 200, 300 ) );
  m1.scale( QVector3D( 2, 5, 7 ) );
  QVector3D t1, s1;
  QQuaternion r1;
  Qgs3DUtils::decomposeTransformMatrix( m1, t1, r1, s1 );

  QCOMPARE( s1, QVector3D( 2, 5, 7 ) );
  QCOMPARE( t1, QVector3D( 100, 200, 300 ) );
  QCOMPARE( r1, QQuaternion() );

  QMatrix4x4 m2;
  m2.translate( QVector3D( 500, 600, 700 ) );
  QQuaternion q2 = QQuaternion::fromAxisAndAngle( QVector3D( 1, 0, 0 ), 90 );
  m2.rotate( q2 );
  QVector3D t2, s2;
  QQuaternion r2;
  Qgs3DUtils::decomposeTransformMatrix( m2, t2, r2, s2 );

  QVERIFY( qgsVectorNear( s2, QVector3D( 1, 1, 1 ), 1e-6 ) );
  QVERIFY( qgsVectorNear( t2, QVector3D( 500, 600, 700 ), 1e-6 ) );
  QVERIFY( qgsQuaternionNear( r2, q2, 1e-6 ) );
}

void TestQgs3DUtils::testScreenPointToMapCoordinates()
{
  Qgs3DMapSettings map;
  map.setCrs( mLayerRgb->crs() );
  map.setExtent( mLayerRgb->extent() );
  map.setLayers( QList<QgsMapLayer *>() << mLayerRgb );
  QgsOffscreen3DEngine engine;
  const Qgs3DMapScene *scene = new Qgs3DMapScene( map, &engine );
  const QgsPoint mapPoint = Qgs3DUtils::screenPointToMapCoordinates( QPoint( 50, 50 ), QSize( 100, 100 ), scene->cameraController(), &map );

  // this placement is weird, but it fixes the clang-tidy warning
  delete scene;
  QGSCOMPARENEAR( mapPoint.x(), 321900, 2 );
  QGSCOMPARENEAR( mapPoint.y(), 129901, 2 );
  QGSCOMPARENEAR( mapPoint.z(), -252, 2 );
}

void TestQgs3DUtils::testLineSegmentToClippingPlanes()
{
  const QgsVector3D point1( 20, 20, 0 );
  const QgsVector3D point2( 50, 50, 0 );
  const QgsVector3D testPointInside( 35, 35, 0 );
  const QgsVector3D testPointOutside( 0, 0, 0 );
  QVector<QgsVector3D> planePoints( { point1, QgsVector3D( 13, 27, 0 ), point2, QgsVector3D( 27, 13, 0 ) } );

  QList<QVector4D> clippingPlanes = Qgs3DUtils::lineSegmentToClippingPlanes( point1, point2, 10, QgsVector3D( 0, 0, 0 ) );
  QVERIFY( clippingPlanes.size() == 4 );
  QVERIFY( !isPointOutsideClippingPlanes( testPointInside, planePoints, clippingPlanes ) );
  QVERIFY( isPointOutsideClippingPlanes( testPointOutside, planePoints, clippingPlanes ) );

  //verify that it works in reverse order too
  clippingPlanes = Qgs3DUtils::lineSegmentToClippingPlanes( point2, point1, 10, QgsVector3D( 0, 0, 0 ) );
  QVERIFY( clippingPlanes.size() == 4 );
  planePoints = { point2, QgsVector3D( 27, 13, 0 ), point1, QgsVector3D( 13, 27, 0 ) };
  QVERIFY( !isPointOutsideClippingPlanes( testPointInside, planePoints, clippingPlanes ) );
  QVERIFY( isPointOutsideClippingPlanes( testPointOutside, planePoints, clippingPlanes ) );

  // verify that it works for perpendicular line too
  const QgsVector3D point3( 50, 20, 0 );
  const QgsVector3D point4( 20, 50, 0 );
  clippingPlanes = Qgs3DUtils::lineSegmentToClippingPlanes( point3, point4, 10, QgsVector3D( 0, 0, 0 ) );
  QVERIFY( clippingPlanes.size() == 4 );
  planePoints = { point3, QgsVector3D( 43, 13, 0 ), point4, QgsVector3D( 57, 27, 0 ) };
  QVERIFY( !isPointOutsideClippingPlanes( testPointInside, planePoints, clippingPlanes ) );
  QVERIFY( isPointOutsideClippingPlanes( testPointOutside, planePoints, clippingPlanes ) );

  // verify that it works for perpendicular line in reverse order too
  clippingPlanes = Qgs3DUtils::lineSegmentToClippingPlanes( point4, point3, 10, QgsVector3D( 0, 0, 0 ) );
  QVERIFY( clippingPlanes.size() == 4 );
  planePoints = { point4, QgsVector3D( 57, 27, 0 ), point3, QgsVector3D( 43, 13, 0 ) };
  QVERIFY( !isPointOutsideClippingPlanes( testPointInside, planePoints, clippingPlanes ) );
  QVERIFY( isPointOutsideClippingPlanes( testPointOutside, planePoints, clippingPlanes ) );
}

void TestQgs3DUtils::testLineSegmentToCameraPose()
{
  const QgsVector3D origin( 0, 0, 0 );
  const QgsVector3D startPoint( 20, 20, 0 );
  const QgsVector3D endPoint( 82, 82, 0 );
  QgsDoubleRange elevationRange( 0, 20 );
  constexpr float fieldOfView = 90;

  // test 1: the distance between start point and end point is longer than elevation range
  QgsCameraPose camPose = Qgs3DUtils::lineSegmentToCameraPose( startPoint, endPoint, elevationRange, fieldOfView, origin );
  QCOMPARE( camPose.centerPoint(), QgsVector3D( 51, 51, 10 ) );
  QCOMPARE( camPose.pitchAngle(), 90 );
  QCOMPARE( camPose.headingAngle(), 45 );
  QGSCOMPARENEAR( camPose.distanceFromCenterPoint(), 46, 0.2 );

  // test 2: the distance between start point and end point is smaller than elevation range
  elevationRange = QgsDoubleRange( 0, 100 );
  camPose = Qgs3DUtils::lineSegmentToCameraPose( startPoint, endPoint, elevationRange, fieldOfView, origin );
  QCOMPARE( camPose.centerPoint(), QgsVector3D( 51, 51, 50 ) );
  QCOMPARE( camPose.pitchAngle(), 90 );
  QCOMPARE( camPose.headingAngle(), 45 );
  QGSCOMPARENEAR( camPose.distanceFromCenterPoint(), 52.5, 0.2 );
}

void TestQgs3DUtils::test3DSceneRay3D()
{
  // configure map Settings
  Qgs3DMapSettings mapSettings;
  mapSettings.setCrs( mLayerRgb->crs() );
  mapSettings.setExtent( mLayerRgb->extent() );
  mapSettings.setLayers( QList<QgsMapLayer *>() << mLayerBuildings << mLayerRgb );

  QgsFlatTerrainSettings *flatTerrainSettings = new QgsFlatTerrainSettings;
  mapSettings.setTerrainSettings( flatTerrainSettings );

  QgsPhongMaterialSettings materialSettings;
  materialSettings.setAmbient( Qt::lightGray );
  QgsPolygon3DSymbol *symbol3d = new QgsPolygon3DSymbol;
  symbol3d->setMaterialSettings( materialSettings.clone() );
  symbol3d->setExtrusionHeight( 10.f );
  QgsVectorLayer3DRenderer *renderer3d = new QgsVectorLayer3DRenderer( symbol3d );
  mLayerBuildings->setRenderer3D( renderer3d );

  // create the 3d scene
  const QSize winSize( 640, 480 ); // default window size
  QgsOffscreen3DEngine engine;
  engine.setSize( winSize );
  Qgs3DMapScene *scene = new Qgs3DMapScene( mapSettings, &engine );
  engine.setRootEntity( scene );

  scene->cameraController()->setLookingAtPoint( QVector3D( 0, -280, 0 ), 50, 40.0, -10.0 );

  // This is needed to ensure that the nodes of mLayerBuildings are loaded
  Qgs3DUtils::captureSceneImage( engine, scene );

  // click on a building
  // building and terrain are hit
  // the distance to the building should be smaller than the distance to the terrain
  Qt3DRender::QCamera *camera = scene->cameraController()->camera();
  const QPoint clickedPoint1( 115, 374 );
  const QgsRay3D ray1 = Qgs3DUtils::rayFromScreenPoint( clickedPoint1, winSize, camera );
  const QHash<QgsMapLayer *, QVector<QgsRayCastingUtils::RayHit>> allHits1 = Qgs3DUtils::castRay( scene, ray1, QgsRayCastingUtils::RayCastContext( true, winSize, camera->farPlane() ) );
  QCOMPARE( allHits1.size(), 2 );
  QVERIFY( allHits1.contains( mLayerBuildings ) );
  QVERIFY( allHits1.contains( nullptr ) );
  QCOMPARE( allHits1[mLayerBuildings].size(), 1 );
  QCOMPARE( allHits1[nullptr].size(), 1 );
  const float buildingDistance1 = allHits1[mLayerBuildings][0].distance;
  const float terrainDistance1 = allHits1[nullptr][0].distance;
  QGSCOMPARENEAR( buildingDistance1, 33.59, 1.0 );
  QGSCOMPARENEAR( terrainDistance1, 45.46, 1.0 );


  // clicking on the terrain
  // the building layer should not be hit
  const QPoint clickedPoint2( 419, 326 );
  const QgsRay3D ray2 = Qgs3DUtils::rayFromScreenPoint( clickedPoint2, winSize, camera );
  const QHash<QgsMapLayer *, QVector<QgsRayCastingUtils::RayHit>> allHits2 = Qgs3DUtils::castRay( scene, ray2, QgsRayCastingUtils::RayCastContext( true, winSize, camera->farPlane() ) );
  QCOMPARE( allHits2.size(), 1 );
  QVERIFY( !allHits2.contains( mLayerBuildings ) );
  QVERIFY( allHits2.contains( nullptr ) );
  QCOMPARE( allHits2[nullptr].size(), 1 );
  const float terrainDistance2 = allHits2[nullptr][0].distance;
  QGSCOMPARENEAR( terrainDistance2, 45.59, 1.0 );

  delete scene;
}


QGSTEST_MAIN( TestQgs3DUtils )
#include "testqgs3dutils.moc"

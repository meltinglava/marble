//
// This file is part of the Marble Project.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2006-2007 Torsten Rahn <tackat@kde.org>"
// Copyright 2007      Inge Wallin  <ingwa@kde.org>"
//

#include "vectormap.h"

#include <QtGui/QColor>
#include <QtCore/QVector>
#include <QtCore/QTime>
#include <QtCore/QDebug>

#include <cmath>
#include <stdlib.h>

#include "clippainter.h"
#include "GeoPolygon.h"


// #define VECMAP_DEBUG 


VectorMap::VectorMap()
{

    m_zlimit = 0.0f; 
    m_plimit = 0.0f;
    m_zBoundingBoxLimit = 0.0f; 
    m_zPointLimit = 0.0f; 

    imgrx = 0; 
    imgry = 0; 
    imgradius = 0;
    imgwidth  = 0; 
    imgheight = 0;

    // Initialising booleans for horizoncrossing
    horizonpair = false;
    lastvisible = false;
    currentlyvisible = false;
    firsthorizon = false;

    m_radius = 0;
    m_rlimit = 0;

    m_brush = QBrush( QColor( 0, 0, 0 ) );

    //	m_debugNodeCount = 0;
}

VectorMap::~VectorMap()
{
}


void VectorMap::createFromPntMap(const PntMap* pntmap, const int& radius, 
                                 Quaternion& rotAxis)
{
    clear();

    m_radius = radius - 1;

    // zlimit: describes the lowest z value of the sphere that is
    //         visible as an excerpt on the screen
    float zlimit = ( ( imgradius < m_radius * m_radius )
                     ? sqrtf(1 - (float)imgradius / (float)(m_radius * m_radius))
                     : 0.0 );

    // qDebug() << "zlimit: " << zlimit;

    m_zBoundingBoxLimit = ( ( m_zBoundingBoxLimit >= 0
                              && zlimit < m_zBoundingBoxLimit )
                            || m_zBoundingBoxLimit < 0 )
                     ? zlimit : m_zBoundingBoxLimit;
    m_zPointLimit = ( ( m_zPointLimit >= 0 && zlimit < m_zPointLimit )
                      || m_zPointLimit < 0 )
                     ? zlimit : m_zPointLimit;
//    m_zPointLimit = 0;

    m_rlimit = (int)( (float)(m_radius * m_radius)
                      * (1.0 - m_zPointLimit * m_zPointLimit ) );

    // Quaternion qbound = ( FastMath::haveSSE() == true )? QuaternionSSE() : Quaternion();

    Quaternion  qbound;

    rotAxis.inverse().toMatrix( m_rotMatrix );

    GeoPolygon::PtrVector::Iterator       itPolyLine;
    GeoPolygon::PtrVector::ConstIterator  itEndPolyLine = pntmap->constEnd();

    //	const int detail = 0;
    const int  detail = getDetailLevel();
    GeoPoint   corner;

    for ( itPolyLine = const_cast<PntMap *>(pntmap)->begin();
          itPolyLine < itEndPolyLine;
          ++itPolyLine )
    {
        // This sorts out polygons by bounding box which aren't visible at all.
        boundary = (*itPolyLine)->getBoundary();

        for ( int i = 0; i < 5; ++i ) {
            qbound = boundary[i].quaternion();
            qbound.rotateAroundAxis(m_rotMatrix); 

            if ( qbound.v[Q_Z] > m_zBoundingBoxLimit ) {
                // if (qbound.v[Q_Z] > 0){
                m_polygon.clear();
                m_polygon.reserve( (*itPolyLine)->size() );
                m_polygon.setClosed( (*itPolyLine)->getClosed() );

                // qDebug() << i << " Visible: YES";
                createPolyLine( (*itPolyLine)->constBegin(),
                                (*itPolyLine)->constEnd(), detail );

                break; // abort foreach test of current boundary
            } 
            // else
            //     qDebug() << i << " Visible: NOT";
            // ++i;
        }
    }
}


void VectorMap::createPolyLine( GeoPoint::Vector::ConstIterator  itStartPoint, 
                                GeoPoint::Vector::ConstIterator  itEndPoint,
                                const int detail)
{
    GeoPoint::Vector::const_iterator  itPoint;

    // Quaternion qpos = ( FastMath::haveSSE() == true ) ? QuaternionSSE() : Quaternion();
    Quaternion qpos;
    //	int step = 1;
    //	int remain = size();

    for ( itPoint = itStartPoint; itPoint != itEndPoint; ++itPoint ) {
        // remain -= step;
        if ( itPoint->detail() >= detail ) {

            // Calculate polygon nodes
#ifdef VECMAP_DEBUG
            ++m_debugNodeCount;
#endif
            qpos = itPoint->quaternion();
            qpos.rotateAroundAxis(m_rotMatrix);
            currentPoint = QPointF( imgrx + m_radius * qpos.v[Q_X] + 1,
                                    imgry + m_radius * qpos.v[Q_Y] + 1 );
			
            // Take care of horizon crossings if horizon is visible
            lastvisible = currentlyvisible;			

            // Less accurate:
            // currentlyvisible = (qpos.v[Q_Z] >= m_zPointLimit) ? true : false;
            currentlyvisible = ( qpos.v[Q_Z] >= 0 ) ? true : false;
            if ( itPoint == itStartPoint ) {
                initCrossHorizon();
            }
            if ( currentlyvisible != lastvisible )
                manageCrossHorizon();

            // Take care of screencrossing crossings if horizon is visible.
            // Filter Points which aren't on the visible Hemisphere.
            if ( currentlyvisible && currentPoint != lastPoint ) {
                // most recent addition: currentPoint != lastPoint
                m_polygon << currentPoint;
            }
#if 0
            else {
                // Speed burst on invisible hemisphere
                step = 1;
                if ( z < -0.2) step = 10;
                if ( z < -0.4) step = 30;
                if ( step > remain ) step = 1; 
            }
#endif

            lastPoint = currentPoint;
        }
    }

    // In case of horizon crossings, make sure that we always get a
    // polygon closed correctly.
    if ( firsthorizon ) {
        horizonb = firstHorizonPoint;
        if (m_polygon.closed())
            createArc();

        firsthorizon = false;
    }
		
    // Avoid polygons degenerated to Points and Lines.
    if ( m_polygon.size() >= 2 ) {
        append(m_polygon);
    }
}


void VectorMap::drawMap(QPaintDevice * origimg, bool antialiasing)
{

    bool clip = (m_radius > imgrx || m_radius > imgry) ? true : false;

    ClipPainter  painter(origimg, clip);
    //	QPainter painter(origimg);
    if ( antialiasing == true )
        painter.setRenderHint( QPainter::Antialiasing );

    painter.setPen(m_pen);
    painter.setBrush(m_brush);

    ScreenPolygon::Vector::const_iterator  itEndPolygon = end();

    for ( ScreenPolygon::Vector::const_iterator itPolygon = begin();
          itPolygon != itEndPolygon; 
          ++itPolygon )
    {

        if ( itPolygon->closed() == true )  
            painter.drawPolygon( *itPolygon );
        else
            painter.drawPolyline( *itPolygon );
    }

    // painter.drawEllipse(imgrx-m_radius,imgry-m_radius,2*m_radius,2*m_radius+1);
}


void VectorMap::paintMap(ClipPainter * painter, bool antialiasing)
{
    // bool clip = (m_radius > imgrx || m_radius > imgry) ? true : false;

    // ClipPainter painter(origimg, clip);
    // QPainter painter(origimg);
    if ( antialiasing )
        painter->setRenderHint( QPainter::Antialiasing );

    painter->setPen( m_pen );
    painter->setBrush( m_brush );

    ScreenPolygon::Vector::const_iterator  itEndPolygon = end();

    for ( ScreenPolygon::Vector::const_iterator itPolygon = begin();
          itPolygon != itEndPolygon;
          ++itPolygon )
    {
        if ( itPolygon->closed() == true )  
            painter->drawPolygon( *itPolygon );
        else
            painter->drawPolyline( *itPolygon );

    }

    // painter.drawEllipse(imgrx-m_radius,imgry-m_radius,2*m_radius,2*m_radius+1);
}


void VectorMap::initCrossHorizon()
{
    // qDebug("Initializing scheduled new PolyLine");
    lastvisible  = currentlyvisible;
    lastPoint    = QPointF( currentPoint.x() + 1, currentPoint.y() + 1 );
    horizonpair  = false;
    firsthorizon = false;
}


void VectorMap::manageCrossHorizon()
{
    // qDebug("Crossing horizon line");
    // if (currentlyvisible == false) qDebug("Leaving visible hemisphere");
    // else qDebug("Entering visible hemisphere");

    if ( horizonpair == false ) {
        // qDebug("Point A");

        if ( currentlyvisible == false ) {
            horizona    = horizonPoint();
            horizonpair = true;
        }
        else {
            // qDebug("Orphaned");
            firstHorizonPoint = horizonPoint();
            firsthorizon      = true;
        }
    }
    else {
        // qDebug("Point B");
        horizonb = horizonPoint();

        createArc();
        horizonpair = false;
    }
}


const QPointF VectorMap::horizonPoint()
{
    // qDebug("Interpolating");
    float  xa;
    float  ya;

    xa = currentPoint.x() - ( imgrx + 1 );

    // Move the currentPoint along the y-axis to match the horizon.
    //	ya = sqrt( (m_radius +1) * ( m_radius +1) - xa*xa);
    ya = ( m_rlimit > xa * xa )
        ? sqrtf( (float)(m_rlimit) - (float)( xa * xa ) ) : 0;
    // qDebug() << " m_rlimit" << m_rlimit << " xa*xa" << xa*xa << " ya: " << ya;
    if ( ( currentPoint.y() - ( imgry + 1 ) ) < 0 )
        ya = -ya; 

    return QPointF( imgrx + xa + 1, imgry + ya + 1 );
}


void VectorMap::createArc()
{

    m_polygon.append( horizona );

    int  beta  = (int)( 180.0 / M_PI * atan2f( horizonb.y() - imgry - 1,
                                               horizonb.x() - imgrx - 1 ) );
    int  alpha = (int)( 180.0 / M_PI * atan2f( horizona.y() - imgry - 1,
                                               horizona.x() - imgrx - 1 ) );

    int diff = beta - alpha;

    if ( diff != 0 ) {
        int sgndiff = diff / abs(diff);

        if (abs(diff) > 180)
            diff = - sgndiff * (360 - abs(diff));

        // Reassigning sgndiff this way seems dull
        sgndiff = diff / abs(diff);
        // qDebug() << "SGN: " << sgndiff;

        // qDebug () << " beta: " << beta << " alpha " << alpha << " diff: " << diff;
	
        int  itx;
        int  ity;
        // qDebug() << "r: " << (m_radius+1) << "rn: " << sqrt((float)(m_rlimit));
        float  arcradius = sqrtf( (float)( m_rlimit ) );

        for ( int it = 1; it < abs(diff); ++it ) {
            float angle = M_PI/180.0 * (float)( alpha + sgndiff * it );
            itx = (int)( imgrx +  arcradius * cosf( angle ) + 1 );
            ity = (int)( imgry +  arcradius * sinf( angle ) + 1 );
            // qDebug() << " ity: " << ity;
            m_polygon.append( QPoint( itx, ity ) );		
        }
    }

    m_polygon.append( horizonb );
}


void VectorMap::resizeMap(const QPaintDevice * origimg)
{
    imgwidth  = origimg -> width();
    imgheight = origimg -> height();
    // qDebug() << "width:" << imgwidth;
    imgrx = (imgwidth  >> 1);
    imgry = (imgheight >> 1);
    imgradius = imgrx * imgrx + imgry * imgry;
}


int VectorMap::getDetailLevel() const
{
    int detail = 5;
	
    if ( m_radius >   50 ) detail = 4;
    if ( m_radius >  600 ) detail = 3;
    if ( m_radius > 1000 ) detail = 2;
    if ( m_radius > 2500 ) detail = 1;
    if ( m_radius > 5000 ) detail = 0;

    //	qDebug() << "Detail: " << detail << " Radius: " << m_radius ;

    return detail;
}


//
// This file is part of the Marble Virtual Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2009      Andrew Manson <g.real.ate@gmail.com>
//

#include "OsmAnnotatePlugin.h"

#include <QtGui/QFileDialog>

#include <QtGui/QAction>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "MarbleDebug.h"
#include "AbstractProjection.h"
#include "AreaAnnotation.h"
#include "GeoDataDocument.h"
#include "GeoDataLatLonBox.h"
#include "GeoDataParser.h"
#include "GeoDataPlacemark.h"
#include "GeoPainter.h"
#include "GeoWriter.h"
#include "MarbleDirs.h"
#include "MarbleWidget.h"
#include "PlacemarkTextAnnotation.h"

namespace Marble
{

OsmAnnotatePlugin::OsmAnnotatePlugin()
        : RenderPlugin( 0 ),
          m_AnnotationDocument( new GeoDataDocument ),
          m_networkAccessManager( 0 ),
          m_isInitialized( false )
{
}

OsmAnnotatePlugin::OsmAnnotatePlugin(const MarbleModel *model)
        : RenderPlugin(model),
          m_AnnotationDocument( new GeoDataDocument ),
          m_networkAccessManager( 0 ),
          m_isInitialized( false )
{
    Q_UNUSED(model);
}

OsmAnnotatePlugin::~OsmAnnotatePlugin()
{
    delete m_AnnotationDocument;
    delete m_networkAccessManager;
}

QStringList OsmAnnotatePlugin::backendTypes() const
{
    return QStringList( "osmannotation" );
}

QString OsmAnnotatePlugin::renderPolicy() const
{
    return QString( "ALWAYS" );
}

QStringList OsmAnnotatePlugin::renderPosition() const
{
    QStringList positions;
    positions.append( "HOVERS_ABOVE_SURFACE" );
    positions.append( "USER_TOOLS" );
    return positions;
}

QString OsmAnnotatePlugin::name() const
{
    return tr( "OSM Annotation Plugin" );
}

QString OsmAnnotatePlugin::guiString() const
{
    return tr( "&OSM Annotation Plugin" );
}

QString OsmAnnotatePlugin::nameId() const
{
    return QString( "osm-annotation-plugin" );
}

QString OsmAnnotatePlugin::description() const
{
    return tr( "This is a render and interaction plugin used for annotating OSM data." );
}

QString OsmAnnotatePlugin::version() const
{
    return "1.0";
}

QString OsmAnnotatePlugin::copyrightYears() const
{
    return "2009";
}

QList<PluginAuthor> OsmAnnotatePlugin::pluginAuthors() const
{
    return QList<PluginAuthor>()
            << PluginAuthor( "Andrew Manson", "<g.real.ate@gmail.com>" );
}

QIcon OsmAnnotatePlugin::icon () const
{
    return QIcon();
}


void OsmAnnotatePlugin::initialize ()
{
    widgetInitalised= false;
    m_tmp_lineString = 0;
    m_addingPlacemark = false;
    m_drawingPolygon = false;

    m_actions = 0;
    m_toolbarActions = 0;

//    PointScreenGraphicsItem* topLeft = new PointScreenGraphicsItem();
//    PointScreenGraphicsItem* bottomRight = new PointScreenGraphicsItem();

//    topLeft->setVisible(false);
//    bottomRight->setVisible(false);

    m_isInitialized = true;
}

bool OsmAnnotatePlugin::isInitialized () const
{
    return m_isInitialized;
}

QList<QActionGroup*>* OsmAnnotatePlugin::actionGroups() const
{
    return m_actions;
}

QList<QActionGroup*>* OsmAnnotatePlugin::toolbarActionGroups() const
{
    return m_toolbarActions;
}

bool OsmAnnotatePlugin::render( GeoPainter *painter, ViewportParams *viewport, const QString& renderPos, GeoSceneLayer * layer )
{
    if ( renderPos != "HOVERS_ABOVE_SURFACE" && renderPos != "USER_TOOLS" ) {
        return true;
    }
    
    if( !widgetInitalised ) {
        MarbleWidget* marbleWidget = (MarbleWidget*) painter->device();
        m_marbleWidget = marbleWidget;
        setupActions( marbleWidget );

        connect(this, SIGNAL(redraw()),
                marbleWidget, SLOT(update()) );

        widgetInitalised = true;
    }

    if( renderPos == "HOVERS_ABOVE_SURFACE" ) {
        painter->autoMapQuality();

        //so the user can keep track of the current polygon drawing
        if( m_tmp_lineString ) {
            painter->drawPolyline( *m_tmp_lineString );
        }
    }

    QListIterator<TmpGraphicsItem*> i( m_graphicsItems );

    while(i.hasNext()) {
        TmpGraphicsItem* tmp= i.next();
        tmp->paint(painter, viewport);
    }

    return true;
}

void OsmAnnotatePlugin::setAddingPlacemark( bool b)
{
    m_addingPlacemark = b;
}

void OsmAnnotatePlugin::setDrawingPolygon(bool b)
{
    m_drawingPolygon = b;
    if( !b ) {
        //stopped drawing the polygon
        if ( m_tmp_lineString != 0 ) {
            GeoDataPlacemark *placemark = new GeoDataPlacemark;
            GeoDataPolygon *poly = new GeoDataPolygon( Tessellate );
            poly->setOuterBoundary( GeoDataLinearRing(*m_tmp_lineString) );
            placemark->setGeometry( poly );

            delete m_tmp_lineString;
            m_tmp_lineString = 0;

            AreaAnnotation* area = new AreaAnnotation( placemark );

            m_AnnotationDocument->append( placemark );
            m_graphicsItems.append( area );

            //FIXME only redraw the new polygon
            emit(redraw());
        }
    }
}

void OsmAnnotatePlugin::receiveNetworkReply( QNetworkReply *reply )
{
    if( reply->error() == QNetworkReply::NoError ) {
        readOsmFile( reply, false );
    } else {
        m_errorMessage.showMessage( tr("Error while trying to download the "
                                            "OSM file from the server. The "
                                            "error was:\n %1" ).arg(reply->errorString()) );
    }
}

void OsmAnnotatePlugin::loadOsmFile()
{
    QString filename;
    filename = QFileDialog::getOpenFileName(0, tr("Open File"),
                            QString(),
                            tr("All Supported Files (*.osm);;Open Street Map Data (*.osm)"));

    if ( ! filename.isNull() ) {

        QFile file( filename );
        if ( !file.exists() ) {
            qDebug( "File does not exist!" );
            return;
        }

        // Open file in right mode
        file.open( QIODevice::ReadOnly );

        readOsmFile( &file, true );

        file.close();
    }
}

void OsmAnnotatePlugin::downloadOsmFile()
{
    QPointF topLeft(0,0);
    QPointF bottomRight(m_marbleWidget->size().width(), m_marbleWidget->size().height());

    qreal lonTop, latTop;
    qreal lonBottom, latBottom;

    GeoDataCoordinates topLeftCoordinates;
    GeoDataCoordinates bottomRightCoordinates;

    bool topIsOnGlobe = m_marbleWidget->geoCoordinates( topLeft.x(),
                                                        topLeft.y(),
                                                        lonTop, latTop,
                                                        GeoDataCoordinates::Radian);
    bool bottomIsOnGlobe = m_marbleWidget->geoCoordinates( bottomRight.x(),
                                                           bottomRight.y(),
                                                           lonBottom, latBottom,
                                                           GeoDataCoordinates::Radian );

    if( ! ( topIsOnGlobe && bottomIsOnGlobe ) ) {
        m_errorMessage.showMessage( tr("One of the selection points is not on"
                                       " the Globe. Please only select a region"
                                       " on the globe.") );
        return;
    }

    topLeftCoordinates = GeoDataCoordinates( lonTop, latTop, 0,
                                             GeoDataCoordinates::Radian );

    bottomRightCoordinates = GeoDataCoordinates( lonBottom, latBottom, 0,
                                                 GeoDataCoordinates::Radian );

    GeoDataLineString tempString;
    tempString.append( topLeftCoordinates );
    tempString.append( bottomRightCoordinates );

    GeoDataLatLonAltBox bounds = GeoDataLatLonAltBox::fromLineString( tempString );

    QString request;
    request = QString("http://api.openstreetmap.org/api/0.6/map?bbox=%1,%2,%3,%4")
              .arg(bounds.west(GeoDataCoordinates::Degree) )
              .arg(bounds.south(GeoDataCoordinates::Degree) )
              .arg(bounds.east( GeoDataCoordinates::Degree) )
              .arg( bounds.north( GeoDataCoordinates::Degree ) );

    QNetworkRequest networkRequest;
    networkRequest.setUrl(QUrl(request) );

    if( ! m_networkAccessManager ) {
        m_networkAccessManager = new QNetworkAccessManager( this ) ;
        connect( m_networkAccessManager, SIGNAL(finished(QNetworkReply*)),
                 this, SLOT(receiveNetworkReply(QNetworkReply*)) );
    }

    m_networkAccessManager->get( networkRequest );
}

void OsmAnnotatePlugin::saveAnnotationFile()
{
    GeoDataDocument document;

    QList<PlacemarkTextAnnotation*> allAnnotations = annotations();

    PlacemarkTextAnnotation* annotation;
    foreach( annotation, allAnnotations ) {
        GeoDataPlacemark *placemark = new GeoDataPlacemark( *annotation->feature() );
        document.append( placemark );
    }

    QString filename;
    filename = QFileDialog::getSaveFileName( 0, tr("Save Annotation File"),
                            QString(),
                            tr("All Supported Files (*.kml);;KML file (*.kml)"));

    if ( ! filename.isNull() ) {

        GeoWriter writer;
        //FIXME: a better way to do this?
        writer.setDocumentType( "http://earth.google.com/kml/2.2" );

        QFile file( filename );

        // Open file in right mode
        file.open( QIODevice::ReadWrite );

//        if ( !writer.write( &file, document ) ) {
//            qDebug( "Could not write the file." );
//        }
    }
}

void OsmAnnotatePlugin::loadAnnotationFile()
{
    //load the file here
    QString filename;
    filename = QFileDialog::getOpenFileName(0, tr("Open Annotation File"),
                            QString(),
                            tr("All Supported Files (*.kml);;Kml Annotation file (*.kml)"));

    if ( ! filename.isNull() ) {

        GeoDataParser parser( GeoData_KML );

        QFile file( filename );
        if ( !file.exists() ) {
            qDebug( "File does not exist!" );
            return;
        }

        // Open file in right mode
        file.open( QIODevice::ReadOnly );

        if ( !parser.read( &file ) ) {
            qDebug( "Could not parse file!" );
            return;
        }

        GeoDataDocument* document = dynamic_cast<GeoDataDocument*>(parser.releaseDocument() );
        Q_ASSERT( document );

        file.close();

        QVector<GeoDataFeature*>::ConstIterator it = document->constBegin();
        for( ; it < document->constEnd(); ++it ) {
            PlacemarkTextAnnotation* annotation = new PlacemarkTextAnnotation( *it );
            annotation->setName( (*it)->name() );
            annotation->setDescription( (*it)->description() );
            m_graphicsItems.append( annotation );
        }

        delete document;
        emit repaintNeeded(QRegion());
    }
}

bool    OsmAnnotatePlugin::eventFilter(QObject* watched, QEvent* event)
{
    MarbleWidget* marbleWidget = dynamic_cast<MarbleWidget*>( watched );
    Q_ASSERT( marbleWidget );

    //FIXME why is the QEvent::MousePress not working? caught somewhere else?
    //does this mean we need to centralize the event handling?

    //so far only accept mouse events
    if( event->type() != QEvent::MouseButtonPress
        && event->type() != QEvent::MouseButtonRelease
        && event->type() != QEvent::MouseMove )
    {
        return false;
    }

    QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    Q_ASSERT( mouseEvent );

    GeoDataCoordinates coordinates;
    qreal lon, lat;
    bool isOnGlobe = marbleWidget->geoCoordinates( mouseEvent->pos().x(),
                                                   mouseEvent->pos().y(),
                                                   lon, lat,
                                                   GeoDataCoordinates::Radian );
    coordinates = GeoDataCoordinates( lon, lat );
    if( !isOnGlobe ) {
        return false;
    }

    //Pass the event to Graphics Items
    QList<TmpGraphicsItem*>::ConstIterator itemIterator = m_graphicsItems.constBegin();
    for( ; itemIterator < m_graphicsItems.constEnd() ; ++itemIterator ) {
        QListIterator<QRegion> it ( (*itemIterator)->regions() );

        while ( it.hasNext() ) {
            QRegion p = it.next();
            if( p.contains( mouseEvent->pos() ) ) {
                if( (*itemIterator)->sceneEvent( event ) ) {
                    return true;
                }
            }
        }
    }

    //FIXME: finish cleaning this up

    // deal with adding a placemark
    if ( mouseEvent->button() == Qt::LeftButton
         && m_addingPlacemark ) {
        //Add a placemark on the screen
        GeoDataPlacemark *placemark = new GeoDataPlacemark;
        placemark->setCoordinate( coordinates );
        PlacemarkTextAnnotation* t = new PlacemarkTextAnnotation( placemark );
        m_AnnotationDocument->append( placemark );
        m_graphicsItems.append( t );

        //FIXME only repaint the new placemark
        ( ( MarbleWidget* ) watched)->update();
        emit placemarkAdded();
        return true;
    }

    // deal with drawing a polygon
    if ( mouseEvent->button() == Qt::LeftButton
         && m_drawingPolygon ) {
        if ( m_tmp_lineString == 0 ) {
            m_tmp_lineString = new GeoDataLineString( Tessellate );
        }

        m_tmp_lineString->append(GeoDataCoordinates(lon, lat));

        //FIXME only repaint the line string so far
        marbleWidget->update();
        return true;
    }

    return false;
}

void OsmAnnotatePlugin::setupActions(MarbleWidget* widget)
{
    QList<QActionGroup*>* toolbarActions = new QList<QActionGroup*>();
    QList<QActionGroup*>* actions = new QList<QActionGroup*>();

    QActionGroup* initial = new QActionGroup(0);
    initial->setExclusive( false );

    QActionGroup* group = new QActionGroup(0);
    group->setExclusive( true );

    QActionGroup* nonExclusiveGroup = new QActionGroup(0);
    nonExclusiveGroup->setExclusive( false );

    QAction*    addPlacemark;
    QAction*    drawPolygon;
    QAction*    beginSeparator;
    QAction*    endSeparator;
    QAction*    loadOsmFile;
    QAction*    saveAnnotationFile;
    QAction*    loadAnnotationFile;
    QAction*    enableInputAction;
    QAction*    downloadOsm;

    addPlacemark = new QAction(this);
    addPlacemark->setText( tr("Add Placemark") );
    addPlacemark->setCheckable( true );
    connect( addPlacemark, SIGNAL(toggled(bool)),
             this, SLOT(setAddingPlacemark(bool)) );
    connect( this, SIGNAL(placemarkAdded()) ,
             addPlacemark, SLOT(toggle()) );

    drawPolygon = new QAction( this );
    drawPolygon->setText( tr("Draw Polygon") );
    drawPolygon->setCheckable( true );
    connect( drawPolygon, SIGNAL(toggled(bool)),
             this, SLOT(setDrawingPolygon(bool)) );

    loadOsmFile = new QAction( this );
    loadOsmFile->setText( tr("Load Osm File") );
    connect( loadOsmFile, SIGNAL(triggered()),
             this, SLOT(loadOsmFile()) );

    saveAnnotationFile = new QAction( this );
    saveAnnotationFile->setText( tr("Save Annotation File") );
    connect( saveAnnotationFile, SIGNAL(triggered()),
             this, SLOT(saveAnnotationFile()) );

    loadAnnotationFile = new QAction( this );
    loadAnnotationFile->setText( tr("Load Annotation File" ) );
    connect( loadAnnotationFile, SIGNAL(triggered()),
             this, SLOT(loadAnnotationFile()) );

    beginSeparator = new QAction( this );
    beginSeparator->setSeparator( true );
    endSeparator = new QAction ( this );
    endSeparator->setSeparator( true );

    enableInputAction = new QAction(this);
    enableInputAction->setToolTip(tr("Enable Marble Input"));
    enableInputAction->setCheckable(true);
    enableInputAction->setChecked( true );
    enableInputAction->setIcon( QIcon( MarbleDirs::path("bitmaps/hand.png") ) );
    connect( enableInputAction, SIGNAL(toggled(bool)),
                       widget, SLOT(setInputEnabled(bool)) );

    downloadOsm = new QAction( this );
    downloadOsm->setText( tr("Download Osm File") );
    downloadOsm->setToolTip(tr("Download Osm File for selected area"));
    connect( downloadOsm, SIGNAL(triggered()),
             this, SLOT(downloadOsmFile()) );


    initial->addAction( enableInputAction );
    initial->addAction( beginSeparator );

    group->addAction( addPlacemark );
    group->addAction( drawPolygon );
    group->addAction( loadOsmFile );
    group->addAction( saveAnnotationFile );
    group->addAction( loadAnnotationFile );
    group->addAction( endSeparator );

    nonExclusiveGroup->addAction( downloadOsm );

    actions->append( initial );
    actions->append( group );
    actions->append( nonExclusiveGroup );

    toolbarActions->append( initial );
    toolbarActions->append( group );
    toolbarActions->append( nonExclusiveGroup );

    //delete the old groups if they exist
    delete m_actions;
    delete m_toolbarActions;

    m_actions = actions;
    m_toolbarActions = toolbarActions;

    emit actionGroupsChanged();
}

void OsmAnnotatePlugin::readOsmFile( QIODevice *device, bool flyToFile )
{
}

QList<PlacemarkTextAnnotation*> OsmAnnotatePlugin::annotations() const
{
    QList<PlacemarkTextAnnotation*> tmpAnnotations;
    TmpGraphicsItem* item;
    foreach( item, m_graphicsItems ) {
        PlacemarkTextAnnotation* annotation = dynamic_cast<PlacemarkTextAnnotation*>(item);
        if( annotation ) {
            tmpAnnotations.append( annotation );
        }
    }

    return tmpAnnotations;
}

}

Q_EXPORT_PLUGIN2( OsmAnnotatePlugin, Marble::OsmAnnotatePlugin )

#include "OsmAnnotatePlugin.moc"

/*
 This file is part of the Marble Virtual Globe.

 This program is free software licensed under the GNU LGPL. You can
 find a copy of this license in LICENSE.txt in the top directory of
 the source code.

 Copyright 2008      Patrick Spendrin  <ps_ml@gmx.de>
 Copyright 2010      Thibaut Gridel  <tgridel@free.fr>
 Copyright 2012      Ander Pijoan <ander.pijoan@deusto.es>
*/

#ifndef MARBLE_VECTORTILELAYER_H
#define MARBLE_VECTORTILELAYER_H

#include "LayerInterface.h"
#include <QtCore/QObject>

#include "MarbleGlobal.h"
#include "MarbleModel.h"
#include "GeoDataDocument.h"
#include "GeoDataLatLonAltBox.h"
#include "GeoSceneTiled.h"

#include <QtCore/QSize>

class QImage;
class QRegion;
class QRect;

namespace Marble
{

class GeoPainter;
class GeoSceneGroup;
class HttpDownloadManager;
class SunLocator;
class ViewportParams;

class VectorTileLayer : public QObject, public LayerInterface
{
    Q_OBJECT

 public:
    VectorTileLayer( HttpDownloadManager *downloadManager,
                  const SunLocator *sunLocator,
                  const PluginManager *pluginManager,
                  GeoDataTreeModel *treeModel);

    ~VectorTileLayer();

    QStringList renderPosition() const;

 public Q_SLOTS:
    bool render( GeoPainter *painter, ViewportParams *viewport,
                 const QString &renderPos = "NONE", GeoSceneLayer *layer = 0 );

    /**
     * @brief  Set the Projection used for the map
     * @param  projection projection type (e.g. Spherical, Equirectangular, Mercator)
     */
    void setupTextureMapper();

    void setMapTheme( const QVector<const GeoSceneTiled *> &textures, GeoSceneGroup *textureLayerSettings );

    void reset();

    void updateTile(TileId const & tileId, GeoDataDocument *document, QString const & format );

 Q_SIGNALS:
    void tileLevelChanged( int );
    void repaintNeeded();

 private:
    Q_PRIVATE_SLOT( d, void updateTextureLayers() )


 private:
    class Private;
    Private *const d;

};

}

#endif // MARBLE_VECTORTILELAYER_H

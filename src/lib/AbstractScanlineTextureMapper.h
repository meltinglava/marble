//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2007      Andrew Manson    <g.real.ate@gmail.com>
//


#ifndef ABSTRACTSCANLINETEXTUREMAPPER_H
#define ABSTRACTSCANLINETEXTUREMAPPER_H

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <cmath>
#include <math.h>

#include "MarbleMath.h"
#include "TileLoader.h"
#include "TextureTile.h"
#include "GeoSceneTexture.h"
#include "MathHelper.h"


namespace Marble
{

class TextureTile;
class TileLoader;
class ViewParams;

class AbstractScanlineTextureMapper : public QObject
{
    Q_OBJECT

public:
    explicit AbstractScanlineTextureMapper( TileLoader *tileLoader, QObject * parent=0 );
    ~AbstractScanlineTextureMapper();

    virtual void mapTexture( ViewParams *viewParams ) = 0;

    void setLayer( GeoSceneLayer * layer );
    void setMaxTileLevel( int level );
    virtual void resizeMap( int width, int height );
    void selectTileLevel( ViewParams* viewParams );
    bool interlaced() const;
    void setInterlaced( bool enabled );

    void centerTiles( ViewParams *viewParams, int tileLevel,
                      qreal& tileCol, qreal& tileRow );

 Q_SIGNALS:
    void mapChanged();

 private Q_SLOTS:
    void notifyMapChanged();

 protected:
    void pixelValue( qreal lon, qreal lat, 
                     QRgb* scanLine, bool smooth = false );

    // method for fast integer calculation
    void nextTile( int& posx, int& posy );

    // method for precise interpolation
    void nextTile( qreal& posx, qreal& posy );

    void detectMaxTileLevel();
    void tileLevelInit( int tileLevel );

    int globalWidth() const;
    int globalHeight() const;

    // Converts Radian to global texture coordinates 
    // ( with origin in center, measured in pixel) 
    qreal rad2PixelX( const qreal longitude ) const;
    qreal rad2PixelY( const qreal latitude ) const;

    // Coordinates on the tile for fast integer calculation
    int        m_iPosX;
    int        m_iPosY;
    // Coordinates on the tile for precise interpolation
    qreal     m_posX;
    qreal     m_posY;

    // maximum values for global texture coordinates
    // ( with origin in upper left corner, measured in pixel) 
    int     m_maxGlobalX;
    int     m_maxGlobalY;

    int     m_imageHeight;
    int     m_imageWidth;
    int     m_imageRadius;

    // Previous coordinates
    qreal  m_prevLat;
    qreal  m_prevLon;

    // Coordinate transformations:

    // Converts global texture coordinates 
    // ( with origin in center, measured in pixel) 
    // to tile coordinates ( measured in pixel )
    qreal  m_toTileCoordinatesLon;
    qreal  m_toTileCoordinatesLat;

    bool m_interlaced;

    // ------------------------
    // Tile stuff
    TileLoader  *m_tileLoader;
    GeoSceneTexture::Projection m_tileProjection;
    QRgb        *m_scanLine;


    TextureTile *m_tile;

    int          m_tileLevel;
    int          m_maxTileLevel;

    int          m_preloadTileLevel;
    int          m_previousRadius;

    // Position of the tile in global Texture Coordinates
    // ( with origin in upper left corner, measured in pixel) 
    int          m_tilePosX;
    int          m_tilePosY;

 private:
    Q_DISABLE_COPY( AbstractScanlineTextureMapper )
    int          m_globalWidth;
    int          m_globalHeight;
    qreal       m_normGlobalWidth;
    qreal       m_normGlobalHeight;
};

inline void AbstractScanlineTextureMapper::setMaxTileLevel( int level )
{
    m_maxTileLevel = level;
}

inline bool AbstractScanlineTextureMapper::interlaced() const
{
    return m_interlaced;
}

inline void AbstractScanlineTextureMapper::setInterlaced( bool enabled )
{
    m_interlaced = enabled;
}

inline int AbstractScanlineTextureMapper::globalWidth() const
{
    return m_globalWidth;
}

inline int AbstractScanlineTextureMapper::globalHeight() const
{
    return m_globalHeight;
}

inline qreal AbstractScanlineTextureMapper::rad2PixelX( qreal longitude ) const
{
    return longitude * m_normGlobalWidth;
}

inline qreal AbstractScanlineTextureMapper::rad2PixelY( qreal lat ) const
{
    switch ( m_tileProjection ) {
    case GeoSceneTexture::Equirectangular:
        return -lat * m_normGlobalHeight;
    case GeoSceneTexture::Mercator:
        if ( fabs( lat ) < 1.4835 ) {
            // We develop the inverse Gudermannian into a MacLaurin Series:
            // Inspite of the many elements needed to get decent 
            // accuracy this is still faster by far than calculating the 
            // trigonometric expression:
            // return - asinh( tan( lat ) ) * 0.5 * m_normGlobalHeight;

            // We are using the Horner Scheme as a polynom representation

            return - gdInv( lat ) * 0.5 * m_normGlobalHeight;
        }
        if ( lat >= +1.4835 )
            // asinh( tan (1.4835)) => 3.1309587
            return - 3.1309587 * 0.5 * m_normGlobalHeight; 
        if ( lat <= -1.4835 )
            // asinh( tan( -1.4835 )) => −3.1309587
            return 3.1309587 * 0.5 * m_normGlobalHeight; 
    }

    // Dummy value to avoid a warning.
    return 0.0;
}

}

#endif

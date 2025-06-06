/***************************************************************************
                         qgssinglebandgrayrenderer.h
                         ---------------------------
    begin                : December 2011
    copyright            : (C) 2011 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSINGLEBANDGRAYRENDERER_H
#define QGSSINGLEBANDGRAYRENDERER_H

#include "qgis_core.h"
#include "qgis_sip.h"
#include "qgsrasterrenderer.h"
#include "qgscolorramplegendnodesettings.h"

#include <memory>

class QgsContrastEnhancement;
class QDomElement;

/**
 * \ingroup core
  * \brief Raster renderer pipe for single band gray.
  */
class CORE_EXPORT QgsSingleBandGrayRenderer: public QgsRasterRenderer
{
  public:
    enum Gradient
    {
      BlackToWhite,
      WhiteToBlack
    };

    QgsSingleBandGrayRenderer( QgsRasterInterface *input, int grayBand );

    //! QgsSingleBandGrayRenderer cannot be copied. Use clone() instead.
    QgsSingleBandGrayRenderer( const QgsSingleBandGrayRenderer & ) = delete;
    //! QgsSingleBandGrayRenderer cannot be copied. Use clone() instead.
    const QgsSingleBandGrayRenderer &operator=( const QgsSingleBandGrayRenderer & ) = delete;

    QgsSingleBandGrayRenderer *clone() const override SIP_FACTORY;
    Qgis::RasterRendererFlags flags() const override;

    static QgsRasterRenderer *create( const QDomElement &elem, QgsRasterInterface *input ) SIP_FACTORY;

    QgsRasterBlock *block( int bandNo, const QgsRectangle &extent, int width, int height, QgsRasterBlockFeedback *feedback = nullptr ) override SIP_FACTORY;

    /**
     * \deprecated QGIS 3.38. Use inputBand() instead.
     */
    Q_DECL_DEPRECATED int grayBand() const SIP_DEPRECATED { return mGrayBand; }

    /**
     * \deprecated QGIS 3.38. Use setInputBand() instead.
     */
    Q_DECL_DEPRECATED void setGrayBand( int band ) SIP_DEPRECATED;

    int inputBand() const override;
    bool setInputBand( int band ) override;

    const QgsContrastEnhancement *contrastEnhancement() const { return mContrastEnhancement.get(); }
    //! Takes ownership
    void setContrastEnhancement( QgsContrastEnhancement *ce SIP_TRANSFER );

    void setGradient( Gradient gradient ) { mGradient = gradient; }
    Gradient gradient() const { return mGradient; }

    void writeXml( QDomDocument &doc, QDomElement &parentElem ) const override;

    QList< QPair< QString, QColor > > legendSymbologyItems() const override;
    QList<QgsLayerTreeModelLegendNode *> createLegendNodes( QgsLayerTreeLayer *nodeLayer ) SIP_FACTORY override;

    QList<int> usesBands() const override;

    Q_DECL_DEPRECATED void toSld( QDomDocument &doc, QDomElement &element, const QVariantMap &props = QVariantMap() ) const override SIP_DEPRECATED;
    bool toSld( QDomDocument &doc, QDomElement &element, QgsSldExportContext &context ) const override;

    /**
     * Returns the color ramp shader legend settings.
     *
     * \see setLegendSettings()
     * \since QGIS 3.18
     */
    const QgsColorRampLegendNodeSettings *legendSettings() const;

    /**
     * Sets the color ramp shader legend \a settings.
     *
     * Ownership of \a settings is transferred.
     *
     * \see legendSettings()
     * \since QGIS 3.18
     */
    void setLegendSettings( QgsColorRampLegendNodeSettings *settings SIP_TRANSFER );

    /**
     * \brief Refreshes the renderer according to the \a min and \a max values associated with the \a extent.
     * If \a min or \a max size is greater than 1, the last values are ignored.
     * If \a forceRefresh is TRUE, this will force the refresh even if needsRefresh() returns FALSE.
     * \returns TRUE if the renderer has been refreshed
     * \note not available in Python bindings
     *
     * \since QGIS 3.42
     */
    bool refresh( const QgsRectangle &extent, const QList<double> &min, const QList<double> &max, bool forceRefresh = false ) override SIP_SKIP;

  private:
#ifdef SIP_RUN
    QgsSingleBandGrayRenderer( const QgsSingleBandGrayRenderer & );
    const QgsSingleBandGrayRenderer &operator=( const QgsSingleBandGrayRenderer & );
#endif

    int mGrayBand;
    Gradient mGradient;
    std::unique_ptr< QgsContrastEnhancement > mContrastEnhancement;
    std::unique_ptr< QgsColorRampLegendNodeSettings > mLegendSettings;
};

#endif // QGSSINGLEBANDGRAYRENDERER_H

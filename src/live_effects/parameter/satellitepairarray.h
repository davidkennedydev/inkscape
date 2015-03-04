#ifndef INKSCAPE_LIVEPATHEFFECT_SATELLITE_PAIR_ARRAY_H
#define INKSCAPE_LIVEPATHEFFECT_SATELLITE_PAIR_ARRAY_H

/*
 * Inkscape::LivePathEffectParameters
 * Copyright (C) Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 * Special thanks to Johan Engelen for the base of the effect -powerstroke-
 * Also to ScislaC for point me to the idea
 * Also su_v for his construvtive feedback and time
 * To Nathan Hurst for his review and help on refactor
 * and finaly to Liam P. White for his big help on coding, that save me a lot of
 * hours
 * 
 * 
 * This parameter act as bridge from pointwise class to serialize it as a LPE
 * parameter
 * 
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glib.h>
#include "live_effects/parameter/array.h"
#include "knot-holder-entity.h"
#include <2geom/pointwise.h>

namespace Inkscape {

namespace LivePathEffect {

class SatellitePairArrayParamKnotHolderEntity;

class SatellitePairArrayParam : public ArrayParam<std::pair<unsigned int, Geom::Satellite> > {
public:
    SatellitePairArrayParam(const Glib::ustring &label,
                                 const Glib::ustring &tip,
                                 const Glib::ustring &key,
                                 Inkscape::UI::Widget::Registry *wr,
                                 Effect *effect);
    virtual ~SatellitePairArrayParam();

    virtual Gtk::Widget * param_newWidget() {
        return NULL;
    }

    void set_oncanvas_looks(SPKnotShapeType shape, SPKnotModeType mode,
                            guint32 color);
    virtual void set_helper_size(int hs);
    virtual void addKnotHolderEntities(KnotHolder *knotholder, SPDesktop *desktop, SPItem *item);
    virtual void addKnotHolderEntitieMirrored(KnotHolder *knotholder, SPDesktop *desktop, SPItem *item, int i);
    virtual void addCanvasIndicators(SPLPEItem const *lpeitem,std::vector<Geom::PathVector> &hp_vec);
    virtual bool providesKnotHolderEntities() const {
        return true;
    }
    void set_document_unit(Glib::ustring const * value_document_unit);
    void set_use_distance(bool use_knot_distance );
    void set_unit(const gchar *abbr);
    virtual void updateCanvasIndicators();
    void set_pointwise(Geom::Pointwise *pointwise);
    friend class SatellitePairArrayParamKnotHolderEntity;

private:
    SatellitePairArrayParam(const SatellitePairArrayParam &);
    SatellitePairArrayParam &operator=(const SatellitePairArrayParam &);

    SPKnotShapeType knot_shape;
    SPKnotModeType knot_mode;
    guint32 knot_color;
    Geom::PathVector hp;
    int helper_size;
    bool use_distance;
    const gchar *unit;
    Glib::ustring const * documentUnit;
    Geom::Pointwise *last_pointwise;

};

class SatellitePairArrayParamKnotHolderEntity : public KnotHolderEntity {
public:
    SatellitePairArrayParamKnotHolderEntity(SatellitePairArrayParam *p, unsigned int index);
    virtual ~SatellitePairArrayParamKnotHolderEntity() {}

    virtual void knot_set(Geom::Point const &p, Geom::Point const &origin, guint state);
    virtual Geom::Point knot_get() const;
    virtual void knot_click(guint state);
    void knot_set_offset(Geom::Satellite);
    /** Checks whether the index falls within the size of the parameter's vector */
    bool valid_index(unsigned int index) const {
        return (_pparam->_vector.size() > index);
    };

private:
    SatellitePairArrayParam *_pparam;
    unsigned int _index;
};

} //namespace LivePathEffect

} //namespace Inkscape

#endif

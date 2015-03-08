/**
 *
 * From the code of Liam P.White from his Power Stroke Knot dialog
 *
 * Released under GNU GPL.  Read the file 'COPYING' for more information
 */

#ifndef INKSCAPE_DIALOG_FILLET_CHAMFER_PROPERTIES_H
#define INKSCAPE_DIALOG_FILLET_CHAMFER_PROPERTIES_H

#include <2geom/point.h>
#include <gtkmm.h>
#include "live_effects/parameter/satellitepairarray.h"

class SPDesktop;

namespace Inkscape {
namespace UI {
namespace Dialogs {

class FilletChamferPropertiesDialog : public Gtk::Dialog {
public:
    FilletChamferPropertiesDialog();
    virtual ~FilletChamferPropertiesDialog();

    Glib::ustring getName() const {
        return "LayerPropertiesDialog";
    }

    static void showDialog(SPDesktop *desktop, double amount,
                           const Inkscape::LivePathEffect::
                           FilletChamferKnotHolderEntity *pt,
                           const gchar *unit,
                           bool use_distance,
                           bool aprox_radius,
                           Glib::ustring documentUnit,
                           Geom::Satellite satellite);

protected:

    SPDesktop *_desktop;
    Inkscape::LivePathEffect::FilletChamferKnotHolderEntity *
    _knotpoint;

    Gtk::Label _fillet_chamfer_position_label;
    Gtk::SpinButton _fillet_chamfer_position_numeric;
    Gtk::RadioButton::Group _fillet_chamfer_type_group;
    Gtk::RadioButton _fillet_chamfer_type_fillet;
    Gtk::RadioButton _fillet_chamfer_type_inverse_fillet;
    Gtk::RadioButton _fillet_chamfer_type_chamfer;
    Gtk::RadioButton _fillet_chamfer_type_inverse_chamfer;
    Gtk::Label _fillet_chamfer_chamfer_subdivisions_label;
    Gtk::SpinButton _fillet_chamfer_chamfer_subdivisions;

    Gtk::Table _layout_table;
    bool _position_visible;

    Gtk::Button _close_button;
    Gtk::Button _apply_button;

    sigc::connection _destroy_connection;

    static FilletChamferPropertiesDialog &_instance() {
        static FilletChamferPropertiesDialog instance;
        return instance;
    }

    void _set_desktop(SPDesktop *desktop);
    void _set_pt(const Inkscape::LivePathEffect::
                FilletChamferKnotHolderEntity *pt);
    void _set_unit(const gchar *abbr);
    void _set_document_unit(Glib::ustring abbr);
    void _set_use_distance(bool use_knot_distance);
    void _set_aprox(bool aprox_radius);
    void _apply();
    void _close();
    bool _flexible;
    Geom::Satellite _satellite;
    const gchar *unit;
    Glib::ustring document_unit;
    bool use_distance;
    double amount;
    bool aprox;
    void _set_amount(double amount);
    void _set_satellite(Geom::Satellite satellite);
    void _prepareLabelRenderer(Gtk::TreeModel::const_iterator const &row);

    bool _handleKeyEvent(GdkEventKey *event);
    void _handleButtonEvent(GdkEventButton *event);

    friend class Inkscape::LivePathEffect::
            FilletChamferKnotHolderEntity;

private:
    FilletChamferPropertiesDialog(
        FilletChamferPropertiesDialog const &); // no copy
    FilletChamferPropertiesDialog &operator=(
        FilletChamferPropertiesDialog const &); // no assign
};

} // namespace
} // namespace
} // namespace

#endif //INKSCAPE_DIALOG_LAYER_PROPERTIES_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:
// filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99
// :

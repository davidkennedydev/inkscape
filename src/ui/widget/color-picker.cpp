// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Abhishek Sharma
 *
 * Copyright (C) Authors 2000-2005
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-picker.h"
#include "inkscape.h"
#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "ui/dialog-events.h"

#include "ui/widget/color-notebook.h"
#include "verbs.h"


static bool _in_use = false;

namespace Inkscape {
namespace UI {
namespace Widget {

ColorPicker::ColorPicker (const Glib::ustring& title, const Glib::ustring& tip,
                          guint32 rgba, bool undo)
          : _preview(rgba), _title(title), _rgba(rgba), _undo(undo),
           _colorSelectorDialog("dialogs.colorpickerwindow")
{
    setupDialog(title);
    _preview.show();
    add (_preview);
    set_tooltip_text (tip);
    _parent_dialog = nullptr;
    _selected_color.signal_changed.connect(sigc::mem_fun(this, &ColorPicker::_onSelectedColorChanged));
    _selected_color.signal_dragged.connect(sigc::mem_fun(this, &ColorPicker::_onSelectedColorChanged));
    _selected_color.signal_released.connect(sigc::mem_fun(this, &ColorPicker::_onSelectedColorChanged));
}

ColorPicker::~ColorPicker()
{
    closeWindow();
}

void ColorPicker::setupDialog(const Glib::ustring &title)
{
    GtkWidget *dlg = GTK_WIDGET(_colorSelectorDialog.gobj());
    sp_transientize(dlg);

    _colorSelectorDialog.hide();
    _colorSelectorDialog.set_title (title);
    _colorSelectorDialog.set_border_width (4);

    _color_selector = Gtk::manage(new ColorNotebook(_selected_color));
    _colorSelectorDialog.get_content_area()->pack_start (
              *_color_selector, true, true, 0);
    _color_selector->show();
}

void ColorPicker::setParentDialog(Gtk::Widget *parent_dialog) { _parent_dialog = parent_dialog; }

void ColorPicker::setSensitive(bool sensitive) { set_sensitive(sensitive); }

void ColorPicker::setRgba32 (guint32 rgba)
{
    if (_in_use) return;

    _preview.setRgba32 (rgba);
    _rgba = rgba;
    if (_color_selector)
    {
        _updating = true;
        _selected_color.setValue(rgba);
        _updating = false;
    }
}

void ColorPicker::closeWindow()
{
    _colorSelectorDialog.hide();
}

void ColorPicker::on_clicked()
{
    if (_color_selector)
    {
        _updating = true;
        _selected_color.setValue(_rgba);
        _updating = false;
    }
    Gtk::Window *originalwindow = dynamic_cast<Gtk::Window *>(_parent_dialog->get_toplevel());
    if (originalwindow) {
        originalwindow->hide();
    }
    _colorSelectorDialog.show();
    Glib::RefPtr<Gdk::Window> window = get_parent_window();
    if (window) {
        window->focus(1);
    }
    if (originalwindow) {
        originalwindow->show();
    }

}

void ColorPicker::on_changed (guint32)
{
}

void ColorPicker::_onSelectedColorChanged() {
    if (_updating) {
        return;
    }

    if (_in_use) {
        return;
    } else {
        _in_use = true;
    }

    guint32 rgba = _selected_color.value();
    _preview.setRgba32(rgba);

    if (_undo && SP_ACTIVE_DESKTOP) {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), SP_VERB_NONE,
                           /* TODO: annotate */ "color-picker.cpp:130");
    }

    on_changed(rgba);
    _in_use = false;
    _changed_signal.emit(rgba);
    _rgba = rgba;
}

}//namespace Widget
}//namespace UI
}//namespace Inkscape


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

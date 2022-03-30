// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Paint Servers dialog
 */
/* Authors:
 *   Valentin Ionita
 *   Rafael Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2019 Valentin Ionita
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <algorithm>
#include <map>

#include <giomm/listmodel.h>
#include <glibmm/regex.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/iconview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/stockid.h>
#include <gtkmm/switch.h>

#include "document.h"
#include "inkscape.h"
#include "paint-servers.h"
#include "path-prefix.h"
#include "style.h"

#include "io/resource.h"
#include "object/sp-defs.h"
#include "object/sp-hatch.h"
#include "object/sp-pattern.h"
#include "object/sp-root.h"
#include "ui/cache/svg_preview_cache.h"
#include "ui/widget/scrollprotected.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

static Glib::ustring const wrapper = R"=====(
<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <defs id="Defs"/>
  <rect id="Back" x="0" y="0" width="100px" height="100px" fill="lightgray"/>
  <rect id="Rect" x="0" y="0" width="100px" height="100px" stroke="black"/>
</svg>
)=====";

// Constructor
PaintServersDialog::PaintServersDialog()
    : DialogBase("/dialogs/paint", "PaintServers")
    , target_selected(true)
    , ALLDOCS(_("All paint servers"))
    , CURRENTDOC(_("Current document"))
    , columns()
{
    current_store = ALLDOCS;
    store[ALLDOCS] = Gtk::ListStore::create(columns);
    store[CURRENTDOC] = Gtk::ListStore::create(columns);

    // Grid holding the contents
    Gtk::Grid *grid = Gtk::manage(new Gtk::Grid());
    grid->set_margin_start(3);
    grid->set_margin_end(3);
    grid->set_margin_top(3);
    grid->set_row_spacing(3);
    pack_start(*grid, Gtk::PACK_EXPAND_WIDGET);

    // Grid row 0
    Gtk::Label *file_label = Gtk::manage(new Gtk::Label(Glib::ustring(_("Server")) + ": "));
    grid->attach(*file_label, 0, 0, 1, 1);

    dropdown = Gtk::manage(new Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBoxText>());
    dropdown->append(ALLDOCS);
    dropdown->append(CURRENTDOC);
    dropdown->set_active_text(ALLDOCS);
    dropdown->set_hexpand();
    grid->attach(*dropdown, 1, 0, 1, 1);

    // Grid row 1
    Gtk::Label *fill_label = Gtk::manage(new Gtk::Label(Glib::ustring(_("Change")) + ": "));
    grid->attach(*fill_label, 0, 1, 1, 1);

    target_dropdown = Gtk::manage(new Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBoxText>());
    target_dropdown->append(_("Fill"));
    target_dropdown->append(_("Stroke"));
    target_dropdown->set_active_text(_("Fill"));
    target_dropdown->set_hexpand();
    grid->attach(*target_dropdown, 1, 1, 1, 1);

    // Grid row 2
    icon_view = Gtk::manage(new Gtk::IconView(
        static_cast<Glib::RefPtr<Gtk::TreeModel>>(store[current_store])
    ));
    icon_view->set_tooltip_column(0);
    icon_view->set_pixbuf_column(2);
    icon_view->set_size_request(200, -1);
    icon_view->show_all_children();
    icon_view->set_selection_mode(Gtk::SELECTION_SINGLE);
    icon_view->set_activate_on_single_click(true);

    Gtk::ScrolledWindow *scroller = Gtk::manage(new Gtk::ScrolledWindow());
    scroller->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scroller->set_hexpand();
    scroller->set_vexpand();
    scroller->add(*icon_view);
    scroller->set_overlay_scrolling(false);
    grid->attach(*scroller, 0, 2, 2, 1);
    fix_inner_scroll(scroller);

    // Events
    target_dropdown->signal_changed().connect([=]() { target_selected = !target_selected; });
    dropdown->signal_changed().connect([=]() { onPaintSourceDocumentChanged(); });
    icon_view->signal_item_activated().connect([=](Gtk::TreeModel::Path const &p) { onPaintClicked(p); });

    // Get wrapper document (rectangle to fill with paint server).
    preview_document = SPDocument::createNewDocFromMem(wrapper.c_str(), wrapper.length(), true);

    SPObject *rect = preview_document->getObjectById("Rect");
    SPObject *defs = preview_document->getObjectById("Defs");
    if (!rect || !defs) {
        g_warn_message("Inkscape", __FILE__, __LINE__, __func__,
                       "Failed to get wrapper defs or rectangle for preview document!");
    }

    // Set up preview document.
    unsigned key = SPItem::display_key_new(1);
    preview_document->getRoot()->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    preview_document->ensureUpToDate();
    renderDrawing.setRoot(preview_document->getRoot()->invoke_show(renderDrawing, key, SP_ITEM_SHOW_DISPLAY));

    _loadStockPaints();
}


PaintServersDialog::~PaintServersDialog()
{
    _defs_changed.disconnect();
    _document_closed.disconnect();
}

/** Handles the replacement of the document that we edit */
void PaintServersDialog::documentReplaced()
{
    _defs_changed.disconnect();
    _document_closed.disconnect();

    auto document = getDocument();
    if (!document) {
        return;
    }
    document_map[CURRENTDOC] = document;
    _loadFromCurrentDocument();
    _regenerateAll();

    if (auto const defs = document->getDefs()) {
        _defs_changed = defs->connectModified([=](SPObject *, unsigned) -> void {
            _loadFromCurrentDocument();
            _regenerateAll();
        });
    }
    _document_closed = document->connectDestroy([=]() { _documentClosed(); });
}

/** Handles the destruction of the current document */
void PaintServersDialog::_documentClosed()
{
    _defs_changed.disconnect();
    _document_closed.disconnect();

    document_map.erase(CURRENTDOC);
    store[CURRENTDOC]->clear();
    _regenerateAll();
}

/** Finds paints used by an object and (recursively) by its descendants
 *  @param in - the object whose paints to grab
 *  @param list - the paints will be added to this vector as strings usable in the `fill` CSS property
 */
void PaintServersDialog::_findPaints(SPObject *in, std::vector<Glib::ustring> &list)
{
    g_return_if_fail(in != nullptr);

    // Add paint servers in <defs> section.
    if (dynamic_cast<SPPaintServer *>(in)) {
        if (in->getId()) {
            // Need to check as one can't construct Glib::ustring with nullptr.
            list.push_back(Glib::ustring("url(#") + in->getId() + ")");
        }
        // Don't recurse into paint servers.
        return;
    }

    // Add paint servers referenced by shapes.
    if (dynamic_cast<SPShape *>(in)) {
        auto const style = in->style;
        list.push_back(style->fill.get_value());
        list.push_back(style->stroke.get_value());
    }

    for (auto child: in->childList(false)) {
        PaintServersDialog::_findPaints(child, list);
    }
}

/** Load stock paints from files in share/paint */
void PaintServersDialog::_loadStockPaints()
{
    std::vector<PaintDescription> paints;

    // Extract out paints from files in share/paint.
    for (auto const &path : get_filenames(Inkscape::IO::Resource::PAINT, {".svg"})) {
        try { // createNewDoc throws
            auto doc = std::unique_ptr<SPDocument>(SPDocument::createNewDoc(path.c_str(), false));
            if (!doc) {
                throw std::exception();
            }
            _loadPaintsFromDocument(doc.get(), paints);
            _stock_documents.push_back(std::move(doc)); // Ensures eventual destruction in our dtor
        } catch (std::exception &e) {
            auto message = Glib::ustring{"Cannot open paint server resource file '"} + path + "'!";
            g_warn_message("Inkscape", __FILE__, __LINE__, __func__, message.c_str());
            continue;
        }
    }

    _createPaints(paints);
}

/** Load paint servers from the <defs> of the current document */
void PaintServersDialog::_loadFromCurrentDocument()
{
    auto document = getDocument();
    if (!document) {
        return;
    }

    std::vector<PaintDescription> paints;
    _loadPaintsFromDocument(document, paints);

    // There can only be one current document, so we clear the corresponding store
    store[CURRENTDOC]->clear();
    _createPaints(paints);
}

/** Creates a collection of paints from the given vector of descriptions */
void PaintServersDialog::_createPaints(std::vector<PaintDescription> &collection)
{
    // Sort and remove duplicates.
    auto paints_cmp = [](PaintDescription const &a, PaintDescription const &b) -> bool {
        return a.url < b.url;
    };
    std::sort(collection.begin(), collection.end(), paints_cmp);
    collection.erase(std::unique(collection.begin(), collection.end()), collection.end());

    for (auto &paint : collection) {
        _instantiatePaint(paint);
    }
}

/** Create a paint from a description and generate its bitmap preview */
void PaintServersDialog::_instantiatePaint(PaintDescription &paint)
{
    if (!paint.has_preview()) {
        _generateBitmapPreview(paint);
    }
    if (paint.has_preview()) { // don't add the paint if preview generation failed.
        _addToStore(paint);
    }
}

/** Adds a paint to store */
void PaintServersDialog::_addToStore(PaintDescription &paint)
{
    if (store.find(paint.doc_title) == store.end()) {
        store[paint.doc_title] = Gtk::ListStore::create(columns);
    }

    auto iter = store[paint.doc_title]->append();
    paint.write_to_iterator(iter, &columns);

    if (document_map.find(paint.doc_title) == document_map.end()) {
        document_map[paint.doc_title] = paint.source_document;
        dropdown->append(paint.doc_title);
    }
}

/** Returns a PaintDescription for a paint already present in the store */
PaintDescription PaintServersDialog::_descriptionFromIterator(Gtk::ListStore::iterator const &iter) const
{
    Glib::ustring doc_title = (*iter)[columns.document];
    SPDocument *doc_ptr;
    try {
        doc_ptr = document_map.at(doc_title);
    } catch (std::out_of_range &exception) {
        doc_ptr = nullptr;
    }
    Glib::ustring paint_url = (*iter)[columns.paint];
    PaintDescription result(doc_ptr, doc_title, std::move(paint_url));

    // Fill in fields that are set only on instantiation
    result.id = (*iter)[columns.id];
    result.bitmap = (*iter)[columns.pixbuf];
    return result;
}

/** Regenerates the list of all paint servers from the already loaded paints */
void PaintServersDialog::_regenerateAll()
{
    // Save active item
    bool showing_all = (current_store == ALLDOCS);
    Gtk::TreePath active;
    if (showing_all) {
        std::vector<Gtk::TreePath> selected = icon_view->get_selected_items();
        if (selected.empty()) {
            showing_all = false;
        } else {
            active = selected[0];
        }
    }

    std::vector<PaintDescription> all_paints;

    for (auto const &[doc, paint_list] : store) {
        if (doc == ALLDOCS) {
            continue; // ignore the target store
        }
        paint_list->foreach_iter([&](Gtk::ListStore::iterator const &paint) -> bool
        {
            all_paints.push_back(_descriptionFromIterator(paint));
            return false;
        });
    }

    // Sort and remove duplicates. When the duplicate entry is from the current document,
    // we remove it preferentially, keeping the stock paint if available.
    std::sort(all_paints.begin(), all_paints.end(),
        [=](PaintDescription const &a, PaintDescription const &b) -> bool
        {
            return (a.url < b.url) || ((a.url == b.url) && a.doc_title != CURRENTDOC);
        });
    all_paints.erase(std::unique(all_paints.begin(), all_paints.end()), all_paints.end());

    store[ALLDOCS]->clear();

    // Add paints from the cleaned up list to the store
    for (auto &&paint : all_paints) {
        auto iter = store[ALLDOCS]->append();
        paint.write_to_iterator(iter, &columns);
    }

    // Restore active item
    if (showing_all) {
        icon_view->select_path(active);
    }
}

/** Generates the bitmap preview for the given paint */
void PaintServersDialog::_generateBitmapPreview(PaintDescription &paint)
{
    SPObject *rect = preview_document->getObjectById("Rect");
    SPObject *defs = preview_document->getObjectById("Defs");

    paint.bitmap = Glib::RefPtr<Gdk::Pixbuf>(nullptr);
    if (paint.url.empty()) {
        return;
    }

    // Set style on the preview rectangle
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property(css, "fill", paint.url.c_str());
    rect->changeCSS(css, "style");
    sp_repr_css_attr_unref(css);

    // Insert paint into the defs of the preview document if required
    Glib::MatchInfo matchInfo;
    static Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create("url\\(#([A-Za-z0-9#._-]*)\\)");

    regex->match(paint.url, matchInfo);
    if (!matchInfo.matches()) {
        // Currently we only show previews for hatches/patterns of the form url(#some-id)
        // TODO: handle colors, gradients, etc.
        // See https://wiki.inkscape.org/wiki/Google_Summer_of_Code#P11._Improvements_to_Paint_Server_Dialog
        return;
    }
    paint.id = matchInfo.fetch(1);

    // Delete old paints if necessary
    std::vector<SPObject *> old_paints = preview_document->getObjectsBySelector("defs > *");
    for (auto paint : old_paints) {
        paint->deleteObject(false);
    }

    // Find the new paint
    SPObject *new_paint = paint.source_document->getObjectById(paint.id);
    if (!new_paint) {
        Glib::ustring error_message = Glib::ustring{"Cannot find paint server: "} + paint.id;
        g_warn_message("Inkscape", __FILE__, __LINE__, __func__, error_message.c_str());
        return;
    }

    // Add the new paint along with all paints it refers to
    XML::Document *xml_doc = preview_document->getReprDoc();
    std::vector<SPObject *> encountered{new_paint}; ///< For the prevention of cyclic refs

    while (new_paint) {
        auto const *new_repr = new_paint->getRepr();
        if (!new_repr) {
            break;
        }

        // Create a copy repr of the paint
        defs->appendChild(new_repr->duplicate(xml_doc));

        // Check for cross-references in the paint
        auto const xlink = new_repr->attribute("xlink:href");
        auto const href = new_repr->attribute("href");
        if (xlink || href) {
            // Paint is cross-referencing another object (probably another paint);
            // we must copy the referenced object as well
            auto const ref = (href ? href : xlink); // Prefer "href" since "xlink:href" is obsolete
            new_paint = paint.source_document->getObjectByHref(ref);
            using namespace std;
            if (find(begin(encountered), end(encountered), new_paint) == end(encountered)) {
                encountered.push_back(new_paint);
            } else {
                break; // Break reference cycle
            }
        } else { // No more hrefs
            break;
        }
    }

    preview_document->getRoot()->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    preview_document->ensureUpToDate();

    if (Geom::OptRect dbox = static_cast<SPItem *>(rect)->visualBounds())
    {
        unsigned size = std::ceil(std::max(dbox->width(), dbox->height()));
        paint.bitmap = Glib::wrap(render_pixbuf(renderDrawing, 1, *dbox, size));
    }
}

/** @brief Load paint servers from the given source document
 *  @param document - the source document
 *  @param output - the paint descriptions will be added to this vector */
void PaintServersDialog::_loadPaintsFromDocument(SPDocument *document, std::vector<PaintDescription> &output)
{
    Glib::ustring document_title;
    if (!document->getRoot()->title()) {
        document_title = CURRENTDOC;
    } else {
        document_title = Glib::ustring(document->getRoot()->title());
    }

    // Find all paints
    std::vector<Glib::ustring> urls;
    _findPaints(document->getRoot(), urls);

    for (auto const &url : urls) {
        output.emplace_back(document, document_title, std::move(url));
    }
}

/** Handles the change of the dropdown for selecting paint sources */
void PaintServersDialog::onPaintSourceDocumentChanged()
{
    current_store = dropdown->get_active_text();
    icon_view->set_model(store[current_store]);
}

/** Event handler for when a paint entry in the dialog has been activated */
void PaintServersDialog::onPaintClicked(Gtk::TreeModel::Path const &path)
{
    // Get the current selected elements
    Selection *selection = getSelection();
    std::vector<SPObject*> const selected_items(selection->items().begin(), selection->items().end());

    if (selected_items.empty()) {
        return;
    }

    Gtk::ListStore::iterator iter = store[current_store]->get_iter(path);
    Glib::ustring id = (*iter)[columns.id];
    Glib::ustring paint = (*iter)[columns.paint];
    Glib::RefPtr<Gdk::Pixbuf> pixbuf = (*iter)[columns.pixbuf];
    Glib::ustring hatches_document_title = (*iter)[columns.document];
    SPDocument *hatches_document = document_map[hatches_document_title];
    SPObject *paint_server = hatches_document->getObjectById(id);

    bool paint_server_exists = false;
    for (auto const &server : store[CURRENTDOC]->children()) {
        if (server[columns.id] == id) {
            paint_server_exists = true;
            break;
        }
    }

    SPDocument *document = getDocument();
    if (!paint_server_exists) {
        // Add the paint server to the current document definition
        Inkscape::XML::Document *xml_doc = document->getReprDoc();
        Inkscape::XML::Node *repr = paint_server->getRepr()->duplicate(xml_doc);
        document->getDefs()->appendChild(repr);
        Inkscape::GC::release(repr);

        // Add the pixbuf to the current document store
        iter = store[CURRENTDOC]->append();
        (*iter)[columns.id] = id;
        (*iter)[columns.paint] = paint;
        (*iter)[columns.pixbuf] = pixbuf;
        (*iter)[columns.document] = CURRENTDOC;
    }

    // Recursively find elements in groups, if any
    std::vector<SPObject*> items;
    for (auto item : selected_items) {
        _unpackGroups(item, items);
    }

    for (auto item : items) {
        item->style->getFillOrStroke(target_selected)->read(paint.c_str());
        item->updateRepr();
    }

    document->collectOrphans();
}

/**
 * Recursively extracts non-group elements from groups, if any
 * @param parent - the parent object which will be unpacked recursively
 * @param output - the resulting SPObject pointers will be added to this vector
 */
void PaintServersDialog::_unpackGroups(SPObject *parent, std::vector<SPObject *> &output) const
{
    std::vector<SPObject *> children = parent->childList(false);
    if (children.empty()) {
        output.push_back(parent);
    } else {
        for (auto child : children) {
            _unpackGroups(child, output);
        }
    }
}

//----------------------------------------------------------------------------------------------------

PaintDescription::PaintDescription(SPDocument *source_doc, Glib::ustring title, Glib::ustring const &&paint_url)
    : source_document{source_doc}
    , doc_title{std::move(title)}
    , id{} // id will be filled in when generating the bitmap
    , url{paint_url}
    , bitmap{nullptr}
{}

/** Write the data stored in this struct to a list store
 * @param it - the iterator to the ListStore to write to
 */
void PaintDescription::write_to_iterator(Gtk::ListStore::iterator &it, PaintServersColumns const *cols) const {
    (*it)[cols->id] = id;
    (*it)[cols->paint] = url;
    (*it)[cols->pixbuf] = bitmap;
    (*it)[cols->document] = doc_title;
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-basic-offset:2
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :

/*
 * feComponentTransfer filter primitive renderer
 *
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Jasper van de Gronde <th.v.d.gronde@hccnet.nl>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <math.h>
#include "display/cairo-templates.h"
#include "display/cairo-utils.h"
#include "display/nr-filter-component-transfer.h"
#include "display/nr-filter-slot.h"

namespace Inkscape {
namespace Filters {

FilterComponentTransfer::FilterComponentTransfer()
{
}

FilterPrimitive * FilterComponentTransfer::create() {
    return new FilterComponentTransfer();
}

FilterComponentTransfer::~FilterComponentTransfer()
{}

struct ComponentTransfer {
    ComponentTransfer(guint32 color)
        : _shift(color * 8)
        , _mask(0xff << _shift)
    {}
protected:
    guint32 _shift;
    guint32 _mask;
};

template <bool alpha>
struct ComponentTransferTable;

template <>
struct ComponentTransferTable<false> : public ComponentTransfer {
    ComponentTransferTable(guint32 color, std::vector<double> const &values)
        : ComponentTransfer(color)
        , _v(values.size())
    {
        for (unsigned i = 0; i< values.size(); ++i) {
            _v[i] = round(CLAMP(values[i], 0.0, 1.0) * 255);
        }
    }
    guint32 operator()(guint32 in) {
        guint32 component = (in & _mask) >> _shift;
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return in;

        component = (255 * component + alpha/2) / alpha;
        guint32 k = (_v.size() - 1) * component;
        guint32 dx = k % 255;  k /= 255;
        component = _v[k]*255 + (_v[k+1] - _v[k])*dx;
        component = (component + 127) / 255;
        component = premul_alpha(component, alpha);
        return (in & ~_mask) | (component << _shift);
    }
private:
    std::vector<guint32> _v;
};

template <>
struct ComponentTransferTable<true> {
    ComponentTransferTable(std::vector<double> const &values)
        : _v(values.size())
    {
        for (unsigned i = 0; i< values.size(); ++i) {
            _v[i] = round(CLAMP(values[i], 0.0, 1.0) * 255);
        }
    }
    guint32 operator()(guint32 in) {
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return in;

        guint32 k = (_v.size() - 1) * alpha;
        guint32 dx = k % 255;  k /= 255;
        alpha = _v[k]*255 + (_v[k+1] - _v[k])*dx;
        alpha = (alpha + 127) / 255;
        return (in & 0x00ffffff) | (alpha << 24);
    }
private:
    std::vector<guint32> _v;
};

template <bool alpha>
struct ComponentTransferDiscrete;

template <>
struct ComponentTransferDiscrete<false> : public ComponentTransfer {
    ComponentTransferDiscrete(guint32 color, std::vector<double> const &values)
        : ComponentTransfer(color)
        , _v(values.size())
    {
        for (unsigned i = 0; i< values.size(); ++i) {
            _v[i] = round(CLAMP(values[i], 0.0, 1.0) * 255);
        }
    }
    guint32 operator()(guint32 in) {
        guint32 component = (in & _mask) >> _shift;
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return in;

        component = (255 * component + alpha/2) / alpha;
        guint32 k = (_v.size() - 1) * component / 255;
        component = _v[k];
        component = premul_alpha(component, alpha);
        return (in & ~_mask) | (component << _shift);
    }
private:
    std::vector<guint32> _v;
};

template <>
struct ComponentTransferDiscrete<true> {
    ComponentTransferDiscrete(std::vector<double> const &values)
        : _v(values.size())
    {
        for (unsigned i = 0; i< values.size(); ++i) {
            _v[i] = round(CLAMP(values[i], 0.0, 1.0) * 255);
        }
    }
    guint32 operator()(guint32 in) {
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return in;

        guint32 k = (_v.size() - 1) * alpha / 255;
        alpha = _v[k];
        return (in & 0x00ffffff) | (alpha << 24);
    }
private:
    std::vector<guint32> _v;
};

template <bool alpha>
struct ComponentTransferLinear;

template <>
struct ComponentTransferLinear<false> : public ComponentTransfer {
    ComponentTransferLinear(guint32 color, double intercept, double slope)
        : ComponentTransfer(color)
        , _intercept(round(intercept*255*255))
        , _slope(round(slope*255))
    {}
    guint32 operator()(guint32 in) {
        gint32 component = (in & _mask) >> _shift;
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return 0;

        // TODO: this can probably be reduced to something simpler
        component = (255 * component + alpha/2) / alpha;
        component = pxclamp(_slope * component + _intercept, 0, 255*255);
        component = (component + 127) / 255;
        component = premul_alpha(component, alpha);
        return (in & ~_mask) | (component << _shift);
    }
private:
    gint32 _intercept;
    gint32 _slope;
};

template <>
struct ComponentTransferLinear<true> {
    ComponentTransferLinear(double intercept, double slope)
        : _intercept(round(intercept*255*255))
        , _slope(round(slope*255))
    {}
    guint32 operator()(guint32 in) {
        gint32 alpha = (in & 0xff000000) >> 24;
        alpha = pxclamp(_slope * alpha + _intercept, 0, 255*255);
        alpha = (alpha + 127) / 255;
        return (in & 0x00ffffff) | (alpha << 24);
    }
private:
    gint32 _intercept;
    gint32 _slope;
};

template <bool alpha>
struct ComponentTransferGamma;

template <>
struct ComponentTransferGamma<false> : public ComponentTransfer {
    ComponentTransferGamma(guint32 color, double amplitude, double exponent, double offset)
        : ComponentTransfer(color)
        , _amplitude(amplitude)
        , _exponent(exponent)
        , _offset(offset)
    {}
    guint32 operator()(guint32 in) {
        double component = (in & _mask) >> _shift;
        guint32 alpha = (in & 0xff000000) >> 24;
        if (alpha == 0) return 0;

        double alphaf = alpha;
        component /= alphaf;
        component = _amplitude * pow(component, _exponent) + _offset;
        guint32 cpx = pxclamp(component * alphaf, 0, 255);
        return (in & ~_mask) | (cpx << _shift);
    }
private:
    double _amplitude;
    double _exponent;
    double _offset;
};

template <>
struct ComponentTransferGamma<true> {
    ComponentTransferGamma(double amplitude, double exponent, double offset)
        : _amplitude(amplitude)
        , _exponent(exponent)
        , _offset(offset)
    {}
    guint32 operator()(guint32 in) {
        double alpha = (in & 0xff000000) >> 24;
        alpha /= 255.0;
        alpha = _amplitude * pow(alpha, _exponent) + _offset;
        guint32 cpx = pxclamp(alpha * 255.0, 0, 255);
        return (in & 0x00ffffff) | (cpx << 24);
    }
private:
    double _amplitude;
    double _exponent;
    double _offset;
};

void FilterComponentTransfer::render_cairo(FilterSlot &slot)
{
    cairo_surface_t *input = slot.getcairo(_input);
    cairo_surface_t *out = ink_cairo_surface_create_same_size(input, CAIRO_CONTENT_COLOR_ALPHA);
    //cairo_surface_t *outtemp = ink_cairo_surface_create_identical(out);
    ink_cairo_surface_blit(input, out);

    // parameters: R = 0, G = 1, B = 2, A = 3
    // Cairo:      R = 2, G = 1, B = 0, A = 3
    for (unsigned i = 0; i < 3; ++i) {
        guint32 color = 2 - i;
        switch (type[i]) {
        case COMPONENTTRANSFER_TYPE_TABLE:
            ink_cairo_surface_filter(out, out,
                ComponentTransferTable<false>(color, tableValues[i]));
            break;
        case COMPONENTTRANSFER_TYPE_DISCRETE:
            ink_cairo_surface_filter(out, out,
                ComponentTransferDiscrete<false>(color, tableValues[i]));
            break;
        case COMPONENTTRANSFER_TYPE_LINEAR:
            ink_cairo_surface_filter(out, out,
                ComponentTransferLinear<false>(color, intercept[i], slope[i]));
            break;
        case COMPONENTTRANSFER_TYPE_GAMMA:
            ink_cairo_surface_filter(out, out,
                ComponentTransferGamma<false>(color, amplitude[i], exponent[i], offset[i]));
            break;
        case COMPONENTTRANSFER_TYPE_ERROR:
        case COMPONENTTRANSFER_TYPE_IDENTITY:
        default:
            break;
        }
        //ink_cairo_surface_blit(out, outtemp);
    }

    // fast paths for alpha channel
    switch (type[3]) {
    case COMPONENTTRANSFER_TYPE_TABLE:
        ink_cairo_surface_filter(out, out,
            ComponentTransferTable<true>(tableValues[3]));
        break;
    case COMPONENTTRANSFER_TYPE_DISCRETE:
        ink_cairo_surface_filter(out, out,
            ComponentTransferDiscrete<true>(tableValues[3]));
        break;
    case COMPONENTTRANSFER_TYPE_LINEAR:
        ink_cairo_surface_filter(out, out,
            ComponentTransferLinear<true>(intercept[3], slope[3]));
        break;
    case COMPONENTTRANSFER_TYPE_GAMMA:
        ink_cairo_surface_filter(out, out,
            ComponentTransferGamma<true>(amplitude[3], exponent[3], offset[3]));
        break;
    case COMPONENTTRANSFER_TYPE_ERROR:
    case COMPONENTTRANSFER_TYPE_IDENTITY:
    default:
        break;
    }

    slot.set(_output, out);
    cairo_surface_destroy(out);
    //cairo_surface_destroy(outtemp);
}

bool FilterComponentTransfer::can_handle_affine(Geom::Matrix const &)
{
    return true;
}

void FilterComponentTransfer::area_enlarge(NRRectL &/*area*/, Geom::Matrix const &/*trans*/)
{
}

} /* namespace Filters */
} /* namespace Inkscape */

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

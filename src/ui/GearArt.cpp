// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "GearArt.h"

#if RIFFHOUSE_HAS_GEAR_ART
 #include <GearData.h>
#endif

#include <map>

namespace rh::ui::art
{
namespace
{
    const void* resource (const juce::String& name, const char* ext, int& size)
    {
        size = 0;
       #if RIFFHOUSE_HAS_GEAR_ART
        return RiffhouseGear::getNamedResource ((name + "_" + ext).toRawUTF8(), size);
       #else
        juce::ignoreUnused (name, ext);
        return nullptr;
       #endif
    }

    // A picture is stored as a JPEG for its colour and a grey PNG for how solid each pixel is,
    // which is a fifth of the size of one PNG. Small parts are plain PNGs.
    juce::Image load (const juce::String& name)
    {
        int size = 0;
        if (auto* png = resource (name, "png", size))
        {
            auto img = juce::ImageFileFormat::loadFrom (png, (size_t) size);
            return img.isValid() ? img.convertedToFormat (juce::Image::ARGB) : img;
        }
        auto* jpg = resource (name, "jpg", size);
        if (jpg == nullptr) return {};
        auto colour = juce::ImageFileFormat::loadFrom (jpg, (size_t) size);
        int maskSize = 0;
        auto* maskData = resource (name + "_a", "png", maskSize);
        if (! colour.isValid()) return {};
        if (maskData == nullptr) return colour.convertedToFormat (juce::Image::ARGB); // solid all over: a strip of frames
        auto mask = juce::ImageFileFormat::loadFrom (maskData, (size_t) maskSize);
        if (! mask.isValid() || mask.getWidth() != colour.getWidth() || mask.getHeight() != colour.getHeight()) return {};
        mask = mask.convertedToFormat (juce::Image::ARGB);
        colour = colour.convertedToFormat (juce::Image::RGB);
        juce::Image out (juce::Image::ARGB, colour.getWidth(), colour.getHeight(), false);
        juce::Image::BitmapData c (colour, juce::Image::BitmapData::readOnly), m (mask, juce::Image::BitmapData::readOnly),
                                o (out, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < out.getHeight(); ++y)
        {
            auto* cp = c.getLinePointer (y);
            auto* mp = (const juce::PixelARGB*) m.getLinePointer (y);
            auto* op = (juce::PixelARGB*) o.getLinePointer (y);
            for (int x = 0; x < out.getWidth(); ++x)
            {
                const auto* px = (const juce::PixelRGB*) (cp + x * c.pixelStride);
                const juce::uint8 a = mp[x].getGreen();
                op[x].setARGB (a, px->getRed(), px->getGreen(), px->getBlue());
                op[x].premultiply();
            }
        }
        return out;
    }

    struct Store
    {
        struct Entry { juce::Image image; juce::uint32 used = 0; };
        std::map<juce::String, Entry> full;                         // full size: only the last few are kept
        std::map<juce::String, juce::Image> small;                  // name@WxH: shrunk copies
        juce::uint32 clock = 0;
        static constexpr size_t keepFull = 6, keepSmall = 160;
    };
    Store& store()
    {
        static Store s;
        return s;
    }
}

bool has (const juce::String& name)
{
    int size = 0;
    return resource (name, "jpg", size) != nullptr || resource (name, "png", size) != nullptr;
}

juce::Image picture (const juce::String& name)
{
    auto& s = store();
    if (auto it = s.full.find (name); it != s.full.end())
    {
        it->second.used = ++s.clock;
        return it->second.image;
    }
    auto img = load (name);
    if (! img.isValid()) return {};
    if (s.full.size() >= Store::keepFull)
    {
        auto oldest = s.full.begin();
        for (auto it = s.full.begin(); it != s.full.end(); ++it)
            if (it->second.used < oldest->second.used) oldest = it;
        s.full.erase (oldest);
    }
    s.full[name] = { img, ++s.clock };
    return img;
}

juce::Image shrunk (const juce::Image& source, int width, int height)
{
    width = juce::jmax (1, width);
    height = juce::jmax (1, height);
    if (! source.isValid() || (source.getWidth() == width && source.getHeight() == height)) return source;
    const auto src = source.convertedToFormat (juce::Image::ARGB);
    const int sw = src.getWidth(), sh = src.getHeight();
    if (width > sw || height > sh) return src.rescaled (width, height, juce::Graphics::highResamplingQuality);

    // box filter, across then down: each new pixel is the average of the strip of old ones under it
    std::vector<float> rows ((size_t) width * (size_t) sh * 4);
    {
        juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
        const double step = (double) sw / width;
        for (int y = 0; y < sh; ++y)
        {
            auto* p = (const juce::PixelARGB*) in.getLinePointer (y);
            auto* o = rows.data() + (size_t) y * (size_t) width * 4;
            for (int x = 0; x < width; ++x)
            {
                const double a = x * step, b = (x + 1) * step;
                double acc[4] = { 0, 0, 0, 0 };
                for (int i = (int) a; i < sw && (double) i < b; ++i)
                {
                    const double wgt = std::min (b, (double) i + 1.0) - std::max (a, (double) i);
                    acc[0] += wgt * p[i].getAlpha(); acc[1] += wgt * p[i].getRed();
                    acc[2] += wgt * p[i].getGreen(); acc[3] += wgt * p[i].getBlue();
                }
                for (int k = 0; k < 4; ++k) o[x * 4 + k] = (float) (acc[k] / step);
            }
        }
    }
    juce::Image out (juce::Image::ARGB, width, height, true);
    {
        juce::Image::BitmapData o (out, juce::Image::BitmapData::writeOnly);
        const double step = (double) sh / height;
        for (int y = 0; y < height; ++y)
        {
            auto* op = (juce::PixelARGB*) o.getLinePointer (y);
            const double a = y * step, b = (y + 1) * step;
            for (int x = 0; x < width; ++x)
            {
                double acc[4] = { 0, 0, 0, 0 };
                for (int i = (int) a; i < sh && (double) i < b; ++i)
                {
                    const double wgt = std::min (b, (double) i + 1.0) - std::max (a, (double) i);
                    const float* r = rows.data() + ((size_t) i * (size_t) width + (size_t) x) * 4;
                    for (int k = 0; k < 4; ++k) acc[k] += wgt * r[k];
                }
                auto v = [&] (int k) { return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (acc[k] / step)); };
                const auto alpha = v (0);
                op[x].setARGB (alpha, std::min (alpha, v (1)), std::min (alpha, v (2)), std::min (alpha, v (3))); // already premultiplied
            }
        }
    }
    return out;
}

namespace
{
    // The copy of `name` that fits `pixelsWide`: sizes go down in steps so that a window being
    // dragged, or two knobs a pixel apart in size, share one copy.
    juce::Image fitted (const juce::String& name, float pixelsWide, float aspect)
    {
        auto& s = store();
        int w = juce::jmax (8, (int) std::ceil (pixelsWide));
        w = (w + 7) / 8 * 8;
        const auto key = name + "@" + juce::String (w);
        if (auto it = s.small.find (key); it != s.small.end()) return it->second;
        auto full = picture (name);
        if (! full.isValid()) return {};
        auto img = w >= full.getWidth() ? full : shrunk (full, w, juce::jmax (1, juce::roundToInt ((float) w * aspect)));
        if (s.small.size() >= Store::keepSmall) s.small.clear();
        s.small[key] = img;
        return img;
    }
}

bool draw (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> dest, float opacity)
{
    if (! has (name)) return false;
    const auto full = picture (name);
    if (! full.isValid()) return false;
    const float aspect = (float) full.getHeight() / (float) full.getWidth();
    const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto img = fitted (name, dest.getWidth() * scale, aspect);
    if (! img.isValid()) return false;
    juce::Graphics::ScopedSaveState state (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (opacity);
    g.drawImage (img, dest);
    return true;
}

bool drawFrame (juce::Graphics& g, const juce::String& name, int frame, int frames, int columns, juce::Rectangle<float> dest)
{
    if (! has (name)) return false;
    const auto strip = picture (name);
    if (! strip.isValid()) return false;
    const int rows = (frames + columns - 1) / columns;
    const int fw = strip.getWidth() / columns, fh = strip.getHeight() / rows;
    frame = juce::jlimit (0, frames - 1, frame);
    juce::Graphics::ScopedSaveState state (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (1.0f);
    g.drawImage (strip, juce::roundToInt (dest.getX()), juce::roundToInt (dest.getY()), juce::roundToInt (dest.getWidth()), juce::roundToInt (dest.getHeight()),
                 (frame % columns) * fw, (frame / columns) * fh, fw, fh);
    return true;
}

juce::String slug (const juce::String& s)
{
    juce::String out;
    for (auto ch : s.toLowerCase())
        out << (juce::CharacterFunctions::isLetterOrDigit (ch) ? juce::String::charToString (ch) : juce::String ("_"));
    return out;
}
} // namespace rh::ui::art

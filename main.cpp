#include <algorithm>
#include <cassert>
#include <complex>
#include <iostream>
#include <memory>
#include <vector>

#include <sixel/sixel.h>

using namespace std;
using namespace std::complex_literals;

struct ImageSize
{
    int width;
    int height;

    constexpr size_t area() const noexcept
    {
        return static_cast<size_t>(width) * static_cast<size_t>(height);
    }
};

struct Point
{
    int x;
    int y;
};

struct RGBColor
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    constexpr uint8_t& operator[](int i) noexcept { return *(&red + i); }
};

struct HSVColor
{
    double hue;
    double saturation;
    double value;

    constexpr double& operator[](int i) noexcept { return *(&hue + i); }
};

// {{{ hsv2rgb
constexpr RGBColor hsv2rgb(HSVColor hsv) noexcept
{
    assert(0.0 <= hsv.hue && hsv.hue <= 1.0);
    assert(0.0 <= hsv.saturation && hsv.saturation <= 1.0);
    assert(0.0 <= hsv.value && hsv.value <= 1.0);

    double r = 0, g = 0, b = 0;

    if (hsv.saturation == 0)
    {
        r = hsv.value;
        g = hsv.value;
        b = hsv.value;
    }
    else
    {
        if (hsv.hue == 360)
            hsv.hue = 0;
        else
            hsv.hue = hsv.hue / 60;

        auto const i = static_cast<int>(trunc(hsv.hue));
        double const f = hsv.hue - i;
        double const p = hsv.value * (1.0 - hsv.saturation);
        double const q = hsv.value * (1.0 - (hsv.saturation * f));
        double const t = hsv.value * (1.0 - (hsv.saturation * (1.0 - f)));

        switch (i)
        {
        case 0:
            r = hsv.value;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = hsv.value;
            b = p;
            break;
        case 2:
            r = p;
            g = hsv.value;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = hsv.value;
            break;
        case 4:
            r = t;
            g = p;
            b = hsv.value;
            break;
        default:
            r = hsv.value;
            g = p;
            b = q;
            break;
        }
    }

    // auto const v = RGBColor { static_cast<unsigned char>(r * 255.0),
    //                   static_cast<unsigned char>(g * 255.0),
    //                   static_cast<unsigned char>(b * 255.0) };
    // printf("hsv2rgb: #%02x%02x%02x\n", v.red, v.green, v.blue);
    return RGBColor { static_cast<unsigned char>(r * 255.0),
                      static_cast<unsigned char>(g * 255.0),
                      static_cast<unsigned char>(b * 255.0) };
}
// }}}

// RGB canvas
struct Canvas
{
    ImageSize size;
    vector<uint8_t> pixels;

    explicit Canvas(ImageSize _size): size { _size }
    {
        pixels.resize(size.area() * 3);
        fill(begin(pixels), end(pixels), 0xFF); // fill color: white
    }

    void write(Point p, RGBColor color)
    {
        if (!(0 <= p.x && p.x < size.width && 0 <= p.y && p.y < size.height))
            return;

        auto* pixel = &pixels[p.y * size.width * 3 + p.x * 3];
        pixel[0] = color.red;
        pixel[1] = color.green;
        pixel[2] = color.blue;
    }
};

template <typename F>
void paint_complex(Canvas& canvas, double xRange, double yRange, F f)
{
    auto const threshold = 0.1;

    // FYI: https://www.algorithm-archive.org/contents/domain_coloring/domain_coloring.html
    //
    // angle(x,y) := (pi + atan2(-y, -x)) / (2*pi)
    // theta(x,y) := atan2(y, x)
    // r(x,y)     := sqrt(x*x + y*y)
    // z(x,y)     := r(x, y) * exp(theta(x, y) * sqrt(-1))
    //
    // magnitude_shading(x,y) := 0.5 + 0.5*(abs(f(z(x,y)))-floor(abs(f(z(x,y)))))
    //
    // gridlines(x,y) := abs(sin(real_f(z(x,y))*pi)**threshold) *
    //                   abs(sin(imaginary_f(z(x,y))*pi))**threshold
    //
    // color(x,y) := hsv2rgb(angle(real_f(z(x,y)), imaginary_f(z(x,y))),
    //                       magnitude_shading(x,y),
    //                       gridlines(x,y))
    //
    // imaginary_f(z) := imag(f(z))
    // real_f(z)      := real(f(z))

    auto const r = [](auto x, auto y) {
        return sqrt(x * x + y * y);
    };
    auto const theta = [](auto x, auto y) {
        return atan2(y, x);
    };
    auto const z = [&](auto x, auto y) {
        return r(x, y) * exp(theta(x, y) * 1i);
    };
    auto const magnitude_shading = [&](auto x, auto y) {
        return 0.5 + 0.5 * (abs(f(z(x, y))) - floor(abs(f(z(x, y)))));
    };
    auto const real_f = [&f](auto z) {
        return f(z).real();
    };
    auto const imaginary_f = [&f](auto z) {
        return f(z).imag();
    };

    auto const gridlines = [&](auto x, auto y) {
        auto const value = abs(pow(sin(real_f(z(x, y)) * M_PI), threshold))
                           * pow(abs(sin(imaginary_f(z(x, y)) * M_PI)), threshold);
        if (isnan(abs(value)) || isinf(abs(value)))
            return 1.0;
        return value;
    };

    auto const angle = [](auto x, auto y) {
        return (M_PI + atan2(-y, -x)) / (2 * M_PI);
    };

    auto const color = [&](auto x, auto y) {
        auto const hsv = HSVColor { angle(real_f(z(x, y)), imaginary_f(z(x, y))),
                                    magnitude_shading(x, y),
                                    gridlines(x, y) };
        return hsv2rgb(hsv);
    };

    auto const w = double(canvas.size.width);
    auto const h = double(canvas.size.height);

    for (int y = 0; y < canvas.size.height; ++y)
    {
        auto const yf = ((double(y) / h) - 0.5) * yRange;
        for (int x = 0; x < canvas.size.width; ++x)
        {
            auto const xf = ((double(x) / w) - 0.5) * xRange;
            canvas.write(Point { x, y }, color(xf, yf));
        }
    }
}

int sixelWriter(char* data, int size, void* priv)
{
    cout.write(data, size);
    return size;
};

template <typename F>
void complex_plot(ImageSize imageSize, double xRange, double yRange, F f)
{
    auto canvas = Canvas(imageSize);
    paint_complex(canvas, xRange, yRange, move(f));

    sixel_output_t* output = nullptr;
    sixel_output_new(&output, &sixelWriter, nullptr, nullptr);
    sixel_dither_t* dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);

    sixel_encode(canvas.pixels.data(), imageSize.width, imageSize.height, 3, dither, output);

    sixel_dither_unref(dither);
    sixel_output_destroy(output);
}

int main(int argc, char const* argv[])
{
    auto const canvasSize = ImageSize { 400, 400 };
    auto const xRange = 4.0; // Ranges from minus N to plus N, inclusive.
    auto const yRange = 4.0;

    cout << "\t";
    complex_plot(canvasSize, xRange, yRange, [](auto z) { return z; });
    cout << "f(z) := z\n\n\t";

    complex_plot(canvasSize, xRange, yRange, [](auto z) { return z * z; });
    cout << "f(z) := z*z\n";

    return EXIT_SUCCESS;
}

#include <algorithm>
#include <complex>
#include <iostream>
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
    uint8_t hue;
    uint8_t saturation;
    uint8_t value;

    constexpr uint8_t& operator[](int i) noexcept { return *(&hue + i); }
};

// {{{ hsv2rgb
RGBColor hsv2rgb(HSVColor hsv)
{
    RGBColor rgb {};
    unsigned char region, remainder, p, q, t;

    if (hsv.saturation == 0)
    {
        rgb.red = hsv.value;
        rgb.green = hsv.value;
        rgb.blue = hsv.value;
        return rgb;
    }

    region = hsv.hue / 43;
    remainder = (hsv.hue - (region * 43)) * 6;

    p = (hsv.value * (255 - hsv.saturation)) >> 8;
    q = (hsv.value * (255 - ((hsv.saturation * remainder) >> 8))) >> 8;
    t = (hsv.value * (255 - ((hsv.saturation * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
    case 0:
        rgb.red = hsv.value;
        rgb.green = t;
        rgb.blue = p;
        break;
    case 1:
        rgb.red = q;
        rgb.green = hsv.value;
        rgb.blue = p;
        break;
    case 2:
        rgb.red = p;
        rgb.green = hsv.value;
        rgb.blue = t;
        break;
    case 3:
        rgb.red = p;
        rgb.green = q;
        rgb.blue = hsv.value;
        break;
    case 4:
        rgb.red = t;
        rgb.green = p;
        rgb.blue = hsv.value;
        break;
    default:
        rgb.red = hsv.value;
        rgb.green = p;
        rgb.blue = q;
        break;
    }

    return rgb;
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

    auto const w = double(canvas.size.width);
    auto const h = double(canvas.size.height);

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

    auto const angle = [](auto x, auto y) {
        return (M_PI + atan2(-y, -x)) / (2 * M_PI);
    };

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
        // clang-format off
        return abs(pow(sin(real_f(z(x, y)) * M_PI), threshold))
             * pow(abs(sin(imaginary_f(z(x, y)) * M_PI)), threshold);
        // clang-format on
    };

    auto const color = [&](auto x, auto y) {
        // clang-format off
        auto const hsv = HSVColor {
            // (uint8_t) clamp(angle(real_f(z(x, y)), imaginary_f(z(x, y))) * 255.0, 0.0, 255.0),
            // (uint8_t) clamp(magnitude_shading(x, y) * 255.0, 0.0, 255.0),
            // (uint8_t) clamp(gridlines(x, y) * 255.0, 0.0, 255.0)
            (uint8_t) clamp(angle(real_f(z(x, y)), imaginary_f(z(x, y))) * 255.0, 0.0, 255.0),
            (uint8_t) clamp(magnitude_shading(x, y) * 255.0, 0.0, 255.0),
            (uint8_t) clamp(gridlines(x, y) * 255.0, 0.0, 255.0)
        };
        return hsv2rgb(hsv);
        // clang-format on
    };

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
    sixel_output_set_encode_policy(output, SIXEL_ENCODEPOLICY_AUTO);

    sixel_dither_t* dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);
    sixel_encode(canvas.pixels.data(), imageSize.width, imageSize.height, 3, dither, output);

    sixel_output_destroy(output);
}

int main(int argc, char const* argv[])
{
    auto const canvasSize = ImageSize { 400, 400 };

    cout << "\t";
    complex_plot(canvasSize, 4.0f, 4.0f, [](complex<double> z) { return z; });
    cout << "f(z) := z\n\n\t";

    complex_plot(canvasSize, 4.0f, 4.0f, [](complex<double> z) { return z * z; });
    cout << "f(z) := z*z\n";

    return EXIT_SUCCESS;
}

#include <algorithm>
#include <cassert>
#include <complex>
#include <iostream>
#include <memory>
#include <vector>

#include <sixel/sixel.h>

using std::abs;
using std::cout;
using std::isinf;
using std::isnan;
using std::move;
using std::vector;

using namespace std::complex_literals;

template <typename T, const T Min, const T Max>
struct domain_value
{
    T value;

    constexpr static T clamped(T v) noexcept { return std::clamp(v, Min, Max); }

    // clang-format off
    constexpr domain_value& operator=(T v) { value = clamped(v); return *this; }
    // clang-format on

    constexpr domain_value(T v): value { clamped(v) } {}
    constexpr operator T() const noexcept { return value; }
    constexpr T operator()() const noexcept { return value; }
    constexpr domain_value operator+(domain_value other) const noexcept { return { value + other.value }; }
    constexpr domain_value operator-(domain_value other) const noexcept { return { value - other.value }; }
    constexpr domain_value operator*(domain_value other) const noexcept { return { value * other.value }; }
    constexpr domain_value operator/(domain_value other) const noexcept { return { value / other.value }; }
    constexpr domain_value operator%(domain_value other) const noexcept { return { value % other.value }; }
};

template <typename T>
using norm = domain_value<T, 0.0, 1.0>;

template <typename T>
using degree = domain_value<T, 0.0, 360.0>;

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
    degree<double> hue;
    norm<double> saturation;
    norm<double> value;
};

constexpr RGBColor hsv2rgb(HSVColor hsv) noexcept
{
    // Implementation from:
    // https://www.codegrepper.com/code-examples/cpp/c%2B%2B+hsl+to+rgb+integer

    double r = 0, g = 0, b = 0;

    if (hsv.saturation() == 0)
    {
        r = hsv.value();
        g = hsv.value();
        b = hsv.value();
    }
    else
    {
        if (hsv.hue() == 360)
            hsv.hue = 0;
        else
            hsv.hue = hsv.hue() / 60;

        auto const i = static_cast<int>(trunc(hsv.hue()));
        auto const f = hsv.hue() - i;
        auto const p = hsv.value() * (1.0 - hsv.saturation());
        auto const q = hsv.value() * (1.0 - (hsv.saturation() * f));
        auto const t = hsv.value() * (1.0 - (hsv.saturation() * (1.0 - f)));

        switch (i)
        {
        case 0:
            r = hsv.value();
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = hsv.value();
            b = p;
            break;
        case 2:
            r = p;
            g = hsv.value();
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = hsv.value();
            break;
        case 4:
            r = t;
            g = p;
            b = hsv.value();
            break;
        default:
            r = hsv.value();
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

struct RGBCanvas
{
    ImageSize size;
    vector<uint8_t> pixels;

    explicit RGBCanvas(ImageSize _size): size { _size }
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

RGBColor color(double x, double y)
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
        auto const angleRadians = (M_PI + atan2(-y, -x)) / (2 * M_PI);
        return angleRadians * (180.0 / M_PI);
    };

    auto const hsv = HSVColor { angle(real_f(z(x, y)), imaginary_f(z(x, y))),
                                magnitude_shading(x, y),
                                gridlines(x, y) };
    // printf("hsv(%f, %f, %f)\n", hsv.hue(), hsv.saturation(), hsv.value());
    return hsv2rgb(hsv);
}

template <typename F, typename Colorizer>
void paint_complex(RGBCanvas& canvas, double xRange, double yRange, F f, Colorizer color)
{

    auto const w = static_cast<double>(canvas.size.width);
    auto const h = static_cast<double>(canvas.size.height);

    auto const xExtent = xRange * 2;
    auto const yExtent = yRange * 2;

    for (int y = 0; y < canvas.size.height; ++y)
    {
        auto const yf = ((static_cast<double>(y) / h) - 0.5) * yExtent;
        for (int x = 0; x < canvas.size.width; ++x)
        {
            auto const xf = ((static_cast<double>(x) / w) - 0.5) * xExtent;
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
    auto canvas = RGBCanvas(imageSize);
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
    auto const xRange = 2; // Ranges from minus N to plus N, inclusive.
    auto const yRange = 2;

    cout << "\t";
    complex_plot(canvasSize, xRange, yRange, [](auto z) { return z; });
    cout << "f(z) := z\n\n\t";

    complex_plot(canvasSize, xRange, yRange, [](auto z) { return z * z; });
    cout << "f(z) := z*z\n";

    return EXIT_SUCCESS;
}

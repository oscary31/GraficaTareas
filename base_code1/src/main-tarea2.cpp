// Oscary Arocha
// C.I: 30.697.617

#include "PixelRender.h"
#include <iostream>
#include <vector>
#include <random>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>


class CMyTest : public CPixelRender
{
private:
    int m_x0 = -1;
    int m_y0 = -1;
    int m_x1 = -1;
    int m_y1 = -1;
    RGBA m_currentColor = { 255,255,255,255 }; // Color blanco
    bool m_useRealArithmetic = false; // false = Bresenham, true = aritmética real
    bool m_drawEllipseMode = false; // false = lines, true = ellipses
    bool m_useEllipse2 = true; // false = drawEllipse1, true = drawEllipse2

    struct Line {
        int x0, y0, x1, y1;
        RGBA color;
    };

    struct Ellipse {
        int cx, cy; // center
        int a, b; // radii
        RGBA color;
    };

    // Estructura para parámetros usados en la prueba de rendimiento
    struct PerfEllipse {
        int cx, cy, a, b;
        RGBA color;
    };

    std::vector<Line> m_lines; // lista de líneas
    std::vector<Ellipse> m_ellipses; // lista de elipses
    int framesThisSecond = 0; // contador de frames para calcular FPS
    int m_totalGeneratedLines = 0; // total de líneas generadas por botón

    // Resultado de la prueba de igualdad de elipses (para mostrar en UI)
    std::string m_testResult;
    std::vector<std::pair<int,int>> m_testMismatchesSample; // sample de pixeles distintos

    // Para la prueba de rendimiento
    std::vector<PerfEllipse> m_lastPerfEllipses; // guarda lista generada para el último N
    std::string m_perfCsvFile = "ellipse_perf_results.csv";
    std::string m_perfStatus; // estado o mensaje para mostrar en UI

public:
    CMyTest() {};
    ~CMyTest() {};

    // Algoritmo con aritmética real 
    void drawLineReal(int x0, int y0, int x1, int y1, RGBA color)
    {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int absDx = abs(dx);
        int absDy = abs(dy);

        // Caso 1: 0 <= m < 1
        if (absDy < absDx && dx > 0 && dy >= 0)
        {
            float m = static_cast<float>(dy) / static_cast<float>(dx);
            float y = static_cast<float>(y0);
            for (int x = x0; x <= x1; x++)
            {
                setPixel(x, static_cast<int>(std::round(y)), color);
                y += m;
            }
        }
        // Caso 2: m >= 1
        else if (absDy >= absDx && dx >= 0 && dy > 0)
        {
            float m = static_cast<float>(dx) / static_cast<float>(dy);
            float x = static_cast<float>(x0);
            for (int y = y0; y <= y1; y++)
            {
                setPixel(static_cast<int>(std::round(x)), y, color);
                x += m;
            }
        }
        // Caso 3: -1 < m <= 0
        else if (absDy < absDx && dx > 0 && dy < 0)
        {
            float m = static_cast<float>(dy) / static_cast<float>(dx);
            float y = static_cast<float>(y0);
            for (int x = x0; x <= x1; x++)
            {
                setPixel(x, static_cast<int>(std::round(y)), color);
                y += m;
            }
        }
        // Caso 4: m <= -1
        else if (absDy >= absDx && dx >= 0 && dy < 0)
        {
            float m = static_cast<float>(dx) / static_cast<float>(dy);
            float x = static_cast<float>(x0);
            for (int y = y0; y >= y1; y--)
            {
                setPixel(static_cast<int>(std::round(x)), y, color);
                x -= m;
            }
        }
        // Casos con dx < 0: intercambiamos puntos
        else if (dx < 0)
        {
            drawLineReal(x1, y1, x0, y0, color);
        }
    }

    // Algoritmo de Bresenham
    void drawLineBresenham(int x0, int y0, int x1, int y1, RGBA color)
    {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int absDx = abs(dx);
        int absDy = abs(dy);

        // Caso 1: 0 <= m < 1 
        if (absDy < absDx && dx > 0 && dy >= 0)
        {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incNE = 2 * (absDx - absDy);
            int x = x0, y = y0;

            setPixel(x, y, color);
            while (x < x1)
            {
                if (d <= 0)
                {
                    d += incNE;
                    y++;
                }
                else
                {
                    d += incE;
                }
                x++;
                setPixel(x, y, color);
            }
        }
        // Caso 2: m >= 1 
        else if (absDy >= absDx && dx >= 0 && dy > 0)
        {
            int d = absDy - 2 * absDx;
            int incN = -2 * absDx;
            int incNE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setPixel(x, y, color);
            while (y < y1)
            {
                if (d <= 0)
                {
                    d += incNE;
                    x++;
                }
                else
                {
                    d += incN;
                }
                y++;
                setPixel(x, y, color);
            }
        }
        // Caso 3: -1 < m <= 0 
        else if (absDy < absDx && dx > 0 && dy < 0)
        {
            int d = absDx - 2 * absDy;
            int incE = -2 * absDy;
            int incSE = 2 * (absDx - absDy);
            int x = x0, y = y0;

            setPixel(x, y, color);
            while (x < x1)
            {
                if (d <= 0)
                {
                    d += incSE;
                    y--;
                }
                else
                {
                    d += incE;
                }
                x++;
                setPixel(x, y, color);
            }
        }
        // Caso 4: m <= -1 
        else if (absDy >= absDx && dx >= 0 && dy < 0)
        {
            int d = absDy - 2 * absDx;
            int incS = -2 * absDx;
            int incSE = 2 * (absDy - absDx);
            int x = x0, y = y0;

            setPixel(x, y, color);
            while (y > y1)
            {
                if (d <= 0)
                {
                    d += incSE;
                    x++;
                }
                else
                {
                    d += incS;
                }
                y--;
                setPixel(x, y, color);
            }
        }
        // Casos con dx < 0
        else if (dx < 0)
        {
            drawLineBresenham(x1, y1, x0, y0, color);
        }
    }

    // Función que decide qué algoritmo usar
    void drawLine(int x0, int y0, int x1, int y1, RGBA color)
    {
        if (m_useRealArithmetic)
            drawLineReal(x0, y0, x1, y1, color);
        else
            drawLineBresenham(x0, y0, x1, y1, color);
    }

    // Dibuja 4 puntos simétricos de la elipse centrada en (cx,cy)
    void EllipsePoints4(long long cx, long long cy, long long x, long long y, RGBA color)
    {
        setPixel(static_cast<int>(cx + x), static_cast<int>(cy + y), color);
        setPixel(static_cast<int>(cx - x), static_cast<int>(cy + y), color);
        setPixel(static_cast<int>(cx + x), static_cast<int>(cy - y), color);
        setPixel(static_cast<int>(cx - x), static_cast<int>(cy - y), color);
    }

    // Helper: encode point into 64-bit key
    static inline uint64_t encodePoint(int x, int y) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
    }

    // Collecting version of EllipsePoints4 (without setPixel)
    static void EllipsePoints4_collect(long long cx, long long cy, long long x, long long y, std::unordered_set<uint64_t>& out)
    {
        out.insert(encodePoint(static_cast<int>(cx + x), static_cast<int>(cy + y)));
        out.insert(encodePoint(static_cast<int>(cx - x), static_cast<int>(cy + y)));
        out.insert(encodePoint(static_cast<int>(cx + x), static_cast<int>(cy - y)));
        out.insert(encodePoint(static_cast<int>(cx - x), static_cast<int>(cy - y)));
    }

    // drawEllipse1 using long long 
    void drawEllipse1(int xc, int yc, int a, int b, RGBA color)
    {
        int x = 0;
        int y = b;

        long long aa = static_cast<long long>(a) * a;
        long long bb = static_cast<long long>(b) * b;

        // Modalidad 1: Cuando la pendiente está en [-1, 0]
        // d = b²(4x²+4x+1) + a²(4y²-8y+4) - 4a²b²
        long long d = bb * (4 * 0 * 0 + 4 * 0 + 1) + aa * (4 * b * b - 8 * b + 4) - 4 * aa * bb;

        // Dibuja 4 puntos simétricos de la elipse en el primer paso
        EllipsePoints4(xc, yc, x, y, color);

        // Ciclo de la Modalidad 1: mientras 2b²(x+1) < 2a²(y-0.5) equivale a b²*2*(x+1) < a*a*(2*y-1)
        while (bb * 2 * (x + 1) < aa * (2 * y - 1))
        {
            if (d < 0)
            {
                // Tomar E(x+1, y)
                d = d + 4 * (bb * (2 * x + 3));
            }
            else
            {
                // Tomar SE(x+1, y-1)
                d = d + 4 * (bb * (2 * x + 3) + aa * (-2 * y + 2));
                y--;
            }
            x++;
            EllipsePoints4(xc, yc, x, y, color);
        }

        // Modalidad 2: Cuando la pendiente está entre (-∞, -1]
        // Reinicializar d para la Modalidad 2
        d = bb * (4 * x * x + 4 * x + 1) + aa * (4 * y * y - 8 * y + 4) - 4 * aa * bb;

        // Ciclo de la Modalidad 2: mientras y > 0
        while (y > 0)
        {
            if (d < 0)
            {
                // Tomar SE(x+1, y-1)
                d = d + 4 * (bb * (2 * x + 2) + aa * (-2 * y + 3));
                x++;
            }
            else
            {
                // Tomar S(x, y-1)
                d = d + 4 * aa * (-2 * y + 3);
            }
            y--;
            EllipsePoints4(xc, yc, x, y, color);
        }
    }

    
    void collectEllipsePixels1(int cx, int cy, int a, int b, std::unordered_set<uint64_t>& out)
    {
        long long x = 0;
        long long y = b;

        long long aa = static_cast<long long>(a) * a;
        long long bb = static_cast<long long>(b) * b;

        // Modalidad 1: Cuando la pendiente está en [-1, 0]
        // d = b²(4x²+4x+1) + a²(4y²-8y+4) - 4a²b²
        long long d = bb * (4 * 0 * 0 + 4 * 0 + 1) + aa * (4 * b * b - 8 * b + 4) - 4 * aa * bb;

        // Recolecta 4 puntos simétricos de la elipse en el primer paso
        EllipsePoints4_collect(cx, cy, x, y, out);

        // Ciclo de la Modalidad 1: mientras 2b²(x+1) < 2a²(y-0.5) equivale a b²*2*(x+1) < a*a*(2*y-1)
        while (bb * 2 * (x + 1) < aa * (2 * y - 1))
        {
            if (d < 0)
            {
                // Tomar E(x+1, y)
                d = d + 4 * (bb * (2 * x + 3));
            }
            else
            {
                // Tomar SE(x+1, y-1)
                d = d + 4 * (bb * (2 * x + 3) + aa * (-2 * y + 2));
                y--;
            }
            x++;
            EllipsePoints4_collect(cx, cy, x, y, out);
        }

        // Modalidad 2: Cuando la pendiente está entre (-∞, -1]
        // Reinicializar d para la Modalidad 2
        d = bb * (4 * x * x + 4 * x + 1) + aa * (4 * y * y - 8 * y + 4) - 4 * aa * bb;

        // Ciclo de la Modalidad 2: mientras y > 0
        while (y > 0)
        {
            if (d < 0)
            {
                // Tomar SE(x+1, y-1)
                d = d + 4 * (bb * (2 * x + 2) + aa * (-2 * y + 3));
                x++;
            }
            else
            {
                // Tomar S(x, y-1)
                d = d + 4 * aa * (-2 * y + 3);
            }
            y--;
            EllipsePoints4_collect(cx, cy, x, y, out);
        }
    }

    // drawEllipse2 using long long integer arithmetic
    void drawEllipse2(int cx, int cy, int a, int b, RGBA color)
    {
        if (a <= 0 || b <= 0) return;

        long long a2 = static_cast<long long>(a) * static_cast<long long>(a);
        long long b2 = static_cast<long long>(b) * static_cast<long long>(b);

        long long x = 0;
        long long y = b;

        long long dx = 2 * b2 * x;
        long long dy = 2 * a2 * y;

        long long d1 = b2 - a2 * b + (a2 + 3) / 4; // slightly different init

        long long twoB2 = 2 * b2;
        long long twoA2 = 2 * a2;

        while (dx < dy) {
            EllipsePoints4(cx, cy, x, y, color);
            if (d1 < 0) {
                x += 1;
                dx += twoB2;
                d1 += dx + b2;
            } else {
                x += 1;
                y -= 1;
                dx += twoB2;
                dy -= twoA2;
                d1 += dx - dy + b2;
            }
        }

        long long d2_num_x = (2 * x + 1);
        long long d2 = b2 * (d2_num_x * d2_num_x) / 4 + a2 * (y - 1) * (y - 1) - a2 * b2;
        while (y >= 0) {
            EllipsePoints4(cx, cy, x, y, color);
            if (d2 > 0) {
                y -= 1;
                dy -= twoA2;
                d2 += a2 - dy;
            } else {
                y -= 1;
                x += 1;
                dx += twoB2;
                dy -= twoA2;
                d2 += dx - dy + a2;
            }
        }
    }

    // collectEllipsePixels2 using long long integer arithmetic
    void collectEllipsePixels2(int cx, int cy, int a, int b, std::unordered_set<uint64_t>& out)
    {
        if (a <= 0 || b <= 0) return;

        long long a2 = static_cast<long long>(a) * static_cast<long long>(a);
        long long b2 = static_cast<long long>(b) * static_cast<long long>(b);

        long long x = 0;
        long long y = b;

        long long dx = 2 * b2 * x;
        long long dy = 2 * a2 * y;

        long long d1 = b2 - a2 * b + (a2 + 3) / 4;

        long long twoB2 = 2 * b2;
        long long twoA2 = 2 * a2;

        while (dx < dy) {
            EllipsePoints4_collect(cx, cy, x, y, out);
            if (d1 < 0) {
                x += 1;
                dx += twoB2;
                d1 += dx + b2;
            } else {
                x += 1;
                y -= 1;
                dx += twoB2;
                dy -= twoA2;
                d1 += dx - dy + b2;
            }
        }

        long long d2_num_x = (2 * x + 1);
        long long d2 = b2 * (d2_num_x * d2_num_x) / 4 + a2 * (y - 1) * (y - 1) - a2 * b2;
        while (y >= 0) {
            EllipsePoints4_collect(cx, cy, x, y, out);
            if (d2 > 0) {
                y -= 1;
                dy -= twoA2;
                d2 += a2 - dy;
            } else {
                y -= 1;
                x += 1;
                dx += twoB2;
                dy -= twoA2;
                d2 += dx - dy + a2;
            }
        }
    }

    // Prueba: compara N elipses aleatorias y guarda resultado
    void runEllipseEqualityTest(int tests = 10000)
    {
        m_testMismatchesSample.clear();
        m_testResult.clear();
        std::cout << "\n-----Starting ellipse equality test for " << tests << " random ellipses-----" << std::endl;

        std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<int> distCx(0, width - 1);
        std::uniform_int_distribution<int> distCy(0, height - 1);
        int maxR = std::max(1, std::min(width, height) / 2);
        std::uniform_int_distribution<int> distR(1, maxR);

        bool allEqual = true;
        int mismatchesFound = 0;

        for (int i = 0; i < tests; ++i) {
            int cx = distCx(rng);
            int cy = distCy(rng);
            int a = distR(rng);
            int b = distR(rng);

            std::unordered_set<uint64_t> s1, s2;
            collectEllipsePixels1(cx, cy, a, b, s1);
            collectEllipsePixels2(cx, cy, a, b, s2);

            if (s1 != s2) {
                allEqual = false;
                mismatchesFound++;

                // store up to 50 samples of differing pixels
                for (auto &p : s1) {
                    if (s2.find(p) == s2.end()) {
                        int x = static_cast<int>(static_cast<int32_t>(p >> 32));
                        int y = static_cast<int>(static_cast<int32_t>(p & 0xFFFFFFFF));
                        if (m_testMismatchesSample.size() < 50) m_testMismatchesSample.emplace_back(x,y);
                    }
                }
                for (auto &p : s2) {
                    if (s1.find(p) == s1.end()) {
                        int x = static_cast<int>(static_cast<int32_t>(p >> 32));
                        int y = static_cast<int>(static_cast<int32_t>(p & 0xFFFFFFFF));
                        if (m_testMismatchesSample.size() < 50) m_testMismatchesSample.emplace_back(x,y);
                    }
                }

                std::cout << "Mismatch #" << mismatchesFound << ": center=(" << cx << "," << cy << ") a=" << a << " b=" << b << " PixelsE1=" << s1.size() << " PixelsE2=" << s2.size() << std::endl;

                // Keep searching a few more to be convincing but don't run forever
                if (mismatchesFound >= 10) break;
            }
        }

        std::ostringstream oss;
        if (allEqual) {
            oss << "Ellipse algorithms produced EXACTLY the same pixels for " << tests << " random ellipses.";
            m_testResult = oss.str();
            std::cout << m_testResult << std::endl;
        }
        else {
            oss << "Ellipse algorithms DIFFER.\n";
            m_testResult = oss.str();
            std::cout << m_testResult << std::endl;
        }
    }

    // Prueba de rendimiento: genera N elipses aleatorias, guarda parámetros y mide tiempo de generar píxeles con cada algoritmo. Exporta CSV.
    void runEllipsePerformanceTest(const std::vector<int>& tests)
    {
        m_perfStatus = "Running performance test...";
        std::cout << "\n-----Starting ellipse performance test-----" << std::endl;

        // Construir ruta del CSV en el mismo directorio donde está este archivo fuente
        std::string srcPath = __FILE__;
        size_t pos = srcPath.find_last_of("/\\");
        std::string dir = (pos == std::string::npos) ? std::string() : srcPath.substr(0, pos + 1);
        std::string csvPath = dir + m_perfCsvFile;

        std::ofstream csv(csvPath, std::ofstream::out);
        if (!csv.is_open()) {
            m_perfStatus = "Failed to open CSV file for writing: " + csvPath;
            std::cerr << m_perfStatus << std::endl;
            return;
        }

        csv << "N;time_ms_draw1;total_pixels_draw1;time_ms_draw2;total_pixels_draw2\n";

        std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<int> distCx(0, width - 1);
        std::uniform_int_distribution<int> distCy(0, height - 1);
        int maxR = std::max(1, std::min(width, height) / 2);
        std::uniform_int_distribution<int> distR(1, maxR);
        std::uniform_int_distribution<int> distC(0, 255);

        for (int N : tests) {
            std::cout << "Generating " << N << " random ellipses..." << std::endl;
            // Generar y guardar parámetros en lista
            std::vector<PerfEllipse> list;
            list.reserve(N);
            for (int i = 0; i < N; ++i) {
                PerfEllipse pe;
                pe.cx = distCx(rng);
                pe.cy = distCy(rng);
                pe.a = distR(rng);
                pe.b = distR(rng);
                pe.color = { static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)) };
                list.push_back(pe);
            }

            
            m_lastPerfEllipses = list;

            // Medir drawEllipse1
            std::cout << "Measuring drawEllipse1 for N=" << N << std::endl;
            // limpiar buffer antes de medir
            if (m_buffer.size() > 0)
                std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 0,0,0,0 });

            auto t0 = std::chrono::high_resolution_clock::now();
            for (const auto &pe : list) {
                drawEllipse1(pe.cx, pe.cy, pe.a, pe.b, pe.color);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms1 = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // contar píxeles no nulos en buffer como métrica adicional
            uint64_t totalPixels1 = 0;
            for (const auto &px : m_buffer) {
                if (px.r || px.g || px.b || px.a) totalPixels1++;
            }

            // Medir drawEllipse2
            std::cout << "Measuring drawEllipse2 for N=" << N << std::endl;
            // limpiar buffer antes de medir
            if (m_buffer.size() > 0)
                std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 0,0,0,0 });

            t0 = std::chrono::high_resolution_clock::now();
            for (const auto &pe : list) {
                drawEllipse2(pe.cx, pe.cy, pe.a, pe.b, pe.color);
            }
            t1 = std::chrono::high_resolution_clock::now();
            double ms2 = std::chrono::duration<double, std::milli>(t1 - t0).count();

            uint64_t totalPixels2 = 0;
            for (const auto &px : m_buffer) {
                if (px.r || px.g || px.b || px.a) totalPixels2++;
            }

            std::cout << "\nN=" << N << "\n drawEllipse1 ms=" << ms1 << " totalPixels1=" << totalPixels1 << " drawEllipse2 ms=" << ms2 << " totalPixels2=" << totalPixels2 << std::endl;

            // Formatear los tiempos con decimales usando ostringstream para evitar efectos de formato persistente
            std::ostringstream oss1, oss2;
            oss1 << std::fixed << std::setprecision(3) << ms1;
            oss2 << std::fixed << std::setprecision(3) << ms2;
            csv << N << ";" << oss1.str() << ";" << totalPixels1 << ";" << oss2.str() << ";" << totalPixels2 << "\n";
            csv.flush();
        }

        csv.close();
        m_perfStatus = "\nPerformance test done. CSV saved to: " + csvPath;
        std::cout << m_perfStatus << std::endl;
    }

    

    void update()
    {
        std::fill(m_buffer.begin(), m_buffer.end(), RGBA{ 0,0,0,0 });
        // Contador de frames
        framesThisSecond++;

        // Dibujar todas las líneas guardadas
        for (const auto& ln : m_lines) {
            drawLine(ln.x0, ln.y0, ln.x1, ln.y1, ln.color);
        }

        // Dibujar todas las elipses guardadas
        for (const auto& e : m_ellipses) {
            if (m_useEllipse2)
                drawEllipse2(e.cx, e.cy, e.a, e.b, e.color);
            else
                drawEllipse1(e.cx, e.cy, e.a, e.b, e.color);
        }

        // Dibujar la forma actualmente en creación si ambos puntos están definidos
        if (m_x0 >= 0 && m_y0 >= 0 && m_x1 >= 0 && m_y1 >= 0)
        {
            if (!m_drawEllipseMode) {
                drawLine(m_x0, m_y0, m_x1, m_y1, m_currentColor);
            }
            else {
                int a = std::abs(m_x1 - m_x0);
                int b = std::abs(m_y1 - m_y0);
                if (m_useEllipse2)
                    drawEllipse2(m_x0, m_y0, a, b, m_currentColor);
                else
                    drawEllipse1(m_x0, m_y0, a, b, m_currentColor);
            }
        }
    }

    void onKey(int key, int scancode, int action, int mods)
    {
        if (action == GLFW_PRESS)
        {
            std::cout << "Key " << key << " pressed\n";
            if (key == GLFW_KEY_ESCAPE)
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        else if (action == GLFW_RELEASE)
            std::cout << "Key " << key << " released\n";
    }

    void onMouseButton(int button, int action, int mods)
    {
        if (button >= 0 && button < 3)
        {
            double xpos, ypos;
            glfwGetCursorPos(m_window, &xpos, &ypos);

            if (action == GLFW_PRESS)
            {
                mouseButtonsDown[button] = true;

                // Al presionar el botón izquierdo, iniciar nueva línea/elipse
                if (button == 0)  // Botón izquierdo
                {
                    m_x0 = static_cast<int>(xpos);
                    m_y0 = height - 1 - static_cast<int>(ypos); // Invertir eje Y
                    m_x1 = m_x0;
                    m_y1 = m_y0;
                    std::cout << "Inicio de figura en (" << m_x0 << ", " << m_y0 << ")\n";
                }
            }
            else if (action == GLFW_RELEASE)
            {
                mouseButtonsDown[button] = false;

                // Al liberar el botón, fijar el punto final y almacenar la figura
                if (button == 0)
                {
                    m_x1 = static_cast<int>(xpos);
                    m_y1 = height - 1 - static_cast<int>(ypos); // Invertir eje Y
                    std::cout << "Figura finalizada en (" << m_x1 << ", " << m_y1 << ")\n";

                    if (!m_drawEllipseMode) {
                        // Añadir la línea a la lista de líneas definidas
                        Line ln;
                        ln.x0 = m_x0;
                        ln.y0 = m_y0;
                        ln.x1 = m_x1;
                        ln.y1 = m_y1;
                        ln.color = m_currentColor;
                        m_lines.push_back(ln);
                    }
                    else {
                        // Añadir la elipse a la lista
                        Ellipse el;
                        el.cx = m_x0;
                        el.cy = m_y0;
                        el.a = std::abs(m_x1 - m_x0);
                        el.b = std::abs(m_y1 - m_y0);
                        el.color = m_currentColor;
                        m_ellipses.push_back(el);
                    }
                }
            }
        }
    }

    void onCursorPos(double xpos, double ypos)
    {
        // Si el botón izquierdo está presionado, actualizar el punto final
        if (mouseButtonsDown[0])
        {
            m_x1 = static_cast<int>(xpos);
            m_y1 = height - 1 - static_cast<int>(ypos); // Invertir eje Y
            if (!m_drawEllipseMode)
                std::cout << "Actualizando línea a (" << m_x1 << ", " << m_y1 << ")\n";
            else
                std::cout << "Actualizando elipse a (" << m_x1 << ", " << m_y1 << ")\n";
        }
    }

    // Añadir control de color
    void drawInterface() override
    {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(300, 240), ImGuiCond_Once);
        ImGui::Begin("Control Panel");

        if (ImGui::SliderInt("n-pixels", &m_nPixels, 1, 1000000)) {
            // valor actualizado automáticamente en m_nPixels
        }

        // Checkbox para seleccionar algoritmo
        ImGui::Separator();
        ImGui::Checkbox("Use Real Arithmetic", &m_useRealArithmetic);
        if (!m_useRealArithmetic)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Algorithm: Bresenham");
        else
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Algorithm: Real Arithmetic");

        ImGui::Separator();

        float col[4] = {
            m_currentColor.r / 255.0f,
            m_currentColor.g / 255.0f,
            m_currentColor.b / 255.0f,
            m_currentColor.a / 255.0f
        };
        if (ImGui::ColorEdit4("Line Color", col)) {
            m_currentColor.r = static_cast<unsigned char>(col[0] * 255.0f);
            m_currentColor.g = static_cast<unsigned char>(col[1] * 255.0f);
            m_currentColor.b = static_cast<unsigned char>(col[2] * 255.0f);
            m_currentColor.a = static_cast<unsigned char>(col[3] * 255.0f);
        }

        // Botón para agregar 1000 líneas aleatorias
        if (ImGui::Button("Generate 1000 random lines")) {
            const int generateCount = 1000;
            std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
            std::uniform_int_distribution<int> distX(0, width - 1);
            std::uniform_int_distribution<int> distY(0, height - 1);
            std::uniform_int_distribution<int> distC(0, 255);
            for (int i = 0; i < generateCount; ++i) {
                Line ln;
                ln.x0 = distX(rng);
                ln.y0 = distY(rng);
                ln.x1 = distX(rng);
                ln.y1 = distY(rng);
                ln.color = { static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)), static_cast<unsigned char>(distC(rng)) };
                m_lines.push_back(ln);
            }
            m_totalGeneratedLines += generateCount;
        }

        // Mostrar contador de líneas generadas
        ImGui::Text("Total generated by button: %d lines", m_totalGeneratedLines);

        ImGui::Separator();


        // Modo dibujo: lineas o elipses
        ImGui::Checkbox("Draw Ellipses", &m_drawEllipseMode);
        if (!m_drawEllipseMode)
            ImGui::Text("Mode: Lines");
        else
            ImGui::Text("Mode: Ellipses");

        // Selección del algoritmo de elipse
        ImGui::Checkbox("Use optimized ellipse (drawEllipse2)", &m_useEllipse2);
        if (m_useEllipse2)
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Ellipse algorithm: drawEllipse2 (optimized)");
        else
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Ellipse algorithm: drawEllipse1 (original)");

       

        // Botón para ejecutar prueba de igualdad
        ImGui::Separator();
        if (ImGui::Button("Run ellipse equality test (10000)")) {
            m_testResult = "Running ellipse equality test... (see console)";
            runEllipseEqualityTest(10000);
        }
        
        if (!m_testResult.empty()) {
            ImGui::TextWrapped("%s", m_testResult.c_str());
        }

        // Botón para ejecutar prueba de rendimiento y exportar CSV
        ImGui::Separator();
        if (ImGui::Button("Run ellipse performance test (CSV)")) {
            m_perfStatus = "Preparing performance test...";
            // Lista de N solicitada
            std::vector<int> tests = {50000,100000,150000,200000,250000,300000,305000,400000,450000,500000};
            runEllipsePerformanceTest(tests);
        }


        ImGui::Separator();

        // Botón para borrar todas las figuras
        if (ImGui::Button("Clear screen")) {
            m_lines.clear();
            m_ellipses.clear();
            m_totalGeneratedLines = 0;
        }

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (deltaTime >= 1.0) {
            double fps = framesThisSecond / deltaTime;
            char title[256];
            snprintf(title, sizeof(title), "CPixelRender - Frames per second: %.2f", fps);
            glfwSetWindowTitle(m_window, title);
            framesThisSecond = 0;
            lastTime = currentTime;
        }
    }
};

int main() {
    CMyTest test;
    if (!test.setup()) {
        fprintf(stderr, "Failed to setup CPixelRender\n");
        return -1;
    }

    test.mainLoop();

    return 0;
}
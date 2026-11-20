#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t signature; uint32_t file_size; uint16_t res1, res2; uint32_t data_offset;
};
struct BMPInfoHeader {
    uint32_t header_size; int32_t width, height; uint16_t planes, bits_per_pixel;
    uint32_t compression, image_size; int32_t x_pixels_per_m, y_pixels_per_m;
    uint32_t colors_used, colors_important;
};
#pragma pack(pop)

struct Pixel { uint8_t r, g, b; double y; };

class Bitmap {
    int width, height;
    std::vector<Pixel> pixels;

    // Универсальная функция для расчета Mean и Median по гистограмме
    void calcStats(const int* hist, int total, double& mean, int& median) {
        long long sum = 0; int count = 0; median = 0;
        for (int i = 0; i < 256; ++i) sum += i * hist[i];
        mean = (double)sum / total;
        for (int i = 0; i < 256; ++i) {
            count += hist[i];
            if (count >= total / 2) { median = i; break; }
        }
    }

public:
    bool load(const char* path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        BMPFileHeader fh; BMPInfoHeader ih;
        f.read((char*)&fh, sizeof(fh)); f.read((char*)&ih, sizeof(ih));
        if (fh.signature != 0x4D42) return false;

        // Вывод заголовка (сокращенный)
        std::cout << "--- Headers ---\nSignature: BM\nFile size: " << fh.file_size
                  << "\nData offset: " << fh.data_offset << "\nInfo header size: " << ih.header_size
                  << "\nWidth: " << ih.width << "\nHeight: " << ih.height
                  << "\nPlanes: " << ih.planes << "\nBits per pixel: " << ih.bits_per_pixel
                  << "\nCompression: " << ih.compression << "\nImage size: " << ih.image_size
                  << "\nXpixelsPerM: " << ih.x_pixels_per_m << "\nYpixelsPerM: " << ih.y_pixels_per_m
                  << "\nColors Used: " << ih.colors_used << "\nColors Important: " << ih.colors_important << "\n\n";

        width = ih.width; height = std::abs(ih.height);
        int bpp = ih.bits_per_pixel;

        // Чтение палитры (если нужно)
        std::vector<Pixel> palette;
        if (bpp <= 8) {
            int n = ih.colors_used == 0 ? (1 << bpp) : ih.colors_used;
            palette.resize(n);
            for (int i = 0; i < n; ++i) {
                uint8_t b, g, r, res;
                f.read((char*)&b, 1); f.read((char*)&g, 1); f.read((char*)&r, 1); f.read((char*)&res, 1);
                palette[i] = {r, g, b, 0.0};
            }
        }

        f.seekg(fh.data_offset);
        int rowSize = ((width * bpp + 31) / 32) * 4;
        std::vector<uint8_t> row(rowSize);
        pixels.resize(width * height);

        // Чтение пикселей
        for (int y = 0; y < height; ++y) {
            f.read((char*)row.data(), rowSize);
            int realY = ih.height > 0 ? height - 1 - y : y;
            for (int x = 0; x < width; ++x) {
                Pixel p = {0, 0, 0, 0.0};
                if (bpp == 24 || bpp == 32) {
                    int offset = x * (bpp / 8);
                    p = {row[offset + 2], row[offset + 1], row[offset], 0.0}; // BGR
                } else if (bpp == 8) p = palette[row[x]];
                else if (bpp == 4) p = palette[(x % 2 == 0) ? (row[x / 2] >> 4) : (row[x / 2] & 0x0F)];

                p.y = 0.2126 * p.r + 0.7152 * p.g + 0.0722 * p.b; // Luminance
                pixels[realY * width + x] = p;
            }
        }
        return true;
    }

    void printCorners() {
        std::cout << "--- Corners (R: G: B) ---\n";
        int xs[] = {0, width - 1, 0, width - 1}, ys[] = {0, 0, height - 1, height - 1};
        const char* names[] = {"Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right"};
        for (int i = 0; i < 4; ++i) {
            Pixel p = pixels[ys[i] * width + xs[i]];
            std::cout << names[i] << ": R:" << (int)p.r << " G:" << (int)p.g << " B:" << (int)p.b << "\n";
        }
        std::cout << "\n";
    }

    void evaluateExposure() {
        int hist[256] = {0};
        for (auto& p : pixels) hist[std::clamp((int)std::round(p.y), 0, 255)]++;
        double mean; int median;
        calcStats(hist, width * height, mean, median);
        std::cout << "--- Exposure (Y) ---\nMean: " << mean << "\nMedian: " << median << "\n\n";
    }

    void evaluateWhiteBalance() {
        long long rS = 0, gS = 0, bS = 0;
        int total = width * height;
        for (auto& p : pixels) { rS += p.r; gS += p.g; bS += p.b; }

        double rAvg = (double)rS / total, gAvg = (double)gS / total, bAvg = (double)bS / total;
        double target = std::max({rAvg, gAvg, bAvg});
        double kr = target / rAvg, kg = target / gAvg, kb = target / bAvg;

        std::cout << "--- White Balance ---\nGains: Kr=" << kr << " Kg=" << kg << " Kb=" << kb << "\n";

        int hR[256] = {0}, hG[256] = {0}, hB[256] = {0};
        for (auto& p : pixels) {
            p.r = std::clamp((int)(p.r * kr), 0, 255);
            p.g = std::clamp((int)(p.g * kg), 0, 255);
            p.b = std::clamp((int)(p.b * kb), 0, 255);
            hR[p.r]++; hG[p.g]++; hB[p.b]++;
        }

        double mean; int median;
        calcStats(hR, total, mean, median); std::cout << "R -> Mean: " << mean << ", Median: " << median << "\n";
        calcStats(hG, total, mean, median); std::cout << "G -> Mean: " << mean << ", Median: " << median << "\n";
        calcStats(hB, total, mean, median); std::cout << "B -> Mean: " << mean << ", Median: " << median << "\n";
    }
};

int main(int argc, char** argv) {
    Bitmap bmp;
    if (!bmp.load(argc > 1 ? argv[1] : "image.bmp")) return 1;
    
    bmp.printCorners();
    bmp.evaluateExposure();
    bmp.evaluateWhiteBalance();
    
    return 0;
}
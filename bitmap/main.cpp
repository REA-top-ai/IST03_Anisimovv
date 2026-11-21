#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

// Одна точка на экране
struct Pixel { 
    int r, g, b; 
    double y; 
};

class Bitmap {
public:
    int width, height;
    vector<Pixel> pixels;

    // --- ФУНКЦИИ ДЛЯ АРИФМЕТИЧЕСКОГО ПЕРЕВОДА БАЙТОВ В ИНТЫ ---

    // Читает 2 байта и собирает из них число (для short)
    int read2Bytes(ifstream& f) {
        unsigned char bytes[2];
        f.read((char*)bytes, 2); // Читаем 2 сырых байта
        // Арифметика: младший байт + старший байт * 256
        return bytes[0] + (bytes[1] * 256); 
    }

    // Читает 4 байта и собирает из них число (для int)
    int read4Bytes(ifstream& f) {
        unsigned char bytes[4];
        f.read((char*)bytes, 4); // Читаем 4 сырых байта
        // Арифметика: собираем число, умножая на степени 256
        return bytes[0] + 
               (bytes[1] * 256) + 
               (bytes[2] * 256 * 256) + 
               (bytes[3] * 256 * 256 * 256);
    }

    // -----------------------------------------------------------

    bool load(const char* path) {
        ifstream f(path, ios::binary);
        if (!f) return false;

        // ЧИТАЕМ ЗАГОЛОВОК ВРУЧНУЮ ЧЕРЕЗ АРИФМЕТИКУ
        int signature = read2Bytes(f); // Читаем 2 байта (BM)
        if (signature != 0x4D42) return false; // Проверка на буквы BM

        int file_size = read4Bytes(f);       // Читаем 4 байта
        read2Bytes(f);                       // Пропускаем 2 байта (reserved)
        read2Bytes(f);                       // Пропускаем 2 байта (reserved)
        int data_offset = read4Bytes(f);     // Смещение до пикселей

        int header_size = read4Bytes(f);     // Размер инфо-заголовка
        width = read4Bytes(f);               // Ширина
        height = read4Bytes(f);              // Высота
        int planes = read2Bytes(f);          // Плоскости
        int bits_per_pixel = read2Bytes(f);  // Битность

        // Пропускаем остальные заголовки, чтобы не читать их по одному
        // (там compression, image_size и т.д., они нам не нужны для логики)
        
        if (bits_per_pixel != 24) {
            cout << "Ошибка: код работает только с 24-битными BMP!" << endl;
            return false;
        }

        // Прыгаем к пикселям
        f.seekg(data_offset);

        int rowSize = (width * 3 + 3) & ~3;
        vector<unsigned char> row(rowSize);
        pixels.resize(width * height);

        // Читаем пиксели
        for (int y = 0; y < height; y++) {
            f.read((char*)row.data(), rowSize);
            int realY = height - 1 - y; 
            
            for (int x = 0; x < width; x++) {
                Pixel p;
                // ТУТ ТОЖЕ ЯВНЫЙ ПЕРЕВОД БАЙТОВ В ИНТЫ
                p.b = (int)row[x * 3];     // Явно приводим тип к int
                p.g = (int)row[x * 3 + 1];
                p.r = (int)row[x * 3 + 2];
                
                p.y = 0.2126 * p.r + 0.7152 * p.g + 0.0722 * p.b;
                pixels[realY * width + x] = p;
            }
        }
        return true;
    }

    void printCorners() {
        cout << "--- Corners (R: G: B) ---" << endl;
        cout << "Top-Left: R:" << pixels[0].r << " G:" << pixels[0].g << " B:" << pixels[0].b << endl;
        cout << "Top-Right: R:" << pixels[width - 1].r << " G:" << pixels[width - 1].g << " B:" << pixels[width - 1].b << endl;
        cout << "Bottom-Left: R:" << pixels[(height - 1) * width].r << " G:" << pixels[(height - 1) * width].g << " B:" << pixels[(height - 1) * width].b << endl;
        cout << "Bottom-Right: R:" << pixels[height * width - 1].r << " G:" << pixels[height * width - 1].g << " B:" << pixels[height * width - 1].b << endl;
    }

    void evaluateExposure() {
        int hist[256] = {0};
        long long sum = 0;

        for (int i = 0; i < pixels.size(); i++) {
            int val = round(pixels[i].y);
            if (val > 255) val = 255;
            if (val < 0) val = 0;
            hist[val]++;
            sum += val;
        }

        double mean = (double)sum / pixels.size();
        int median = 0, count = 0;
        for (int i = 0; i < 256; i++) {
            count += hist[i];
            if (count >= pixels.size() / 2) { median = i; break; }
        }

        cout << "--- Exposure (Y) ---" << endl;
        cout << "Mean: " << mean << endl;
        cout << "Median: " << median << endl;
    }

    void evaluateWhiteBalance() {
        long long sumR = 0, sumG = 0, sumB = 0;
        int total = pixels.size();

        for (int i = 0; i < total; i++) {
            sumR += pixels[i].r; sumG += pixels[i].g; sumB += pixels[i].b;
        }

        double avgR = (double)sumR / total;
        double avgG = (double)sumG / total;
        double avgB = (double)sumB / total;
        double target = max({avgR, avgG, avgB});

        double kr = target / avgR;
        double kg = target / avgG;
        double kb = target / avgB;

        cout << "--- White Balance ---" << endl;
        cout << "Gains: Kr=" << kr << " Kg=" << kg << " Kb=" << kb << endl;

        for (int i = 0; i < total; i++) {
            pixels[i].r = min(255, (int)(pixels[i].r * kr));
            pixels[i].g = min(255, (int)(pixels[i].g * kg));
            pixels[i].b = min(255, (int)(pixels[i].b * kb));
        }
    }
};

int main(int argc, char** argv) {
    Bitmap bmp;
    if (!bmp.load(argc > 1 ? argv[1] : "image.bmp")) {
        cout << "Ошибка загрузки!" << endl;
        return 1;
    }

    bmp.printCorners();
    bmp.evaluateExposure();
    bmp.evaluateWhiteBalance();
    return 0;
}

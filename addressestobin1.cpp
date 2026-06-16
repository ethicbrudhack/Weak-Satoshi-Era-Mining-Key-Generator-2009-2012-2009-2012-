#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>

const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

int base58_char_value(char c) {
    const char* p = strchr(BASE58_ALPHABET, c);
    return p ? p - BASE58_ALPHABET : -1;
}

bool base58_decode_fast(const std::string& input, std::vector<unsigned char>& output) {
    std::vector<unsigned char> temp(25, 0);
    for (char c : input) {
        int carry = base58_char_value(c);
        if (carry == -1) return false;
        for (int i = 24; i >= 0; --i) {
            carry += 58 * temp[i];
            temp[i] = carry & 0xFF;
            carry >>= 8;
        }
        if (carry != 0) return false;
    }
    output.assign(temp.begin() + 1, temp.begin() + 21);
    return true;
}

void process_chunk(const std::vector<std::string>& lines,
                   std::ofstream& fout,
                   std::mutex& fout_mutex,
                   std::atomic<uint64_t>& processed)
{
    std::vector<unsigned char> buffer;
    buffer.reserve(lines.size() * 20);

    std::vector<unsigned char> ripemd160;
    for (const auto& line : lines) {
        if (line.empty()) continue;
        if (!base58_decode_fast(line, ripemd160)) continue;
        buffer.insert(buffer.end(), ripemd160.begin(), ripemd160.end());
    }

    {
        std::lock_guard<std::mutex> lock(fout_mutex);
        fout.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }

    processed += lines.size();
    std::cout << "\rProcessed: " << processed / 1000000.0 << " M lines" << std::flush;
}

int main() {
    std::ifstream fin("addresses.txt");
    if (!fin) {
        std::cerr << "Cannot open input file" << std::endl;
        return 1;
    }
    std::ofstream fout("addresses.bin", std::ios::binary);
    if (!fout) {
        std::cerr << "Cannot create addresses.bin" << std::endl;
        return 1;
    }

    const size_t CHUNK_LINES = 1000000; // process 1 million lines at a time
    std::vector<std::string> lines;
    lines.reserve(CHUNK_LINES);

    std::mutex fout_mutex;
    std::atomic<uint64_t> processed(0);
    std::string line;

    std::cout << "Starting conversion of large file..." << std::endl;

    while (true) {
        lines.clear();
        for (size_t i = 0; i < CHUNK_LINES && std::getline(fin, line); ++i) {
            if (!line.empty()) lines.push_back(std::move(line));
        }
        if (lines.empty()) break;

        int num_threads = std::min(8u, std::thread::hardware_concurrency());
        size_t per_thread = (lines.size() + num_threads - 1) / num_threads;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            size_t start = i * per_thread;
            size_t end = std::min(lines.size(), start + per_thread);
            if (start >= end) break;
            std::vector<std::string> sub(lines.begin() + start, lines.begin() + end);
            threads.emplace_back(process_chunk, std::move(sub),
                                 std::ref(fout), std::ref(fout_mutex), std::ref(processed));
        }

        for (auto& t : threads) t.join();
    }

    std::cout << "\nConversion finished! Saved to addresses.bin" << std::endl;
    return 0;
}

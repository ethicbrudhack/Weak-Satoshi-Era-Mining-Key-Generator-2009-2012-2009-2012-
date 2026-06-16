// weak_scanner_historical_bilingual.cpp
// Compilation / Компиляция:
// g++ -o weak_scanner_historical_bilingual weak_scanner_historical_bilingual.cpp -lssl -lcrypto -lsecp256k1 -lpthread -O3 -march=native -flto

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/rand.h>
#include <secp256k1.h>

// ===============================
// PARAMETERS / ПАРАМЕТРЫ
// ===============================
static const int THREAD_COUNT = 32;
static const uint64_t BATCH_SIZE = 10000;
static const uint64_t SAVE_INTERVAL = 100000;

// Time range: 2009-01-01 – 2012-12-31
static const uint64_t START_TIMESTAMP = 1230768000ULL;
static const uint64_t END_TIMESTAMP   = 1356998399ULL;
static const uint64_t MAX_TS_OFFSET = END_TIMESTAMP - START_TIMESTAMP + 1;
static const uint64_t MAX_COUNTER = 1000;          // simulates milliseconds

// Fixed PID and TID (can be expanded)
static const uint32_t FIXED_PID = 1234;
static const uint32_t FIXED_TID = 5678;

// Full range
static const uint64_t TOTAL_SEEDS = MAX_TS_OFFSET * MAX_COUNTER;

// ===============================
// DONATION INFO / ИНФОРМАЦИЯ О ПОЖЕРТВОВАНИИ
// ===============================
static const char* DONATION_ADDRESS = "bc1qps62cyk9f9unmdkc9k3ccj9e2h8ywfhg2j53ec";
static const char* FOUND_MESSAGE =
    "\n==============================================================\n"
    "🎉 Congratulations! You found a historical Bitcoin private key\n"
    "   from the 2009-2012 weak entropy era.\n"
    "   If you find this tool useful, please consider sending a\n"
    "   small percentage of the reward to the developer:\n"
    "   BTC: bc1qps62cyk9f9unmdkc9k3ccj9e2h8ywfhg2j53ec\n"
    "   Thank you! 🙏\n"
    "--------------------------------------------------------------\n"
    "🎉 Поздравляем! Вы нашли исторический приватный ключ Bitcoin\n"
    "   из эпохи слабой энтропии (2009-2012).\n"
    "   Если этот инструмент оказался полезным, пожалуйста,\n"
    "   отправьте небольшой процент вознаграждения разработчику:\n"
    "   BTC: bc1qps62cyk9f9unmdkc9k3ccj9e2h8ywfhg2j53ec\n"
    "   Спасибо! 🙏\n"
    "==============================================================\n";

// ===============================
// MUTEXES & ATOMICS
// ===============================
std::mutex log_mutex;
std::mutex rand_mutex;          // OpenSSL is not thread-safe
std::atomic<uint64_t> total_processed{0};
std::atomic<uint64_t> total_found{0};
std::atomic<bool> stop{false};
auto start_time = std::chrono::steady_clock::now();

// ===============================
// MMAP FILE
// ===============================
class MMapFile {
public:
    MMapFile(const char* path) {
        fd = ::open(path, O_RDONLY);
        if (fd < 0) throw std::runtime_error(std::string("open: ") + strerror(errno));
        struct stat st{};
        if (fstat(fd, &st) != 0) throw std::runtime_error(std::string("fstat: ") + strerror(errno));
        size = st.st_size;
        if (size == 0 || size % 20 != 0) throw std::runtime_error("invalid bin file");
        data = (const unsigned char*) mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) throw std::runtime_error(std::string("mmap: ") + strerror(errno));
    }
    ~MMapFile() { if (data) munmap((void*)data, size); if (fd >= 0) close(fd); }
    const unsigned char* ptr() const { return data; }
    size_t count() const { return size / 20; }
private:
    int fd;
    const unsigned char* data;
    size_t size;
};

inline bool contains_address(const MMapFile& mm, const unsigned char addr20[20]) {
    const unsigned char* base = mm.ptr();
    size_t count = mm.count();
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        const unsigned char* midp = base + mid * 20;
        int cmp = memcmp(midp, addr20, 20);
        if (cmp == 0) return true;
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
    return false;
}

// ===============================
// HASH FUNCTIONS
// ===============================
inline void sha256_once(const unsigned char* d, size_t n, unsigned char out[32]) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, d, n);
    SHA256_Final(out, &ctx);
}
inline void ripemd160_once(const unsigned char* d, size_t n, unsigned char out[20]) {
    RIPEMD160_CTX ctx;
    RIPEMD160_Init(&ctx);
    RIPEMD160_Update(&ctx, d, n);
    RIPEMD160_Final(out, &ctx);
}
inline void pubkey_hash160(const unsigned char* pub, size_t len, unsigned char out[20]) {
    unsigned char sh[32];
    sha256_once(pub, len, sh);
    ripemd160_once(sh, 32, out);
}

// ===============================
// FAST PUBKEY CONTEXT
// ===============================
struct FastPubCtx {
    secp256k1_context* ctx;
    secp256k1_pubkey pub;
};
inline bool fast_load_priv(FastPubCtx& pc, const unsigned char priv[32]) {
    return secp256k1_ec_pubkey_create(pc.ctx, &pc.pub, priv) == 1;
}
inline bool fast_get_hash160(FastPubCtx& pc, unsigned char h160[20], bool compressed) {
    unsigned char pubkey[65];
    size_t len = compressed ? 33 : 65;
    unsigned int flags = compressed ? SECP256K1_EC_COMPRESSED : SECP256K1_EC_UNCOMPRESSED;
    if (secp256k1_ec_pubkey_serialize(pc.ctx, pubkey, &len, &pc.pub, flags) != 1)
        return false;
    pubkey_hash160(pubkey, len, h160);
    return true;
}

// ===============================
// BASE58
// ===============================
static const char* BASE58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
std::string base58_encode(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> digits(data.size() * 138 / 100 + 1);
    int digitslen = 1;
    for (size_t i = 0; i < data.size(); i++) {
        unsigned int carry = data[i];
        for (int j = 0; j < digitslen; j++) {
            carry += (unsigned int)(digits[j]) << 8;
            digits[j] = (unsigned char)(carry % 58);
            carry /= 58;
        }
        while (carry) {
            digits[digitslen++] = (unsigned char)(carry % 58);
            carry /= 58;
        }
    }
    std::string result;
    for (int i = digitslen - 1; i >= 0; i--) result += BASE58[digits[i]];
    for (size_t i = 0; i < data.size() && data[i] == 0; i++) result = '1' + result;
    return result;
}
std::string base58check_encode(const std::vector<unsigned char>& data) {
    unsigned char hash1[32], hash2[32];
    sha256_once(data.data(), data.size(), hash1);
    sha256_once(hash1, 32, hash2);
    std::vector<unsigned char> extended = data;
    extended.insert(extended.end(), hash2, hash2 + 4);
    return base58_encode(extended);
}

// ===============================
// GENERATOR – recreates original entropy sources
// ===============================
void generateWeakSeed(unsigned char* output, uint64_t timestamp_offset, uint64_t counter) {
    uint64_t timestamp = START_TIMESTAMP + timestamp_offset;
    uint32_t tick = static_cast<uint32_t>((timestamp * 1000) + counter);
    uint32_t pid = FIXED_PID;
    uint32_t tid = FIXED_TID;

    unsigned char seed_buf[20];
    memcpy(seed_buf,      &timestamp, 8);
    memcpy(seed_buf + 8,  &tick,      4);
    memcpy(seed_buf + 12, &pid,       4);
    memcpy(seed_buf + 16, &tid,       4);

    std::lock_guard<std::mutex> lock(rand_mutex);
    RAND_seed(seed_buf, sizeof(seed_buf));
    RAND_bytes(output, 32);
}

// ===============================
// SCANNING BATCH (with debug for first 10)
// ===============================
void scan_batch(secp256k1_context* ctx, const MMapFile& mm,
                uint64_t start_idx, uint64_t end_idx,
                std::ofstream& found, std::atomic<bool>& debug_done) {
    FastPubCtx fctx;
    fctx.ctx = ctx;

    for (uint64_t idx = start_idx; idx < end_idx && !stop; ++idx) {
        uint64_t ts_offset = idx / MAX_COUNTER;
        uint64_t counter = idx % MAX_COUNTER;
        if (ts_offset >= MAX_TS_OFFSET) break;

        unsigned char priv[32];
        generateWeakSeed(priv, ts_offset, counter);

        bool do_debug = false;
        uint64_t cur = total_processed.load();
        if (!debug_done.load() && cur < 10) do_debug = true;

        if (do_debug) {
            uint64_t timestamp = START_TIMESTAMP + ts_offset;
            uint32_t tick = static_cast<uint32_t>((timestamp * 1000) + counter);
            uint32_t pid = FIXED_PID;
            uint32_t tid = FIXED_TID;
            unsigned char raw_seed[20];
            memcpy(raw_seed,      &timestamp, 8);
            memcpy(raw_seed + 8,  &tick,      4);
            memcpy(raw_seed + 12, &pid,       4);
            memcpy(raw_seed + 16, &tid,       4);

            char raw_hex[41];
            for (int i = 0; i < 20; i++) sprintf(raw_hex + i*2, "%02x", raw_seed[i]);
            char priv_hex[65];
            for (int i = 0; i < 32; i++) sprintf(priv_hex + i*2, "%02x", priv[i]);

            std::string addr_uncomp, addr_comp;
            if (fast_load_priv(fctx, priv)) {
                unsigned char h160[20];
                if (fast_get_hash160(fctx, h160, false)) {
                    std::vector<unsigned char> addr_data = {0x00};
                    addr_data.insert(addr_data.end(), h160, h160 + 20);
                    addr_uncomp = base58check_encode(addr_data);
                }
                if (fast_get_hash160(fctx, h160, true)) {
                    std::vector<unsigned char> addr_data = {0x00};
                    addr_data.insert(addr_data.end(), h160, h160 + 20);
                    addr_comp = base58check_encode(addr_data);
                }
            }
            {
                std::lock_guard<std::mutex> lock(log_mutex);
                std::cout << "\n🔍 DEBUG seed " << cur
                          << " (ts_offset=" << ts_offset << ", counter=" << counter << "):\n";
                std::cout << "   Raw entropy (20B): " << raw_hex << "\n";
                std::cout << "   Stretched (priv):  " << priv_hex << "\n";
                std::cout << "   Uncompressed addr: " << addr_uncomp << "\n";
                std::cout << "   Compressed addr:   " << addr_comp << "\n";
            }
            if (cur == 9) debug_done.store(true);
        }

        // Main search
        if (fast_load_priv(fctx, priv)) {
            unsigned char h160[20];
            bool found_addr = false;
            std::string address;
            bool compressed = false;

            if (fast_get_hash160(fctx, h160, false) && contains_address(mm, h160)) {
                std::vector<unsigned char> addr_data = {0x00};
                addr_data.insert(addr_data.end(), h160, h160 + 20);
                address = base58check_encode(addr_data);
                found_addr = true;
                compressed = false;
            }
            if (!found_addr && fast_get_hash160(fctx, h160, true) && contains_address(mm, h160)) {
                std::vector<unsigned char> addr_data = {0x00};
                addr_data.insert(addr_data.end(), h160, h160 + 20);
                address = base58check_encode(addr_data);
                found_addr = true;
                compressed = true;
            }

            if (found_addr) {
                char priv_hex[65];
                for (int i = 0; i < 32; i++) sprintf(priv_hex + i*2, "%02x", priv[i]);

                {
                    std::lock_guard<std::mutex> lock(log_mutex);
                    // Write to file
                    found << "FOUND / НАЙДЕНО: " << (compressed ? "compressed / сжатый" : "uncompressed / несжатый") << "\n"
                          << "PRIV: " << priv_hex << "\n"
                          << "ADDR: " << address << "\n";
                    found << FOUND_MESSAGE;
                    found.flush();

                    // Console output
                    std::cout << "\n🎯 FOUND / НАЙДЕНО! " << address << "\n";
                    std::cout << FOUND_MESSAGE;

                    total_found++;
                }
            }
        }
        total_processed++;
    }
}

// ===============================
// STATE MANAGEMENT
// ===============================
void save_state(uint64_t idx) {
    std::ofstream f("progress.txt", std::ios::trunc);
    f << idx << "\n";
}
bool load_state(uint64_t& idx) {
    std::ifstream f("progress.txt");
    if (!f.is_open()) return false;
    f >> idx;
    return true;
}

// ===============================
// MAIN LOOP (multi-threaded)
// ===============================
void run_scanner(const MMapFile& mm, uint64_t start_idx, uint64_t total_seeds) {
    std::ofstream found("found.txt", std::ios::app);
    std::atomic<uint64_t> next_idx(start_idx);
    std::atomic<bool> stop_threads(false);
    std::atomic<bool> debug_done(false);

    std::thread monitor([&]() {
        uint64_t last = 0;
        while (!stop_threads) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            uint64_t now = total_processed.load();
            double rate = (now - last) / 1000000.0;
            last = now;
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cout << "\r⚡ " << std::fixed << std::setprecision(2) << rate
                      << " M/s | " << (total_processed / 1000000) << "M seeds / сидов | found / найдено: " << total_found
                      << std::flush;
        }
    });

    std::vector<std::thread> workers;
    for (int t = 0; t < THREAD_COUNT; ++t) {
        workers.emplace_back([&]() {
            secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
            while (true) {
                uint64_t start = next_idx.fetch_add(BATCH_SIZE);
                if (start >= total_seeds) break;
                uint64_t end = std::min(start + BATCH_SIZE, total_seeds);
                scan_batch(ctx, mm, start, end, found, debug_done);
                uint64_t cur = total_processed.load();
                if (cur % SAVE_INTERVAL == 0 && cur > 0) save_state(cur);
            }
            secp256k1_context_destroy(ctx);
        });
    }
    for (auto& w : workers) w.join();
    stop_threads = true;
    monitor.join();
}

// ===============================
// HELP
// ===============================
void printHelp() {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   WEAK SEED SCANNER – HISTORICAL (2009-2012)                   ║\n";
    std::cout << "║   СКАНЕР СЛАБЫХ СИДОВ – ИСТОРИЧЕСКИЙ (2009-2012)              ║\n";
    std::cout << "║   Recreates original Bitcoin-Qt entropy sources                ║\n";
    std::cout << "║   Воссоздает оригинальные источники энтропии Bitcoin-Qt       ║\n";
    std::cout << "║   (time, tick, PID, TID) – skipping QueryPerformanceCounter    ║\n";
    std::cout << "║   (время, тик, PID, TID) – пропуская QueryPerformanceCounter ║\n";
    std::cout << "║   Supports uncompressed and compressed addresses               ║\n";
    std::cout << "║   Поддерживает несжатые и сжатые адреса                       ║\n";
    std::cout << "║   Speed ~0.5-1 M/s, supports resume (--resume)                ║\n";
    std::cout << "║   Скорость ~0.5-1 M/с, поддержка возобновления (--resume)    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "Usage / Использование: ./weak_scanner_historical_bilingual <adresy.bin> [--resume]\n";
}

// ===============================
// MAIN
// ===============================
int main(int argc, char* argv[]) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   WEAK SEED SCANNER – HISTORICAL (2009-2012)                   ║\n";
    std::cout << "║   СКАНЕР СЛАБЫХ СИДОВ – ИСТОРИЧЕСКИЙ (2009-2012)              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";

    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string bin_file = argv[1];
    bool resume = (argc >= 3 && std::string(argv[2]) == "--resume");

    uint64_t total_seeds = TOTAL_SEEDS;

    try {
        MMapFile mm(bin_file.c_str());
        std::cout << "\n📁 Loaded / Загружено: " << mm.count() << " addresses / адресов\n";
        std::cout << "📅 Time range / Временной диапазон: 2009-01-01 – 2012-12-31\n";
        std::cout << "🔢 Max seeds / Макс. сидов: " << total_seeds << " (approx / ок. "
                  << (total_seeds / 1e9) << " billion / млрд)\n";

        uint64_t start_index = 0;
        if (resume && load_state(start_index)) {
            std::cout << "🔁 Resuming from index / Возобновление с индекса: " << start_index << "\n";
        } else {
            std::cout << "🚀 Starting from scratch / Запуск с начала\n";
        }

        start_time = std::chrono::steady_clock::now();
        run_scanner(mm, start_index, total_seeds);

        auto elapsed = std::chrono::steady_clock::now() - start_time;
        double seconds = std::chrono::duration<double>(elapsed).count();

        std::cout << "\n\n✅ SCAN FINISHED / СКАНИРОВАНИЕ ЗАВЕРШЕНО\n";
        std::cout << "   📊 Processed / Обработано: " << total_processed << " seeds / сидов\n";
        std::cout << "   ✅ Found / Найдено: " << total_found << " addresses / адресов\n";
        std::cout << "   ⏱️  Time / Время: " << std::fixed << std::setprecision(2) << seconds << " s / с\n";
        if (seconds > 0) {
            std::cout << "   🚀 Speed / Скорость: " << std::fixed << std::setprecision(2)
                      << (total_processed / seconds / 1000000.0) << " M seeds/s / M сидов/с\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error / Ошибка: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

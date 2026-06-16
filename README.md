╔═══════════════════════════════════════════════════════════════════════════╗
║                                                                           ║
║   WEAK SEED SCANNER – HISTORICAL (2009-2012)                             ║
║   СКАНЕР СЛАБЫХ СИДОВ – ИСТОРИЧЕСКИЙ (2009-2012)                        ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝

📖 DESCRIPTION / ОПИСАНИЕ

This tool scans for Bitcoin private keys that were generated using weak
entropy sources on Windows systems between 2009 and 2012. It reproduces the
exact entropy collection logic used by Bitcoin-Qt (pre-0.5.x) when
QueryPerformanceCounter and performance registry data failed silently,
leaving only:

   • system timestamp        (8 bytes)
   • tick count (ms)         (4 bytes)
   • PID                     (4 bytes)
   • TID                     (4 bytes)

That 20-byte seed was fed into OpenSSL's RAND_seed(), and the resulting
32-byte private key was used. The scanner iterates over all combinations
of timestamp_offset (seconds since 2009-01-01) and counter (0-999,
simulating milliseconds) – total ~126 billion seeds.

Both uncompressed and compressed addresses are checked.

─────────────────────────────────────────────────────────────────────────────

📚 HISTORICAL SOURCES / ИСТОРИЧЕСКИЕ ИСТОЧНИКИ

1. Bitcointalk (2012) – "Possible new vulnerability: poor entropy in
   Windows generated keypairs"

   Describes how QueryPerformanceCounter and RegQueryValueExA() with
   HKEY_PERFORMANCE_DATA could fail silently, leaving RAND_Screen as the
   only entropy source. No warning is logged in debug.log.

   🔗 https://bitcointalk.org/index.php?topic=113496.msg1228101

2. Bitcoin StackExchange (2024) – "When Satoshi Nakamoto mined his first
   set of blocks in 2008/2009, it was on Bitcoin Core, but was he using
   Linux or Windows?"

   Confirms that:
   • Bitcoin v0.1 was Windows only ("Windows only for now" – Satoshi)
   • On Windows, OpenSSL's RNG was initialized manually by Satoshi
     using HKEY_PERFORMANCE_DATA
   • Linux support arrived only in v0.2.0 (December 2009)

   🔗 https://bitcoin.stackexchange.com/questions/124794

─────────────────────────────────────────────────────────────────────────────

💡 CONCLUSION / ВЫВОД

Satoshi Nakamoto had to use Windows in 2009. The entropy source he
implemented (HKEY_PERFORMANCE_DATA) was later found to be silently failing
on many systems – exactly the weakness this scanner reproduces.

─────────────────────────────────────────────────────────────────────────────

🔧 REQUIREMENTS / ТРЕБОВАНИЯ

   • C++ compiler with C++11 support (g++ recommended)
   • OpenSSL (libssl-dev, libcrypto)
   • libsecp256k1
   • pthread (standard on Linux)

Install on Ubuntu/Debian:

   sudo apt update
   sudo apt install build-essential libssl-dev libsecp256k1-dev

─────────────────────────────────────────────────────────────────────────────

🚀 COMPILATION / КОМПИЛЯЦИЯ

   g++ -o weak_scanner_historical_bilingual \
        weak_scanner_historical_bilingual.cpp \
        -lssl -lcrypto -lsecp256k1 -lpthread \
        -O3 -march=native -flto

─────────────────────────────────────────────────────────────────────────────

📁 PREPARING THE ADDRESS FILE (.bin)

The scanner expects a binary file containing 20-byte RIPEMD-160 hashes of
the addresses you want to check. The file must be sorted in ascending
order (memcmp order).

Convert a list of addresses (one per line) using Python:

   python3 convert.py addresses.txt addresses.bin

Where convert.py:

   #!/usr/bin/env python3
   import sys, base58, hashlib
   def hash160(data):
       return hashlib.new('ripemd160', hashlib.sha256(data).digest()).digest()
   def address_to_hash160(addr):
       decoded = base58.b58decode_check(addr)
       return decoded[1:]
   if len(sys.argv) < 3:
       print(f"Usage: {sys.argv[0]} <input.txt> <output.bin>")
       sys.exit(1)
   hashes = []
   with open(sys.argv[1], 'r') as f:
       for line in f:
           addr = line.strip()
           if addr:
               hashes.append(address_to_hash160(addr))
   hashes.sort(key=lambda h: h)
   with open(sys.argv[2], 'wb') as out:
       for h in hashes:
           out.write(h)
   print(f"Wrote {len(hashes)} hashes to {sys.argv[2]}")

─────────────────────────────────────────────────────────────────────────────

▶️ USAGE / ИСПОЛЬЗОВАНИЕ

   ./weak_scanner_historical_bilingual addresses.bin

Resume from previous run:

   ./weak_scanner_historical_bilingual addresses.bin --resume

─────────────────────────────────────────────────────────────────────────────

📂 OUTPUT FILES / ВЫХОДНЫЕ ФАЙЛЫ

   • found.txt    – each found key (private key, address, type)
   • progress.txt – last processed index (used with --resume)

─────────────────────────────────────────────────────────────────────────────

⚡ PERFORMANCE / ПРОИЗВОДИТЕЛЬНОСТЬ

   • Speed:         ~0.5–1 million seeds per second (modern CPU)
   • Full scan:     ~1–2 months (126 billion seeds)
   • Use --resume   to continue after interruptions

─────────────────────────────────────────────────────────────────────────────

🙏 DONATION / ПОЖЕРТВОВАНИЯ

If this tool helps you recover funds, please consider supporting
the developer:

   BTC: bc1qps62cyk9f9unmdkc9k3ccj9e2h8ywfhg2j53ec

Thank you! / Спасибо!

─────────────────────────────────────────────────────────────────────────────

📜 LICENSE / ЛИЦЕНЗИЯ

MIT – free to use and modify.

─────────────────────────────────────────────────────────────────────────────

Happy scanning! / Удачного сканирования! 🍀


╔═══════════════════════════════════════════════════════════════════════════╗
║                                                                           ║
║   ADDRESS CONVERTER & SORTER – for Weak Seed Scanner                     ║
║   КОНВЕРТЕР АДРЕСОВ И СОРТИРОВЩИК – для сканера слабых сидов           ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝

These two tools prepare the binary address file (.bin) required by the
scanner. The first tool converts a text file of addresses (Base58Check) to
a raw 20‑byte binary format. The second tool sorts the resulting binary
file using external merge sort – essential for large files.

Эти два инструмента подготавливают бинарный файл адресов (.bin),
необходимый для сканера. Первый конвертирует текстовый файл с адресами
(Base58Check) в сырой 20‑байтовый бинарный формат. Второй сортирует
полученный бинарный файл с помощью внешней сортировки слиянием –
это необходимо для больших файлов.

─────────────────────────────────────────────────────────────────────────────

📄 1. CONVERTER / КОНВЕРТЕР (addresses2bin1.cpp)

Input  / Вход:   addresses.txt   – one Base58Check address per line
                                      (по одному адресу в строке)
Output / Выход:  addresses.bin    – raw 20‑byte hashes (unsorted)
                                      (сырые 20‑байтовые хеши, не сортированы)

Compilation / Компиляция:

   g++ -o converter addresses2bin1.cpp -std=c++11 -pthread -O3

Usage / Использование:

   ./converter

The program will read addresses.txt and write addresses.bin. It uses
multiple threads (up to 8) and processes the file in chunks of 1 million
lines, showing progress in real time.

Программа читает addresses.txt и записывает addresses.bin. Использует
несколько потоков (до 8) и обрабатывает файл блоками по 1 млн строк,
отображая прогресс в реальном времени.

─────────────────────────────────────────────────────────────────────────────

🧩 2. BINARY SORTER / СОРТИРОВЩИК БИНАРНЫХ ФАЙЛОВ (sortbinfile.cpp)

Input  / Вход:   addresses.bin    – unsorted 20‑byte binary file
                                      (неотсортированный бинарный файл)
Output / Выход:  addresses_sorted.bin – sorted 20‑byte binary file
                                      (отсортированный бинарный файл)

Compilation / Компиляция:

   g++ -o sorter sortbinfile.cpp -std=c++17 -pthread -O3

Usage / Использование:

   ./sorter addresses.bin addresses_sorted.bin

How it works / Как это работает:

   • Splits the input file into 2 GB chunks
     (Разбивает входной файл на части по 2 ГБ)

   • Sorts each chunk in memory using std::sort
     (Сортирует каждую часть в памяти с помощью std::sort)

   • Merges all sorted chunks using a priority queue (merge sort)
     (Сливает все отсортированные части с помощью очереди с приоритетами)

   • Progress is shown for both sorting and merging stages
     (Прогресс отображается для этапов сортировки и слияния)

   • Temporary chunk files (chunk_*.bin) are automatically deleted
     (Временные файлы chunk_*.bin автоматически удаляются)

─────────────────────────────────────────────────────────────────────────────

🔧 FULL WORKFLOW / ПОЛНЫЙ ПОРЯДОК РАБОТЫ

Step 1 – Convert text addresses to binary:

   ./converter

Step 2 – Sort the binary file:

   ./sorter addresses.bin addresses_sorted.bin

Step 3 – Run the scanner with the sorted file:

   ./weak_scanner_historical_bilingual addresses_sorted.bin

─────────────────────────────────────────────────────────────────────────────

⚡ PERFORMANCE NOTES / ЗАМЕЧАНИЯ ПО ПРОИЗВОДИТЕЛЬНОСТИ

   • Converter:   ~1‑2 million lines/sec on modern CPU
                  (~1‑2 млн строк/сек на современном процессоре)

   • Sorter:      Sorting a 100 GB file takes ~2‑4 hours
                  (Сортировка файла 100 ГБ занимает ~2‑4 часа)

   • Disk space:  Temporary chunks require ~2x input size
                  (Временные части требуют ~2x места от входного файла)

   • RAM:         Sorter uses ~2.5 GB per chunk (adjust CHUNK_BYTES if needed)
                  (Сортировщик использует ~2.5 ГБ на часть – уменьшите CHUNK_BYTES при необходимости)

─────────────────────────────────────────────────────────────────────────────

📂 INPUT FILE FORMAT / ФОРМАТ ВХОДНОГО ФАЙЛА

The converter accepts only P2PKH addresses (starting with '1').
Each line must contain a valid Base58Check address. No other formats
(P2SH, Bech32) are supported.

Конвертер принимает только адреса P2PKH (начинающиеся с '1').
Каждая строка должна содержать корректный адрес в формате Base58Check.
Другие форматы (P2SH, Bech32) не поддерживаются.

─────────────────────────────────────────────────────────────────────────────

💡 TIPS / СОВЕТЫ

   • For huge files (1B+ addresses), use the sorter on a machine with
     enough RAM and SSD storage for better performance.
     (Для огромных файлов (1 млрд+ адресов) используйте сортировщик
     на машине с достаточным объёмом RAM и SSD для лучшей производительности.)

   • The converter does NOT sort – you must run the sorter separately.
     (Конвертер НЕ сортирует – вы должны запустить сортировщик отдельно.)

   • If your input text file is already sorted, you can skip the sorter
     and use the converter output directly.
     (Если ваш текстовый файл уже отсортирован, вы можете пропустить
     сортировщик и использовать выход конвертера напрямую.)

─────────────────────────────────────────────────────────────────────────────

Happy converting & sorting! / Удачной конвертации и сортировки! 🚀

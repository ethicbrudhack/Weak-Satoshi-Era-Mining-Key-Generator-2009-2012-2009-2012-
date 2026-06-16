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

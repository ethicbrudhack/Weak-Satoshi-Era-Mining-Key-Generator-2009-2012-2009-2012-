Weak Seed Scanner – Historical (2009–2012)
Сканер слабых сидов – исторический (2009–2012)
https://img.shields.io/badge/License-MIT-yellow.svg

📖 Description / Описание
This tool scans for Bitcoin private keys that were generated using weak entropy sources on Windows systems between 2009 and 2012. It reproduces the exact entropy collection logic used by Bitcoin-Qt (pre‑0.5.x) when QueryPerformanceCounter and performance registry data failed silently – leaving only:

system timestamp (8 bytes)

tick count (milliseconds, 4 bytes)

PID (4 bytes)

TID (4 bytes)

That 20‑byte seed was fed into OpenSSL’s RAND_seed() and the resulting 32‑byte private key was used. The scanner iterates over all combinations of timestamp_offset (seconds since 2009‑01‑01) and counter (0‑999, simulating milliseconds) – a total of ~126 billion seeds. It checks both uncompressed and compressed addresses.

Этот инструмент сканирует приватные ключи Bitcoin, сгенерированные с использованием слабых источников энтропии в Windows в период с 2009 по 2012. Он воспроизводит точную логику сбора энтропии, использовавшуюся в Bitcoin-Qt (до версии 0.5.x), когда QueryPerformanceCounter и данные реестра производительности молча отказывали – оставались только:

временная метка системы (8 байт)

счётчик тиков (миллисекунды, 4 байта)

PID (4 байта)

TID (4 байта)

Этот 20‑байтовый сид подавался в RAND_seed() OpenSSL, а полученный 32‑байтовый приватный ключ использовался. Сканер перебирает все комбинации смещения_метки_времени (секунды с 2009‑01‑01) и счётчика (0‑999, имитация миллисекунд) – всего ~126 миллиардов сидов. Проверяются как несжатые, так и сжатые адреса.

📚 Historical Sources / Исторические источники
This scanner is based on documented vulnerabilities in Windows entropy generation used by early Bitcoin clients:

Bitcointalk (2012) – "Possible new vulnerability: poor entropy in Windows generated keypairs"
Describes how QueryPerformanceCounter and RegQueryValueExA(HKEY_PERFORMANCE_DATA) could fail silently, leaving RAND_Screen as the only entropy source. No warning is logged.
🔗 https://bitcointalk.org/index.php?topic=113496.msg1228101

Bitcoin StackExchange (2024) – "When Satoshi Nakamoto mined his first set of blocks in 2008/2009, it was on Bitcoin Core, but was he using Linux or Windows?"
Confirms that:

Bitcoin v0.1 was Windows only ("Windows only for now" – Satoshi Nakamoto)

On Windows, OpenSSL's RNG was initialized manually by Satoshi using HKEY_PERFORMANCE_DATA

Linux support arrived only in v0.2.0 (December 2009)
🔗 https://bitcoin.stackexchange.com/questions/124794

Conclusion:
Satoshi Nakamoto had to use Windows in 2009, and the entropy source he implemented (HKEY_PERFORMANCE_DATA) was later found to be silently failing on many systems – exactly the weakness this scanner reproduces.

🔧 Requirements / Требования
C++ compiler with C++11 support (e.g., g++)

OpenSSL (libssl-dev, libcrypto)

libsecp256k1 (development package)

pthread (standard on Linux)

Install on Ubuntu/Debian:
sudo apt update
sudo apt install build-essential libssl-dev libsecp256k1-dev

🚀 Compilation / Компиляция
g++ -o weak_scanner_historical_bilingual weak_scanner_historical_bilingual.cpp -lssl -lcrypto -lsecp256k1 -lpthread -O3 -march=native -flto
If you prefer the version without donation messages, replace the source file with weak_scanner_historical.cpp – the compilation command stays the same.
📁 Preparing the Address File (.bin)
Подготовка файла с адресами (.bin)
The scanner expects a binary file containing 20‑byte RIPEMD‑160 hashes of the addresses you want to check. The file must be sorted in ascending order (memcmp order).

How to create it from a list of addresses (one per line):

Decode each Base58Check address to raw bytes.

Strip the leading 0x00 (version byte) – keep only the 20‑byte hash.

Write these 20 bytes in little‑endian order (i.e., as they appear in memory).

Sort the entire file using binary comparison (memcmp).

Python script to convert a list of#!/usr/bin/env python3
#!/usr/bin/env python3
import sys
import base58
import hashlib

def hash160(data):
    return hashlib.new('ripemd160', hashlib.sha256(data).digest()).digest()

def address_to_hash160(addr):
    decoded = base58.b58decode_check(addr)
    # decoded[0] is the version (0x00 for mainnet)
    return decoded[1:]  # 20 bytes

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} <input_addresses.txt> <output.bin>")
    sys.exit(1)

hashes = []
with open(sys.argv[1], 'r') as f:
    for line in f:
        addr = line.strip()
        if addr:
            hashes.append(address_to_hash160(addr))

hashes.sort(key=lambda h: h)  # lexicographic sort

with open(sys.argv[2], 'wb') as out:
    for h in hashes:
        out.write(h)

print(f"Wrote {len(hashes)} hashes to {sys.argv[2]}")


Run it:
python3 convert.py addresses.txt addresses.bin

▶️ Usage / Использование:
./weak_scanner_historical_bilingual addresses.bin
To resume from a previous run (saves progress in progress.txt):
./weak_scanner_historical_bilingual addresses.bin --resume

📂 Output Files / Выходные файлы
found.txt – each found key is appended with full details (private key, address, type). Also includes the donation message (if using bilingual version).

progress.txt – stores the last processed index. Used with --resume.

⚡ Performance / Производительность
Speed: ~0.5–1 million seeds per second (on a modern CPU).

Full scan of 126 billion seeds takes approximately 1–2 months on a single machine.

Use --resume to continue after interruptions.

📜 License / Лицензия
MIT – feel free to use and modify.

🙏 Donation / Пожертвования
If this tool helps you recover funds, please consider supporting the developer:
bc1qps62cyk9f9unmdkc9k3ccj9e2h8ywfhg2j53ec
Thank you! / Спасибо!

🤝 Contributing / Вклад
Issues and pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

📧 Contact / Контакты
For questions, open an issue on GitHub.
По вопросам открывайте issue на GitHub.

Happy scanning! Удачного сканирования! 🍀




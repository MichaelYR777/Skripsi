# TEKNOLOGI LONG RANGE (LORA) DIGUNAKAN UNTUK MONITORING PENERANGAN JALAN UMUM TENAGA SURYA BERBASIS INTERNET OF THINGS (IOT) di KABUPATEN MALINAU

Repositori ini berisi kode sumber (*source code*) untuk tugas akhir/skripsi saya yang berfokus pada komunikasi data menggunakan modul LoRa.

## 📝 Informasi Mahasiswa
* **Nama:** Michael Yehezkiel Rattu
* **NPM:** 2140302010
* **Program Studi:** Teknik Elektro
* **Instansi:** Universitas Borneo Tarakan

## 🚀 Deskripsi Proyek
Proyek ini mengimplementasikan komunikasi nirkabel jarak jauh (Long Range) antara dua perangkat mikrokontroler:
1.  **Node Transmitter (Arduino Nano):** Berfungsi sebagai pengirim data sensor.
2.  **Node Receiver (ESP32):** Berfungsi sebagai penerima data dan menampilkannya pada monitor/sistem.

## 🛠️ Perangkat & Teknologi
* **Hardware:** ESP32 DevKit, Arduino Nano, Modul LoRa SX1278.
* **Software:** Arduino IDE.
* **Library:** WiFi.h, WebServer.h, SPI.h, dan LoRa.h.

## 📂 Struktur File
* `LoRa_Nano.ino`: Kode program untuk sisi pengirim (Gateaway/Node).
* `LoRa_ESP32.ino`: Kode program untuk sisi penerima (End-node).

## 📌 Cara Penggunaan
1.  Buka file menggunakan Arduino IDE.
2.  Pastikan library LoRa sudah terinstal di IDE Anda.
3.  Sesuaikan pin koneksi pada kode program dengan rangkaian hardware Anda.
4.  Lakukan *upload* ke masing-masing perangkat.

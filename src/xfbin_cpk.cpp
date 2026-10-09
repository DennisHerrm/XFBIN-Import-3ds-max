// ============================================================
//  XFBIN CPK - Implementierung. Siehe xfbin_cpk.h.
// ============================================================

#include "xfbin_cpk.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <tuple>

namespace xfbin {

namespace {

// ------------------------------------------------------------
//  Lesen in Big- und Little-Endian
// ------------------------------------------------------------
uint16_t Be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
uint32_t Be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
uint64_t Be64(const uint8_t* p) { return (static_cast<uint64_t>(Be32(p)) << 32) | Be32(p + 4); }
uint32_t Le32(const uint8_t* p) {
    return p[0] | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool Magic(const uint8_t* p, size_t n, const char* m) {
    const size_t k = std::strlen(m);
    return n >= k && std::memcmp(p, m, k) == 0;
}

// ------------------------------------------------------------
//  @UTF-Tabelle
//
//  Kopf (ab "@UTF", alles big-endian):
//    +4  u32 Tabellengroesse ab +8
//    +8  u32 Zeilen, +12 u32 Strings, +16 u32 Datenbereich,
//    +20 u32 Tabellenname  (alle Offsets ab +8)
//    +24 u16 Spalten, +26 u16 Zeilenlaenge, +28 u32 Zeilenzahl
//  Dann je Spalte: u8 Flags, u32 Name, bei konstanten Spalten
//  gleich der Wert. Flags: obere 4 Bit Ablage (0x10 null,
//  0x30 konstant, 0x50 je Zeile), untere 4 Bit Typ.
// ------------------------------------------------------------
struct UtfValue {
    enum Kind { kNone, kInt, kFloat, kString, kData } kind = kNone;
    uint64_t    u = 0;
    double      f = 0.0;
    std::string s;
    uint64_t    dataOffset = 0;   // ab Tabellenanfang ("@UTF")
    uint64_t    dataSize   = 0;
};

struct UtfTable {
    std::vector<std::string>           columns;
    std::vector<std::vector<UtfValue>> rows;

    int Column(const char* name) const {
        for (size_t i = 0; i < columns.size(); ++i)
            if (columns[i] == name) return static_cast<int>(i);
        return -1;
    }
    const UtfValue* Get(size_t row, const char* name) const {
        const int c = Column(name);
        if (c < 0 || row >= rows.size()) return nullptr;
        return &rows[row][static_cast<size_t>(c)];
    }
    uint64_t U(size_t row, const char* name) const {
        const UtfValue* v = Get(row, name);
        return (v != nullptr && v->kind == UtfValue::kInt) ? v->u : 0;
    }
    std::string S(size_t row, const char* name) const {
        const UtfValue* v = Get(row, name);
        return (v != nullptr && v->kind == UtfValue::kString) ? v->s : std::string();
    }
};

bool ParseUtf(const uint8_t* p, size_t n, UtfTable& t, std::string& error) {
    if (n < 32 || !Magic(p, n, "@UTF")) { error = "keine @UTF-Tabelle"; return false; }
    const uint64_t size = Be32(p + 4);
    if (size + 8 > n) { error = "@UTF-Tabelle laenger als der Puffer"; return false; }
    const size_t end = static_cast<size_t>(size + 8);
    const size_t rowsOff = Be32(p + 8) + 8;
    const size_t strOff  = Be32(p + 12) + 8;
    const size_t dataOff = Be32(p + 16) + 8;
    const uint16_t ncols = Be16(p + 24);
    const uint32_t nrows = Be32(p + 28);
    if (rowsOff > end || strOff > end || dataOff > end) { error = "@UTF-Offsets ausserhalb"; return false; }

    auto str = [&](uint32_t off) -> std::string {
        size_t a = strOff + off;
        if (a >= end) return std::string();
        size_t e = a;
        while (e < end && p[e] != 0) ++e;
        return std::string(reinterpret_cast<const char*>(p + a), e - a);
    };

    // Einen Wert ab pos lesen. Liefert false, wenn er nicht mehr
    // in die Tabelle passt.
    auto value = [&](size_t& pos, int type, UtfValue& v) -> bool {
        static const int kSize[] = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 8 };
        if (type < 0 || type > 0x0B) return false;
        const size_t need = static_cast<size_t>(kSize[type]);
        if (pos + need > end) return false;
        const uint8_t* q = p + pos;
        switch (type) {
        case 0: case 1: v.kind = UtfValue::kInt; v.u = q[0]; break;
        case 2: case 3: v.kind = UtfValue::kInt; v.u = Be16(q); break;
        case 4: case 5: v.kind = UtfValue::kInt; v.u = Be32(q); break;
        case 6: case 7: v.kind = UtfValue::kInt; v.u = Be64(q); break;
        case 8: { v.kind = UtfValue::kFloat; uint32_t b = Be32(q); float fl; std::memcpy(&fl, &b, 4); v.f = fl; break; }
        case 9: { v.kind = UtfValue::kFloat; uint64_t b = Be64(q); double d; std::memcpy(&d, &b, 8); v.f = d; break; }
        case 0x0A: v.kind = UtfValue::kString; v.s = str(Be32(q)); break;
        case 0x0B: v.kind = UtfValue::kData; v.dataOffset = dataOff + Be32(q); v.dataSize = Be32(q + 4); break;
        default: return false;
        }
        pos += need;
        return true;
    };

    struct Col { int storage; int type; UtfValue constant; };
    std::vector<Col> cols;
    size_t pos = 32;
    for (uint16_t c = 0; c < ncols; ++c) {
        if (pos + 5 > end) { error = "@UTF-Spaltenkopf abgeschnitten"; return false; }
        const uint8_t flags = p[pos];
        Col col{ flags & 0xF0, flags & 0x0F, UtfValue() };
        t.columns.push_back(str(Be32(p + pos + 1)));
        pos += 5;
        if (col.storage == 0x30 && !value(pos, col.type, col.constant)) {
            error = "@UTF-Konstante abgeschnitten";
            return false;
        }
        cols.push_back(col);
    }

    pos = rowsOff;
    t.rows.resize(nrows);
    for (uint32_t r = 0; r < nrows; ++r) {
        auto& row = t.rows[r];
        row.resize(ncols);
        for (uint16_t c = 0; c < ncols; ++c) {
            if (cols[c].storage == 0x50) {
                if (!value(pos, cols[c].type, row[c])) { error = "@UTF-Zeile abgeschnitten"; return false; }
            } else if (cols[c].storage == 0x30) {
                row[c] = cols[c].constant;
            }
        }
    }
    return true;
}

// ------------------------------------------------------------
//  Dateizugriff mit 64-Bit-Offsets - die Film-Archive sind
//  ueber 4 GB gross.
// ------------------------------------------------------------
struct FileCloser { void operator()(FILE* f) const { if (f) std::fclose(f); } };
using FilePtr = std::unique_ptr<FILE, FileCloser>;

FilePtr OpenFile(const std::wstring& path) {
#ifdef _WIN32
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0) f = nullptr;
    return FilePtr(f);
#else
    return FilePtr(std::fopen(std::string(path.begin(), path.end()).c_str(), "rb"));
#endif
}

bool ReadAt(FILE* f, uint64_t offset, size_t size, std::vector<uint8_t>& out) {
#ifdef _WIN32
    if (_fseeki64(f, static_cast<long long>(offset), SEEK_SET) != 0) return false;
#else
    if (fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0) return false;
#endif
    out.resize(size);
    return size == 0 || std::fread(out.data(), 1, size, f) == size;
}

// Tabelle hinter einem 16-Byte-Kopf ("CPK ", "TOC ", "ITOC") lesen.
bool ReadTableAt(FILE* f, uint64_t offset, const char* magic, UtfTable& t, std::string& error) {
    std::vector<uint8_t> head;
    if (!ReadAt(f, offset, 24, head)) { error = std::string(magic) + ": nicht lesbar"; return false; }
    if (!Magic(head.data(), head.size(), magic) || !Magic(head.data() + 16, 8, "@UTF")) {
        error = std::string("kein ") + magic + "-Block an der erwarteten Stelle";
        return false;
    }
    const uint32_t size = Be32(head.data() + 20);
    std::vector<uint8_t> table;
    if (!ReadAt(f, offset + 16, size + 8, table)) { error = std::string(magic) + ": Tabelle abgeschnitten"; return false; }
    return ParseUtf(table.data(), table.size(), t, error);
}

// Wie ReadTableAt, aber aus einem Puffer.
bool TableInBuffer(const std::vector<uint8_t>& b, uint64_t offset, const char* magic, UtfTable& t,
                   std::vector<uint8_t>* raw, std::string& error) {
    if (offset + 24 > b.size() || !Magic(b.data() + offset, 4, magic) ||
        !Magic(b.data() + offset + 16, 4, "@UTF")) {
        error = std::string("kein ") + magic + "-Block im inneren CPK";
        return false;
    }
    const uint8_t* p = b.data() + offset + 16;
    if (raw != nullptr) raw->assign(p, b.data() + b.size());
    return ParseUtf(p, b.size() - static_cast<size_t>(offset) - 16, t, error);
}

// FileOffset im TOC zaehlt ab dem kleinsten der beiden Bereiche,
// hoechstens ab 0x800 (so wie CRIs eigene Werkzeuge).
uint64_t TocBase(uint64_t tocOffset, uint64_t contentOffset) {
    uint64_t base = std::min<uint64_t>(tocOffset, 0x800);
    if (contentOffset != 0) base = std::min(base, contentOffset);
    return base;
}

// ------------------------------------------------------------
//  Eine Lage abschaelen. Liefert true, wenn sich etwas geaendert
//  hat; false mit leerem error heisst "nichts mehr zu tun".
// ------------------------------------------------------------
bool UnwrapPiece(std::vector<uint8_t>& b, std::string& error);

bool UnwrapInnerCpk(std::vector<uint8_t>& b, std::string& error) {
    UtfTable head;
    if (!TableInBuffer(b, 0, "CPK ", head, nullptr, error) || head.rows.empty()) return false;
    const uint64_t tocOffset     = head.U(0, "TocOffset");
    const uint64_t itocOffset    = head.U(0, "ItocOffset");
    const uint64_t contentOffset = head.U(0, "ContentOffset");
    const uint64_t align         = std::max<uint64_t>(1, head.U(0, "Align"));

    std::vector<uint8_t> out;
    if (tocOffset != 0) {
        // Mit Namen - hier nur der Fall "eine Datei im Archiv".
        UtfTable toc;
        if (!TableInBuffer(b, tocOffset, "TOC ", toc, nullptr, error)) return false;
        if (toc.rows.size() != 1) { error = "inneres CPK mit mehr als einer Datei"; return false; }
        const uint64_t off  = toc.U(0, "FileOffset") + TocBase(tocOffset, contentOffset);
        const uint64_t size = toc.U(0, "FileSize");
        if (off + size > b.size()) { error = "inneres CPK: Datei ausserhalb"; return false; }
        out.assign(b.begin() + static_cast<std::ptrdiff_t>(off), b.begin() + static_cast<std::ptrdiff_t>(off + size));
        if (!UnwrapPiece(out, error) && !error.empty()) return false;
    } else if (itocOffset != 0) {
        // Ohne Namen: Stuecke nach ID, ab ContentOffset, ausgerichtet.
        UtfTable itoc;
        std::vector<uint8_t> raw;
        if (!TableInBuffer(b, itocOffset, "ITOC", itoc, &raw, error) || itoc.rows.empty()) return false;
        std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> pieces;   // id, size, extract
        for (const char* key : { "DataL", "DataH" }) {
            const UtfValue* v = itoc.Get(0, key);
            if (v == nullptr || v->kind != UtfValue::kData || v->dataSize == 0) continue;
            if (v->dataOffset + v->dataSize > raw.size()) { error = "ITOC: Teiltabelle ausserhalb"; return false; }
            UtfTable part;
            if (!ParseUtf(raw.data() + v->dataOffset, static_cast<size_t>(v->dataSize), part, error)) return false;
            for (size_t r = 0; r < part.rows.size(); ++r)
                pieces.emplace_back(part.U(r, "ID"), part.U(r, "FileSize"), part.U(r, "ExtractSize"));
        }
        std::sort(pieces.begin(), pieces.end());
        uint64_t pos = contentOffset;
        for (const auto& pc : pieces) {
            const uint64_t size = std::get<1>(pc);
            if (pos + size > b.size()) { error = "ITOC: Stueck ausserhalb"; return false; }
            std::vector<uint8_t> piece(b.begin() + static_cast<std::ptrdiff_t>(pos),
                                       b.begin() + static_cast<std::ptrdiff_t>(pos + size));
            if (!UnwrapPiece(piece, error) && !error.empty()) return false;
            if (piece.size() != std::get<2>(pc)) { error = "ITOC: Stueck hat nach dem Auspacken die falsche Groesse"; return false; }
            out.insert(out.end(), piece.begin(), piece.end());
            pos += (size + align - 1) / align * align;
        }
    } else {
        error = "inneres CPK ohne Inhaltsverzeichnis";
        return false;
    }
    b.swap(out);
    return true;
}

// Entschluesseln und CRILAYLA - die Huellen eines einzelnen Stuecks.
bool UnwrapPiece(std::vector<uint8_t>& b, std::string& error) {
    bool changed = false;
    if (LooksEncrypted(b.data(), b.size())) {
        DecryptStormData(b.data(), b.size());
        changed = true;
    }
    if (Magic(b.data(), b.size(), "CRILAYLA")) {
        std::vector<uint8_t> out;
        if (!DecompressCrilayla(b.data(), b.size(), out, error)) return false;
        b.swap(out);
        changed = true;
    }
    return changed;
}

} // namespace

// ============================================================
//  Verschluesselung (Storm 4, Storm Connections)
//
//  Jede Datei ist ab ihrem ersten Byte mit demselben Strom
//  XOR-verknuepft: 32-Bit-Woerter eines xorshift128-Generators,
//  little-endian. Die vier Startwoerter sind zugleich die ersten
//  vier Ausgaben; danach laeuft der Generator weiter.
// ============================================================
void DecryptStormData(uint8_t* d, size_t n) {
    static const uint32_t kStart[4] = { 0x0c789dccu, 0x90d91837u, 0xcdb024cfu, 0x58eb75fdu };
    uint32_t x = kStart[0], y = kStart[1], z = kStart[2], w = kStart[3];
    size_t i = 0;
    for (size_t word = 0; i < n; ++word) {
        uint32_t k;
        if (word < 4) {
            k = kStart[word];
        } else {
            const uint32_t t = x ^ (x << 11);
            x = y; y = z; z = w;
            w = w ^ (w >> 19) ^ t ^ (t >> 8);
            k = w;
        }
        for (int b = 0; b < 4 && i < n; ++b, ++i) d[i] ^= static_cast<uint8_t>(k >> (8 * b));
    }
}

bool LooksEncrypted(const uint8_t* d, size_t n) {
    if (n < 8) return false;
    if (Magic(d, n, "NUCC") || Magic(d, n, "CRILAYLA") || Magic(d, n, "CPK ")) return false;
    uint8_t probe[8];
    std::memcpy(probe, d, 8);
    DecryptStormData(probe, 8);
    return Magic(probe, 8, "NUCC") || Magic(probe, 8, "CRILAYLA") || Magic(probe, 8, "CPK ");
}

// ============================================================
//  CRILAYLA
//
//  Aufbau: "CRILAYLA", u32 Groesse ohne Kopf, u32 Offset des
//  Kopfes (ab +0x10), dann der gepackte Strom, am Ende 0x100
//  Bytes ungepackter Dateikopf. Der Strom wird von hinten nach
//  vorn gelesen und schreibt die Ausgabe ebenfalls von hinten:
//    Bit 0  -> 8 Bit Literal
//    Bit 1  -> Rueckverweis: 13 Bit Abstand (+3), Laenge 3 plus
//              Stufen von 2, 3, 5, 8 Bit, danach 8-Bit-Bloecke,
//              solange ein Block voll (255) ist.
// ============================================================
bool DecompressCrilayla(const uint8_t* d, size_t n, std::vector<uint8_t>& out, std::string& error) {
    if (n < 0x110 || !Magic(d, n, "CRILAYLA")) { error = "kein CRILAYLA-Strom"; return false; }
    const uint32_t usize = Le32(d + 8);
    const uint32_t hoff  = Le32(d + 12);
    if (static_cast<uint64_t>(hoff) + 0x10 + 0x100 > n) { error = "CRILAYLA: Kopf ausserhalb"; return false; }
    if (usize > (1u << 30)) { error = "CRILAYLA: unplausible Groesse"; return false; }

    out.assign(static_cast<size_t>(usize) + 0x100, 0);
    std::memcpy(out.data(), d + hoff + 0x10, 0x100);

    std::ptrdiff_t pos = static_cast<std::ptrdiff_t>(n) - 0x100 - 1;
    uint32_t pool = 0;
    int left = 0;
    bool broken = false;
    auto bits = [&](int count) -> uint32_t {
        uint32_t v = 0;
        int got = 0;
        while (got < count) {
            if (left == 0) {
                if (pos < 0x10) { broken = true; return 0; }
                pool = d[pos--];
                left = 8;
            }
            const int take = std::min(left, count - got);
            v = (v << take) | ((pool >> (left - take)) & ((1u << take) - 1u));
            left -= take;
            got += take;
        }
        return v;
    };

    const size_t end = 0x100 + static_cast<size_t>(usize) - 1;
    size_t done = 0;
    static const int kLevels[4] = { 2, 3, 5, 8 };
    while (done < usize) {
        if (bits(1) != 0) {
            size_t ref = end - done + bits(13) + 3;
            size_t len = 3;
            int level = 0;
            for (; level < 4; ++level) {
                const uint32_t t = bits(kLevels[level]);
                len += t;
                if (t != (1u << kLevels[level]) - 1u) break;
            }
            if (level == 4) {
                uint32_t t;
                do { t = bits(8); len += t; } while (t == 255 && !broken);
            }
            if (broken) break;
            if (ref >= out.size() || len > usize - done) { error = "CRILAYLA: Rueckverweis ausserhalb"; return false; }
            for (size_t i = 0; i < len; ++i) {
                out[end - done] = out[ref];
                --ref;
                ++done;
            }
        } else {
            const uint32_t lit = bits(8);
            if (broken) break;
            out[end - done] = static_cast<uint8_t>(lit);
            ++done;
        }
    }
    if (broken) { error = "CRILAYLA: Strom zu kurz"; return false; }
    return true;
}

bool UnwrapPayload(std::vector<uint8_t>& data, std::string& error) {
    error.clear();
    // Hoechstens acht Lagen - mehr als drei kommen nicht vor, die
    // Grenze verhindert nur eine Endlosschleife bei kaputten Daten.
    for (int round = 0; round < 8; ++round) {
        const uint8_t* p = data.data();
        const size_t   n = data.size();
        if (Magic(p, n, "NUCC")) return true;
        if (LooksEncrypted(p, n)) {
            DecryptStormData(data.data(), n);
        } else if (Magic(p, n, "CPK ")) {
            if (!UnwrapInnerCpk(data, error)) return false;
        } else if (Magic(p, n, "CRILAYLA")) {
            std::vector<uint8_t> out;
            if (!DecompressCrilayla(p, n, out, error)) return false;
            data.swap(out);
        } else {
            return true;   // keine bekannte Huelle mehr - so lassen
        }
    }
    return true;
}

// ============================================================
//  CpkArchive
// ============================================================
bool CpkArchive::Open(const std::wstring& path, std::string& error) {
    path_ = path;
    entries_.clear();
    FilePtr f = OpenFile(path);
    if (!f) { error = "Archiv laesst sich nicht oeffnen"; return false; }

    UtfTable head;
    if (!ReadTableAt(f.get(), 0, "CPK ", head, error) || head.rows.empty()) return false;
    const uint64_t tocOffset     = head.U(0, "TocOffset");
    const uint64_t contentOffset = head.U(0, "ContentOffset");
    if (tocOffset == 0) { error = "Archiv ohne Dateinamen (kein TOC)"; return false; }

    UtfTable toc;
    if (!ReadTableAt(f.get(), tocOffset, "TOC ", toc, error)) return false;
    const uint64_t base = TocBase(tocOffset, contentOffset);
    entries_.reserve(toc.rows.size());
    for (size_t r = 0; r < toc.rows.size(); ++r) {
        CpkEntry e;
        e.dir         = toc.S(r, "DirName");
        e.name        = toc.S(r, "FileName");
        e.size        = toc.U(r, "FileSize");
        e.extractSize = toc.U(r, "ExtractSize");
        e.offset      = toc.U(r, "FileOffset") + base;
        if (e.extractSize == 0) e.extractSize = e.size;
        entries_.push_back(std::move(e));
    }
    return true;
}

bool CpkArchive::ReadRaw(const CpkEntry& e, std::vector<uint8_t>& out, std::string& error) const {
    FilePtr f = OpenFile(path_);
    if (!f) { error = "Archiv laesst sich nicht oeffnen"; return false; }
    if (e.size > (1ull << 31)) { error = "Eintrag zu gross"; return false; }
    if (!ReadAt(f.get(), e.offset, static_cast<size_t>(e.size), out)) { error = "Eintrag nicht lesbar: " + e.Path(); return false; }
    return true;
}

bool CpkArchive::Read(const CpkEntry& e, std::vector<uint8_t>& out, std::string& error) const {
    if (!ReadRaw(e, out, error)) return false;
    if (!UnwrapPayload(out, error)) {
        error = e.Path() + ": " + error;
        return false;
    }
    return true;
}

} // namespace xfbin

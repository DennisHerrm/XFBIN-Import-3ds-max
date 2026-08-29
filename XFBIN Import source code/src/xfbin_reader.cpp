// ============================================================
//  XFBIN Container Reader - Implementierung
//
//  Referenz: xfbin_lib/xfbin/structure/br/br_xfbin.py aus dem
//  Blender-XFBIN-Importer 2.5.2. Die Reihenfolge der Lesevorgaenge
//  ist absichtlich 1:1 uebernommen, damit sich die Dumps beider
//  Implementierungen direkt diffen lassen.
// ============================================================

#include "xfbin_reader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>

namespace xfbin {

namespace {

// ------------------------------------------------------------
//  Big-Endian-Leser auf einem zusammenhaengenden Puffer.
//
//  Bewusst kein std::istream und kein memcpy-auf-struct: XFBIN ist
//  durchgehend Big-Endian, x86 ist Little-Endian, und die
//  Feldbreiten sind nicht immer natuerlich ausgerichtet. Jeder
//  Zugriff geht deshalb byteweise ueber den Puffer.
//
//  Jeder Lesevorgang prueft die Grenze. Faellt er durch, wird
//  failed_ gesetzt und ab da liefern alle Reads 0 - so muss nicht
//  hinter jedem einzelnen Read ein if stehen, und trotzdem laeuft
//  nichts aus dem Puffer heraus.
// ------------------------------------------------------------
class BeReader {
public:
    BeReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool   failed() const { return failed_; }
    size_t pos()    const { return pos_; }
    size_t size()   const { return size_; }
    bool   eof()    const { return pos_ >= size_; }

    void seek(size_t p) {
        if (p > size_) { failed_ = true; return; }
        pos_ = p;
    }

    void skip(size_t n) {
        if (!Check(n)) return;
        pos_ += n;
    }

    // Richtet die absolute Position aus - identisch zu
    // BinaryReader.align_pos() der Python-Lib.
    void alignPos(size_t alignment) {
        const size_t rest = pos_ % alignment;
        if (rest) skip(alignment - rest);
    }

    uint8_t u8() {
        if (!Check(1)) return 0;
        return data_[pos_++];
    }

    uint16_t u16() {
        if (!Check(2)) return 0;
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(data_[pos_]) << 8) |
             static_cast<uint16_t>(data_[pos_ + 1]));
        pos_ += 2;
        return v;
    }

    uint32_t u32() {
        if (!Check(4)) return 0;
        const uint32_t v =
            (static_cast<uint32_t>(data_[pos_])     << 24) |
            (static_cast<uint32_t>(data_[pos_ + 1]) << 16) |
            (static_cast<uint32_t>(data_[pos_ + 2]) <<  8) |
             static_cast<uint32_t>(data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }

    // Nullterminierter String. Gibt die rohen Bytes zurueck
    // (cp932), ohne Terminator.
    RawString cstr() {
        RawString s;
        while (pos_ < size_) {
            const uint8_t c = data_[pos_++];
            if (c == 0) return s;
            s.push_back(static_cast<char>(c));
        }
        failed_ = true;          // kein Terminator bis Puffer-Ende
        return s;
    }

    RawString fixedStr(size_t n) {
        if (!Check(n)) return RawString();
        RawString s(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return s;
    }

    bool bytes(std::vector<uint8_t>& out, size_t n) {
        if (!Check(n)) return false;
        out.assign(data_ + pos_, data_ + pos_ + n);
        pos_ += n;
        return true;
    }

private:
    bool Check(size_t n) {
        if (failed_) return false;
        if (pos_ + n > size_) { failed_ = true; return false; }
        return true;
    }

    const uint8_t* data_ = nullptr;
    size_t         size_ = 0;
    size_t         pos_  = 0;
    bool           failed_ = false;
};

// Nicht-ASCII und die Sonderzeichen des Dump-Formats escapen.
std::string Escape(const RawString& s) {
    std::string out;
    out.reserve(s.size());
    static const char* kHex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c >= 0x20 && c < 0x7F) {
            out.push_back(static_cast<char>(c));
        } else {
            out += "\\x";
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

const RawString kEmptyString;

const RawString* At(const std::vector<RawString>& v, uint32_t i) {
    return (i < v.size()) ? &v[i] : &kEmptyString;
}

} // namespace

// ============================================================
//  XfbinFile
// ============================================================

size_t XfbinFile::TotalChunks() const {
    size_t n = 0;
    for (const XfbinPage& p : pages) n += p.chunks.size();
    return n;
}

size_t XfbinFile::CountOfType(const char* typeName) const {
    size_t n = 0;
    for (const XfbinPage& p : pages) {
        for (const XfbinChunk& c : p.chunks) {
            if (c.type && *c.type == typeName) ++n;
        }
    }
    return n;
}

// ============================================================
//  Lesen
// ============================================================

ReadResult ReadXfbinBuffer(const uint8_t* data, size_t size, XfbinFile& out) {
    ReadResult res;
    out = XfbinFile();

    if (!data || size < 28) {
        res.error = "Datei ist zu klein fuer einen XFBIN-Header.";
        return res;
    }

    BeReader br(data, size);

    // ---------- Header ----------
    const RawString magic = br.fixedStr(4);
    if (magic != "NUCC") {
        if (magic == "CPK ") {
            res.error = "Ungueltige Magic: die Datei ist CPK-komprimiert "
                        "und muss zuerst entpackt werden.";
        } else {
            res.error = "Ungueltige Magic '" + Escape(magic) +
                        "' - erwartet wurde 'NUCC'.";
        }
        return res;
    }

    out.header.nuccId = br.u32();
    br.skip(8);                       // Padding
    out.header.chunkTableSize = br.u32();
    out.header.minPageSize    = br.u32();
    out.header.nuccId2        = br.u16();
    out.header.unk            = br.u16();

    // ---------- Chunk-Tabelle ----------
    const uint32_t typeCount  = br.u32();
    br.u32();                         // typeSize - nur informativ
    const uint32_t pathCount  = br.u32();
    br.u32();                         // pathSize
    const uint32_t nameCount  = br.u32();
    br.u32();                         // nameSize
    const uint32_t mapCount   = br.u32();
    br.u32();                         // mapSize
    const uint32_t indexCount = br.u32();
    const uint32_t refCount   = br.u32();

    if (br.failed()) {
        res.error = "Datei bricht schon in der Tabellen-Kopfzeile ab.";
        return res;
    }

    // Plausibilitaet, bevor reserviert wird: eine kaputte Datei darf
    // nicht dazu fuehren, dass hier Gigabytes angefordert werden.
    const uint64_t worstCase =
        static_cast<uint64_t>(typeCount) + pathCount + nameCount +
        static_cast<uint64_t>(mapCount) + indexCount + refCount;
    if (worstCase > size) {
        res.error = "Tabellen-Kopfzeile ist unplausibel (Eintraege > Dateigroesse). "
                    "Vermutlich keine XFBIN-Datei oder beschaedigt.";
        return res;
    }

    ChunkTable& tbl = out.table;
    tbl.types.reserve(typeCount);
    tbl.paths.reserve(pathCount);
    tbl.names.reserve(nameCount);

    for (uint32_t i = 0; i < typeCount && !br.failed(); ++i)
        tbl.types.push_back(br.cstr());
    for (uint32_t i = 0; i < pathCount && !br.failed(); ++i)
        tbl.paths.push_back(br.cstr());
    for (uint32_t i = 0; i < nameCount && !br.failed(); ++i)
        tbl.names.push_back(br.cstr());

    // Nach den drei String-Bloecken wird ausgerichtet - nicht nach
    // jedem einzelnen. Genau so macht es br_xfbin.py.
    br.alignPos(4);

    tbl.maps.resize(mapCount);
    for (uint32_t i = 0; i < mapCount && !br.failed(); ++i) {
        tbl.maps[i].typeIndex = br.u32();
        tbl.maps[i].pathIndex = br.u32();
        tbl.maps[i].nameIndex = br.u32();
    }

    // Die Referenzen liegen ZWISCHEN den Maps und den Map-Indices.
    tbl.references.resize(refCount);
    for (uint32_t i = 0; i < refCount && !br.failed(); ++i) {
        tbl.references[i].nameIndex = br.u32();
        tbl.references[i].mapIndex  = br.u32();
    }

    tbl.mapIndices.resize(indexCount);
    for (uint32_t i = 0; i < indexCount && !br.failed(); ++i)
        tbl.mapIndices[i] = br.u32();

    if (br.failed()) {
        res.error = "Chunk-Tabelle ist unvollstaendig - Datei abgeschnitten?";
        return res;
    }

    // ---------- Pages ----------
    // Die Page-Grenze steht nicht im Voraus fest: eine Page endet,
    // sobald ein Chunk vom Typ nuccChunkPage gelesen wurde. Dessen
    // pageSize/referenceSize schieben die Fenster in den globalen
    // Tabellen weiter.
    size_t pageStart      = 0;
    size_t referenceStart = 0;
    size_t warnCount      = 0;
    std::ostringstream warn;

    while (!br.eof() && !br.failed()) {
        XfbinPage page;
        bool sawPageChunk = false;

        while (!br.eof() && !br.failed()) {
            XfbinChunk chunk;

            const uint32_t chunkSize = br.u32();
            chunk.localMapIndex = br.u32();
            chunk.version       = br.u16();
            chunk.unk           = br.u16();
            chunk.dataOffset    = br.pos();

            if (br.failed()) break;

            if (chunkSize > size) {
                res.error = "Chunk-Groesse " + std::to_string(chunkSize) +
                            " liegt ausserhalb der Datei (Offset " +
                            std::to_string(chunk.dataOffset) + ").";
                return res;
            }
            if (!br.bytes(chunk.data, chunkSize)) break;

            // Typ aufloesen: maps[mapIndices[pageStart + localMapIndex]]
            const size_t idx = pageStart + chunk.localMapIndex;
            if (idx < tbl.mapIndices.size()) {
                const uint32_t g = tbl.mapIndices[idx];
                chunk.globalMapIndex = g;
                if (g < tbl.maps.size()) {
                    const ChunkMap& m = tbl.maps[g];
                    chunk.type = At(tbl.types, m.typeIndex);
                    chunk.path = At(tbl.paths, m.pathIndex);
                    chunk.name = At(tbl.names, m.nameIndex);
                } else if (warnCount < 20) {
                    ++warnCount;
                    warn << "Chunk verweist auf Map-Index " << g
                         << ", es gibt aber nur " << tbl.maps.size() << ".\n";
                }
            } else if (warnCount < 20) {
                ++warnCount;
                warn << "Chunk verweist auf Indexposition " << idx
                     << ", die Indexliste hat nur "
                     << tbl.mapIndices.size() << " Eintraege.\n";
            }

            const bool isPageChunk =
                chunk.type && *chunk.type == "nuccChunkPage";

            if (isPageChunk) {
                BeReader pr(chunk.data.data(), chunk.data.size());
                page.pageSize      = pr.u32();
                page.referenceSize = pr.u32();
                sawPageChunk = true;
            }

            page.chunks.push_back(std::move(chunk));

            if (isPageChunk) break;
        }

        if (page.chunks.empty()) break;

        if (!sawPageChunk) {
            // Datei endet ohne abschliessenden nuccChunkPage. Die
            // gelesenen Chunks werden behalten - besser ein
            // unvollstaendiges Ergebnis als gar keins.
            warn << "Letzte Page endet ohne nuccChunkPage - Datei "
                    "vermutlich abgeschnitten.\n";
            out.pages.push_back(std::move(page));
            break;
        }

        // Fenster der globalen Tabellen fuer diese Page merken.
        const size_t idxEnd = std::min(pageStart + page.pageSize,
                                       tbl.mapIndices.size());
        if (pageStart <= idxEnd) {
            page.pageMapIndices.assign(tbl.mapIndices.begin() + static_cast<ptrdiff_t>(pageStart),
                                       tbl.mapIndices.begin() + static_cast<ptrdiff_t>(idxEnd));
        }
        const size_t refEnd = std::min(referenceStart + page.referenceSize,
                                       tbl.references.size());
        if (referenceStart <= refEnd) {
            page.pageReferences.assign(tbl.references.begin() + static_cast<ptrdiff_t>(referenceStart),
                                       tbl.references.begin() + static_cast<ptrdiff_t>(refEnd));
        }

        pageStart      += page.pageSize;
        referenceStart += page.referenceSize;

        out.pages.push_back(std::move(page));
    }

    if (out.pages.empty()) {
        res.error = "Keine Page gefunden - Datei ist leer oder beschaedigt.";
        return res;
    }

    res.ok = true;
    res.warnings = warn.str();
    return res;
}

ReadResult ReadXfbinFile(const std::string& path, XfbinFile& out) {
    ReadResult res;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        res.error = "Datei konnte nicht geoeffnet werden: " + path;
        return res;
    }

    const std::streamoff len = f.tellg();
    if (len <= 0) {
        res.error = "Datei ist leer: " + path;
        return res;
    }
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(static_cast<size_t>(len));
    if (!f.read(reinterpret_cast<char*>(buf.data()), len)) {
        res.error = "Datei konnte nicht vollstaendig gelesen werden: " + path;
        return res;
    }

    return ReadXfbinBuffer(buf.data(), buf.size(), out);
}

// ============================================================
//  Dump
// ============================================================

void WriteDump(const XfbinFile& file, const std::string& label,
               std::ostream& o, bool includeTables) {
    // Locale-fest ausgeben - siehe Num() in xfbin_clump.cpp.
    o.imbue(std::locale::classic());

    o << "# XFBIN dump v1\n";
    o << "file " << label << "\n";
    o << "header nuccId=" << file.header.nuccId
      << " chunkTableSize=" << file.header.chunkTableSize
      << " minPageSize="    << file.header.minPageSize
      << " nuccId2="        << file.header.nuccId2
      << " unk="            << file.header.unk << "\n";

    const ChunkTable& t = file.table;
    o << "table types=" << t.types.size()
      << " paths="      << t.paths.size()
      << " names="      << t.names.size()
      << " maps="       << t.maps.size()
      << " indices="    << t.mapIndices.size()
      << " refs="       << t.references.size() << "\n";

    if (includeTables) {
        for (size_t i = 0; i < t.types.size(); ++i)
            o << "type[" << i << "] " << Escape(t.types[i]) << "\n";
        for (size_t i = 0; i < t.paths.size(); ++i)
            o << "path[" << i << "] " << Escape(t.paths[i]) << "\n";
        for (size_t i = 0; i < t.names.size(); ++i)
            o << "name[" << i << "] " << Escape(t.names[i]) << "\n";
        for (size_t i = 0; i < t.maps.size(); ++i)
            o << "map[" << i << "] type=" << t.maps[i].typeIndex
              << " path=" << t.maps[i].pathIndex
              << " name=" << t.maps[i].nameIndex << "\n";
        for (size_t i = 0; i < t.references.size(); ++i)
            o << "ref[" << i << "] name=" << t.references[i].nameIndex
              << " map=" << t.references[i].mapIndex << "\n";
        for (size_t i = 0; i < t.mapIndices.size(); ++i)
            o << "idx[" << i << "] " << t.mapIndices[i] << "\n";
    }

    for (size_t p = 0; p < file.pages.size(); ++p) {
        const XfbinPage& page = file.pages[p];
        o << "page[" << p << "] chunks=" << page.chunks.size()
          << " pageSize="  << page.pageSize
          << " refSize="   << page.referenceSize << "\n";

        for (size_t c = 0; c < page.chunks.size(); ++c) {
            const XfbinChunk& ch = page.chunks[c];
            o << "chunk[" << p << "." << c << "]"
              << " type="  << Escape(ch.type ? *ch.type : kEmptyString)
              << " name="  << Escape(ch.name ? *ch.name : kEmptyString)
              << " path="  << Escape(ch.path ? *ch.path : kEmptyString)
              << " ver="   << ch.version
              << " unk="   << ch.unk
              << " size="  << ch.data.size()
              << " localMap=" << ch.localMapIndex
              << "\n";
        }
    }

    o << "end\n";
}

std::string MakeSummary(const XfbinFile& file) {
    // std::map statt unordered_map: die Ausgabe soll stabil sortiert
    // sein, sonst ist sie zwischen zwei Laeufen nicht vergleichbar.
    std::map<std::string, size_t> counts;
    for (const XfbinPage& p : file.pages)
        for (const XfbinChunk& c : p.chunks)
            ++counts[c.type ? *c.type : std::string("<unbekannt>")];

    std::ostringstream s;
    s << "Pages: " << file.pages.size()
      << " | Chunks: " << file.TotalChunks()
      << " | Typen: " << file.table.types.size()
      << " | Namen: " << file.table.names.size() << "\n";
    for (const auto& kv : counts)
        s << "  " << kv.first << " = " << kv.second << "\n";
    return s.str();
}

} // namespace xfbin

// ============================================================
//  XFBIN Texturen und Materialien - Implementierung
//
//  Referenz: br_nut.py, br_nucc.py (BrNuccChunkTexture,
//  BrNuccChunkMaterial), nucc.py und dds.py aus xfbin_lib.
// ============================================================

#include "xfbin_tex.h"

#include <cstring>
#include <fstream>
#include <locale>
#include <ostream>
#include <sstream>

namespace xfbin {

namespace {

class TexReader {
public:
    TexReader(const uint8_t* d, size_t n) : data_(d), size_(n) {}
    TexReader(const std::vector<uint8_t>& v) : data_(v.data()), size_(v.size()) {}

    bool   failed() const { return failed_; }
    size_t pos()    const { return pos_; }
    size_t size()   const { return size_; }

    void seek(size_t p) { if (p > size_) { failed_ = true; return; } pos_ = p; }
    void skip(size_t n) { if (Check(n)) pos_ += n; }

    void align(size_t a) {
        const size_t rest = pos_ % a;
        if (rest) skip(a - rest);
    }

    uint8_t u8() { if (!Check(1)) return 0; return data_[pos_++]; }

    uint16_t u16() {
        if (!Check(2)) return 0;
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(data_[pos_]) << 8) | data_[pos_ + 1]);
        pos_ += 2;
        return v;
    }
    int16_t i16() { return static_cast<int16_t>(u16()); }

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
    int32_t i32() { return static_cast<int32_t>(u32()); }

    float f32() {
        const uint32_t b = u32();
        float f = 0.0f;
        std::memcpy(&f, &b, sizeof(f));
        return f;
    }

    RawString fixedStr(size_t n) {
        if (!Check(n)) return RawString();
        RawString s(reinterpret_cast<const char*>(data_ + pos_), n);
        pos_ += n;
        return s;
    }

    bool bytes(std::vector<uint8_t>& out, size_t n) {
        if (!Check(n)) return false;
        out.insert(out.end(), data_ + pos_, data_ + pos_ + n);
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
    size_t size_ = 0;
    size_t pos_  = 0;
    bool   failed_ = false;
};

std::string Escape(const RawString& s) {
    std::string out;
    static const char* kHex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c >= 0x20 && c < 0x7F) out.push_back(static_cast<char>(c));
        else { out += "\\x"; out.push_back(kHex[c >> 4]); out.push_back(kHex[c & 0x0F]); }
    }
    return out;
}

// ------------------------------------------------------------
//  Pixelformate.
//
//  Die Zahlen sind die des NUT-Formats, die Masken die der
//  entsprechenden DDS-Pixelformate. Uebernommen aus dds.py.
// ------------------------------------------------------------
struct PixelFormatInfo {
    bool     valid       = false;
    bool     compressed  = false;
    char     fourCC[5]   = { 0, 0, 0, 0, 0 };
    uint32_t bitsPerPixel = 0;
    uint32_t maskR = 0, maskG = 0, maskB = 0, maskA = 0;
    int      swapWidth = 0;         // 0 = nicht drehen, 2 oder 4 = Bytes je Wert
};

PixelFormatInfo GetPixelFormat(uint8_t fmt) {
    PixelFormatInfo p;

    switch (fmt) {
    case 0:  p.valid = true; p.compressed = true; std::memcpy(p.fourCC, "DXT1", 4); break;
    case 1:  p.valid = true; p.compressed = true; std::memcpy(p.fourCC, "DXT3", 4); break;
    case 2:  p.valid = true; p.compressed = true; std::memcpy(p.fourCC, "DXT5", 4); break;

    case 6:  // A1R5G5B5
        p.valid = true; p.bitsPerPixel = 16; p.swapWidth = 2;
        p.maskR = 0x7C00; p.maskG = 0x03E0; p.maskB = 0x001F; p.maskA = 0x8000;
        break;
    case 7:  // A4R4G4B4
        p.valid = true; p.bitsPerPixel = 16; p.swapWidth = 2;
        p.maskR = 0x0F00; p.maskG = 0x00F0; p.maskB = 0x000F; p.maskA = 0xF000;
        break;
    case 8:  // R5G6B5, kein Alpha
        p.valid = true; p.bitsPerPixel = 16; p.swapWidth = 2;
        p.maskR = 0xF800; p.maskG = 0x07E0; p.maskB = 0x001F; p.maskA = 0;
        break;
    case 14: // X8R8G8B8
        p.valid = true; p.bitsPerPixel = 32; p.swapWidth = 4;
        p.maskR = 0x00FF0000; p.maskG = 0x0000FF00; p.maskB = 0x000000FF; p.maskA = 0;
        break;
    case 17: // A8R8G8B8
        p.valid = true; p.bitsPerPixel = 32; p.swapWidth = 4;
        p.maskR = 0x00FF0000; p.maskG = 0x0000FF00; p.maskB = 0x000000FF;
        p.maskA = 0xFF000000;
        break;
    default:
        break;
    }
    return p;
}

// Little-Endian schreiben - DDS ist LE, unabhaengig davon, dass
// alles andere in dieser Datei BE ist.
void PutU32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>( x        & 0xFF));
    v.push_back(static_cast<uint8_t>((x >>  8) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
}

} // namespace

RawString XfbinMaterial::DiffuseTexture() const {
    for (const MaterialTextureGroup& g : groups) {
        if (!g.textureNames.empty()) return g.textureNames[0];
    }
    return RawString();
}

// ============================================================
//  NUT
// ============================================================

namespace {

bool ReadNut(const std::vector<uint8_t>& data, XfbinTexture& out, std::string& why) {
    TexReader br(data);

    const RawString magic = br.fixedStr(4);
    if (magic != "NTP3") {
        why = "NUT-Magic ist nicht 'NTP3' (gelesen: '" + Escape(magic) + "')";
        return false;
    }

    const uint16_t version   = br.u16();
    const uint16_t texCount  = br.u16();
    br.u32();
    br.u32();

    if (br.failed()) { why = "NUT-Kopf abgeschnitten"; return false; }

    out.textures.resize(texCount);

    for (uint16_t i = 0; i < texCount && !br.failed(); ++i) {
        NutTexture& t = out.textures[i];

        br.u32();                              // totalSize
        br.u32();
        const uint32_t dataSize   = br.u32();
        br.u16();                              // headerSize
        br.u16();

        br.u8();
        t.mipmapCount = br.u8();
        br.u8();
        t.pixelFormat = br.u8();

        t.width  = br.u16();
        t.height = br.u16();

        br.u32();
        const uint32_t cubemapFormat = br.u32();
        t.isCubeMap = (cubemapFormat & 0x200) != 0;

        if (version < 0x200) {
            br.u32(); br.u32(); br.u32(); br.u32();
        } else {
            br.u32(); br.u32(); br.u32(); br.u32();
        }

        uint32_t cubeSize = 0;
        if (t.isCubeMap) {
            cubeSize = br.u32();
            br.u32(); br.u32(); br.u32();
        }

        if (t.mipmapCount > 1) {
            t.mipmapSizes.resize(t.mipmapCount);
            for (uint8_t m = 0; m < t.mipmapCount; ++m) t.mipmapSizes[m] = br.u32();
            br.align(16);
        }

        br.skip(0x18);                         // eXt und GIDX
        br.u32();                              // hashId
        br.u32();

        if (br.failed()) {
            why = "Texturkopf " + std::to_string(i) + " abgeschnitten";
            return false;
        }

        if (t.isCubeMap) {
            for (int f = 0; f < 6; ++f) br.bytes(t.data, cubeSize);
        } else if (t.mipmapCount > 1) {
            uint32_t sum = 0;
            for (uint32_t s : t.mipmapSizes) sum += s;

            if (sum != dataSize) {
                // Die Groessenangaben passen nicht zur
                // Datenmenge. Die Python-Lib behilft sich hier
                // damit, nur die erste Ebene zu nehmen - dasselbe
                // hier, damit die Dumps vergleichbar bleiben.
                br.bytes(t.data, t.mipmapSizes[0]);
                t.mipmapCount = 1;
                t.mipmapSizes.clear();
            } else {
                for (uint32_t s : t.mipmapSizes) br.bytes(t.data, s);
            }
        } else {
            br.bytes(t.data, dataSize);
        }

        if (br.failed()) {
            why = "Texturdaten " + std::to_string(i) + " abgeschnitten";
            return false;
        }
    }

    return true;
}

} // namespace

bool ParseTextures(const XfbinFile& file, std::vector<XfbinTexture>& out,
                   std::string& error, std::string& warnings) {
    out.clear(); error.clear(); warnings.clear();

    std::ostringstream warn;
    bool allOk = true;

    for (size_t pi = 0; pi < file.pages.size(); ++pi) {
        for (const XfbinChunk& chunk : file.pages[pi].chunks) {
            if (!chunk.type || *chunk.type != "nuccChunkTexture") continue;

            XfbinTexture tex;
            tex.name          = chunk.name ? *chunk.name : RawString();
            tex.pageIndex     = pi;
            tex.localMapIndex = chunk.localMapIndex;

            TexReader br(chunk.data);
            br.u16();                          // field00
            tex.width  = br.u16();
            tex.height = br.u16();
            br.u16();                          // field06
            const uint32_t nutSize = br.u32();

            if (br.failed() || nutSize == 0) {
                warn << "Textur '" << Escape(tex.name)
                     << "' hat keinen NUT-Block.\n";
                allOk = false;
                continue;
            }

            const size_t start = br.pos();
            if (start + nutSize > chunk.data.size()) {
                warn << "Textur '" << Escape(tex.name)
                     << "': NUT ragt ueber das Chunk-Ende hinaus.\n";
                allOk = false;
                continue;
            }

            std::vector<uint8_t> nut(chunk.data.begin() + static_cast<ptrdiff_t>(start),
                                     chunk.data.begin() + static_cast<ptrdiff_t>(start + nutSize));

            std::string why;
            if (!ReadNut(nut, tex, why)) {
                warn << "Textur '" << Escape(tex.name) << "': " << why << ".\n";
                allOk = false;
                continue;
            }

            out.push_back(std::move(tex));
        }
    }

    warnings = warn.str();
    if (out.empty()) {
        error = "Kein nuccChunkTexture in der Datei.";
        return false;
    }
    return allOk;
}

// ============================================================
//  Materialien
// ============================================================

namespace {

// Page-lokalen Chunkindex in einen Chunk-Namen aufloesen.
// Derselbe Weg wie bei den Kamera-Eintraegen der Animationen:
// der Wert ist eine Position in der Indexliste der Page, und
// erst der Eintrag dort ist ein Chunk-Map-Index.
RawString ResolveName(const XfbinFile& file, size_t pageIndex, uint32_t slot) {
    if (pageIndex >= file.pages.size()) return RawString();
    const XfbinPage& page = file.pages[pageIndex];

    if (slot >= page.pageMapIndices.size()) return RawString();
    const uint32_t mapIdx = page.pageMapIndices[slot];

    if (mapIdx >= file.table.maps.size()) return RawString();
    const uint32_t nameIdx = file.table.maps[mapIdx].nameIndex;

    if (nameIdx >= file.table.names.size()) return RawString();
    return file.table.names[nameIdx];
}

} // namespace

bool ParseMaterials(const XfbinFile& file, std::vector<XfbinMaterial>& out,
                    std::string& error, std::string& warnings) {
    out.clear(); error.clear(); warnings.clear();

    std::ostringstream warn;
    bool allOk = true;

    for (size_t pi = 0; pi < file.pages.size(); ++pi) {
        for (const XfbinChunk& chunk : file.pages[pi].chunks) {
            if (!chunk.type || *chunk.type != "nuccChunkMaterial") continue;

            XfbinMaterial mat;
            mat.name          = chunk.name ? *chunk.name : RawString();
            mat.pageIndex     = pi;
            mat.localMapIndex = chunk.localMapIndex;

            TexReader br(chunk.data);

            const uint16_t groupCount = br.u16();
            mat.alpha = br.u8();
            br.u8();
            mat.glare = br.f32();
            mat.flags = br.u32();

            // Die Felder haengen einzeln an Bits von flags. Wer
            // eines ueberliest, verschiebt alles danach.
            if (mat.flags & 0x01) for (int i = 0; i < 4; ++i) mat.uv0[i] = br.f32();
            if (mat.flags & 0x02) for (int i = 0; i < 4; ++i) mat.uv1[i] = br.f32();
            if (mat.flags & 0x04) for (int i = 0; i < 4; ++i) mat.uv2[i] = br.f32();
            if (mat.flags & 0x08) for (int i = 0; i < 4; ++i) mat.uv3[i] = br.f32();
            if (mat.flags & 0x10) { mat.blendRate = br.f32(); mat.blendType = br.f32(); }
            if (mat.flags & 0x20) mat.fallOff   = br.f32();
            if (mat.flags & 0x40) mat.outlineId = br.f32();

            mat.groups.resize(groupCount);
            for (uint16_t g = 0; g < groupCount && !br.failed(); ++g) {
                const int16_t texCount = br.i16();
                br.u16();
                mat.groups[g].unk = br.i32();

                for (int16_t t = 0; t < texCount && !br.failed(); ++t) {
                    mat.groups[g].textureNames.push_back(
                        ResolveName(file, pi, br.u32()));
                }
            }

            if (br.failed()) {
                warn << "Material '" << Escape(mat.name)
                     << "' ist abgeschnitten.\n";
                allOk = false;
            }

            out.push_back(std::move(mat));
        }
    }

    warnings = warn.str();
    if (out.empty()) {
        error = "Kein nuccChunkMaterial in der Datei.";
        return false;
    }
    return allOk;
}

std::vector<RawString> ResolveModelMaterials(const XfbinFile& file,
                                             size_t pageIndex,
                                             const std::vector<uint32_t>& materialIndices) {
    std::vector<RawString> names;
    names.reserve(materialIndices.size());
    for (uint32_t idx : materialIndices) {
        names.push_back(ResolveName(file, pageIndex, idx));
    }
    return names;
}

// ============================================================
//  DDS
// ============================================================

bool WriteDds(const NutTexture& tex, const std::string& path, std::string& why) {
    const PixelFormatInfo pf = GetPixelFormat(tex.pixelFormat);

    if (!pf.valid) {
        why = "unbekanntes Pixelformat " + std::to_string(tex.pixelFormat);
        return false;
    }
    if (tex.data.empty()) {
        why = "keine Bilddaten";
        return false;
    }

    std::vector<uint8_t> hdr;
    hdr.reserve(128);

    hdr.push_back('D'); hdr.push_back('D'); hdr.push_back('S'); hdr.push_back(' ');

    uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000;      // CAPS|HEIGHT|WIDTH|PIXELFORMAT
    flags |= pf.compressed ? 0x80000u : 0x8u;       // LINEARSIZE oder PITCH

    const uint32_t mipCount = (tex.mipmapCount > 1) ? tex.mipmapCount : 1u;
    if (mipCount > 1) flags |= 0x20000u;            // MIPMAPCOUNT

    uint32_t pitchOrLinear = 0;
    if (pf.compressed) {
        const uint32_t px = static_cast<uint32_t>(tex.width) * tex.height;
        pitchOrLinear = (std::strcmp(pf.fourCC, "DXT1") == 0) ? (px / 2) : px;
    } else {
        pitchOrLinear = static_cast<uint32_t>(tex.width) * (pf.bitsPerPixel / 8);
    }

    PutU32(hdr, 124);
    PutU32(hdr, flags);
    PutU32(hdr, tex.height);
    PutU32(hdr, tex.width);
    PutU32(hdr, pitchOrLinear);
    // dwDepth: laut Spezifikation nur bei Volumentexturen
    // benutzt, sonst frei. Die Python-Lib schreibt 1 - hier
    // dasselbe, damit die Dateien byteweise vergleichbar bleiben.
    PutU32(hdr, 1);                                  // depth
    PutU32(hdr, mipCount);
    for (int i = 0; i < 11; ++i) PutU32(hdr, 0);     // reserved

    // --- DDS_PIXELFORMAT, 32 Bytes ---
    PutU32(hdr, 32);
    if (pf.compressed) {
        PutU32(hdr, 0x4);                            // DDPF_FOURCC
        hdr.push_back(static_cast<uint8_t>(pf.fourCC[0]));
        hdr.push_back(static_cast<uint8_t>(pf.fourCC[1]));
        hdr.push_back(static_cast<uint8_t>(pf.fourCC[2]));
        hdr.push_back(static_cast<uint8_t>(pf.fourCC[3]));
        PutU32(hdr, 0);
        PutU32(hdr, 0); PutU32(hdr, 0); PutU32(hdr, 0); PutU32(hdr, 0);
    } else {
        uint32_t pfFlags = 0x40;                     // DDPF_RGB
        if (pf.maskA != 0) pfFlags |= 0x01;          // DDPF_ALPHAPIXELS
        PutU32(hdr, pfFlags);
        PutU32(hdr, 0);                              // kein FourCC
        PutU32(hdr, pf.bitsPerPixel);
        PutU32(hdr, pf.maskR);
        PutU32(hdr, pf.maskG);
        PutU32(hdr, pf.maskB);
        PutU32(hdr, pf.maskA);
    }

    uint32_t caps1 = 0x1000;                         // TEXTURE
    if (mipCount > 1) caps1 |= (0x8 | 0x400000);     // COMPLEX | MIPMAP
    if (tex.isCubeMap) caps1 |= 0x8;

    PutU32(hdr, caps1);
    PutU32(hdr, tex.isCubeMap ? 0xFE00u : 0u);       // CUBEMAP + alle Seiten
    PutU32(hdr, 0);
    PutU32(hdr, 0);
    PutU32(hdr, 0);

    // --- Bilddaten ---
    std::vector<uint8_t> pixels = tex.data;

    if (pf.swapWidth == 2) {
        for (size_t i = 0; i + 1 < pixels.size(); i += 2) {
            std::swap(pixels[i], pixels[i + 1]);
        }
    } else if (pf.swapWidth == 4) {
        for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
            std::swap(pixels[i],     pixels[i + 3]);
            std::swap(pixels[i + 1], pixels[i + 2]);
        }
    }

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        why = "Datei nicht schreibbar";
        return false;
    }
    f.write(reinterpret_cast<const char*>(hdr.data()),
            static_cast<std::streamsize>(hdr.size()));
    f.write(reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));

    return f.good();
}

namespace {

// Pfadtrenner. Windows nimmt auch '/', aber ein Werkzeug, das
// sich auch ausserhalb von Max bauen laesst, sollte auf beiden
// Seiten das Richtige tun - sonst entstehen unter Linux Dateien
// mit einem Backslash im Namen statt in einem Unterordner.
#ifdef _WIN32
const char kSep = '\\';
#else
const char kSep = '/';
#endif

// Zeichen, die in einem Dateinamen nichts verloren haben,
// ersetzen. Chunk-Namen duerfen Leerzeichen und cp932-Bytes
// enthalten - Leerzeichen bleiben, der Rest wird zu _.
std::string SafeFileName(const RawString& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
            c == '"'  || c == '<' || c == '>' || c == '|' || c < 0x20 || c >= 0x7F) {
            out.push_back('_');
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    if (out.empty()) out = "unbenannt";
    return out;
}

} // namespace

int ExportTextures(const std::vector<XfbinTexture>& textures,
                   const std::string& directory,
                   std::vector<std::string>& fileNames,
                   std::string& warnings) {
    fileNames.clear();
    fileNames.resize(textures.size());

    std::ostringstream warn;
    int written = 0;

    for (size_t i = 0; i < textures.size(); ++i) {
        const XfbinTexture& t = textures[i];
        const std::string base = SafeFileName(t.name);

        if (t.textures.empty()) {
            warn << "Textur '" << base << "' enthaelt kein Bild.\n";
            continue;
        }

        for (size_t k = 0; k < t.textures.size(); ++k) {
            std::string name = base;
            if (t.textures.size() > 1) name += "_" + std::to_string(k);
            name += ".dds";

            const std::string full = directory + kSep + name;

            std::string why;
            if (WriteDds(t.textures[k], full, why)) {
                ++written;
                // Nur die erste Teiltextur zaehlt als "die"
                // Textur des Chunks - die weiteren sind in den
                // bisher gesehenen Dateien Varianten desselben
                // Bildes.
                if (k == 0) fileNames[i] = name;
            } else {
                warn << "Textur '" << name << "': " << why << ".\n";
            }
        }
    }

    warnings = warn.str();
    return written;
}

std::string MakeTextureSummary(const std::vector<XfbinTexture>& textures,
                               const std::vector<XfbinMaterial>& materials) {
    std::ostringstream s;
    s.imbue(std::locale::classic());

    size_t images = 0;
    for (const XfbinTexture& t : textures) images += t.textures.size();

    s << "Texturen: " << textures.size() << " (" << images << " Bilder)"
      << " | Materialien: " << materials.size() << "\n";

    for (const XfbinTexture& t : textures) {
        s << "  " << t.name << ": ";
        for (size_t k = 0; k < t.textures.size(); ++k) {
            if (k) s << ", ";
            s << t.textures[k].width << "x" << t.textures[k].height
              << " fmt=" << static_cast<int>(t.textures[k].pixelFormat);
        }
        s << "\n";
    }

    for (const XfbinMaterial& m : materials) {
        s << "  " << m.name << " -> " << m.DiffuseTexture() << "\n";
    }

    return s.str();
}

} // namespace xfbin

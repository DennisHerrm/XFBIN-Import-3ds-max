// ============================================================
//  XFBIN Clump / Coord - Implementierung
//
//  Referenz: BrNuccChunkClump und BrNuccChunkCoord aus
//  xfbin_lib/xfbin/structure/br/br_nucc.py sowie make_armature()
//  aus blender/importer.py des Blender-XFBIN-Importers 2.5.2.
// ============================================================

#include "xfbin_clump.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <locale>
#include <ostream>
#include <sstream>

namespace xfbin {

namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

// ------------------------------------------------------------
//  Big-Endian-Leser auf einem Chunk-Datenblock.
//  Bewusst eine eigene, kleine Kopie statt den Leser aus
//  xfbin_reader.cpp zu exportieren: der ist dort ein
//  Implementierungsdetail und soll es bleiben.
// ------------------------------------------------------------
class ChunkReader {
public:
    ChunkReader(const std::vector<uint8_t>& data)
        : data_(data.data()), size_(data.size()) {}

    bool failed() const { return failed_; }

    void skip(size_t n) { if (Check(n)) pos_ += n; }

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

    // IEEE-754 aus Big-Endian. Der Umweg ueber memcpy statt eines
    // Zeiger-Casts vermeidet einen Verstoss gegen die
    // Aliasing-Regeln, den MSVC bei /O2 gnadenlos ausnutzt.
    float f32() {
        const uint32_t bits = u32();
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    void f32v(float* out, int count) {
        for (int i = 0; i < count; ++i) out[i] = f32();
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

} // namespace

// ============================================================
//  Matrizen
// ============================================================

Mat43 Mat43::Identity() {
    Mat43 r{};
    r.m[0][0] = 1.0; r.m[1][1] = 1.0; r.m[2][2] = 1.0;
    return r;
}

Mat43 Mat43::Translation(double x, double y, double z) {
    Mat43 r = Identity();
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
}

Mat43 Mat43::Scale(double x, double y, double z) {
    Mat43 r{};
    r.m[0][0] = x; r.m[1][1] = y; r.m[2][2] = z;
    return r;
}

// Zeilenvektor-Rotationsmatrizen: die Transponierte der
// gewohnten Spaltenform. Rechte-Hand-Regel, wie in der Max-UI.
Mat43 Mat43::RotationX(double deg) {
    const double a = deg * kDegToRad;
    const double c = std::cos(a), s = std::sin(a);
    Mat43 r = Identity();
    r.m[1][1] =  c; r.m[1][2] = s;
    r.m[2][1] = -s; r.m[2][2] = c;
    return r;
}

Mat43 Mat43::RotationY(double deg) {
    const double a = deg * kDegToRad;
    const double c = std::cos(a), s = std::sin(a);
    Mat43 r = Identity();
    r.m[0][0] = c; r.m[0][2] = -s;
    r.m[2][0] = s; r.m[2][2] =  c;
    return r;
}

Mat43 Mat43::RotationZ(double deg) {
    const double a = deg * kDegToRad;
    const double c = std::cos(a), s = std::sin(a);
    Mat43 r = Identity();
    r.m[0][0] =  c; r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}

Mat43 operator*(const Mat43& a, const Mat43& b) {
    Mat43 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[i][0] * b.m[0][j]
                      + a.m[i][1] * b.m[1][j]
                      + a.m[i][2] * b.m[2][j];
        }
    }
    for (int j = 0; j < 3; ++j) {
        r.m[3][j] = a.m[3][0] * b.m[0][j]
                  + a.m[3][1] * b.m[1][j]
                  + a.m[3][2] * b.m[2][j]
                  + b.m[3][j];
    }
    return r;
}

// Reihenfolge ZYX heisst: Z wird zuerst angewendet. In
// Zeilenschreibweise steht das zuerst Angewendete LINKS.
Mat43 MakeRotation(double rxDeg, double ryDeg, double rzDeg) {
    return Mat43::RotationZ(rzDeg)
         * Mat43::RotationY(ryDeg)
         * Mat43::RotationX(rxDeg);
}

Mat43 MakeRotationFromQuat(double x, double y, double z, double w) {
    // Normalisieren: die quantisierten Formate (int16 / 0x8000)
    // liefern nicht exakt Einheitslaenge.
    const double len = std::sqrt(x * x + y * y + z * z + w * w);
    if (len > 1e-12) { x /= len; y /= len; z /= len; w /= len; }

    Mat43 r = Mat43::Identity();

    r.m[0][0] = 1.0 - 2.0 * (y * y + z * z);
    r.m[0][1] =       2.0 * (x * y - w * z);
    r.m[0][2] =       2.0 * (x * z + w * y);

    r.m[1][0] =       2.0 * (x * y + w * z);
    r.m[1][1] = 1.0 - 2.0 * (x * x + z * z);
    r.m[1][2] =       2.0 * (y * z - w * x);

    r.m[2][0] =       2.0 * (x * z - w * y);
    r.m[2][1] =       2.0 * (y * z + w * x);
    r.m[2][2] = 1.0 - 2.0 * (x * x + y * y);

    return r;
}

Mat43 MakeLocal(const float pos[3], const float rot[3], const float scl[3]) {
    return Mat43::Scale(scl[0], scl[1], scl[2])
         * MakeRotation(rot[0], rot[1], rot[2])
         * Mat43::Translation(pos[0], pos[1], pos[2]);
}

// ============================================================
//  Clump
// ============================================================

int Clump::Depth(int nodeIndex) const {
    int d = 0;
    int i = nodeIndex;
    // Obergrenze gegen einen Zyklus in einer beschaedigten Datei.
    while (i >= 0 && d <= static_cast<int>(nodes.size())) {
        i = nodes[static_cast<size_t>(i)].parent;
        if (i >= 0) ++d;
    }
    return d;
}

namespace {

// Ein nuccChunkCoord auswerten.
bool ReadCoord(const XfbinChunk& chunk, CoordNode& out, std::string& why) {
    ChunkReader br(chunk.data);

    br.f32v(out.position, 3);
    br.f32v(out.rotation, 3);
    br.f32v(out.scale, 3);
    out.opacity = br.f32();

    // Das flags-Feld gibt es erst ab Version 0x66. Darunter endet
    // der Chunk nach der Deckkraft.
    if (chunk.version > 0x66) {
        out.flags = br.u16();
    } else {
        out.flags = 0;
    }

    if (br.failed()) {
        why = "Coord-Chunk ist zu kurz";
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        out.scaleSigns[i] = (out.scale[i] < 0.0f) ? -1 : 1;
    }

    out.name = chunk.name ? *chunk.name : RawString();
    return true;
}

} // namespace

bool ParseClumps(const XfbinFile& file, std::vector<Clump>& out,
                 std::string& error, std::string& warnings) {
    out.clear();
    error.clear();
    warnings.clear();

    std::ostringstream warn;
    bool allOk = true;

    for (size_t p = 0; p < file.pages.size(); ++p) {
        const XfbinPage& page = file.pages[p];

        // Die Coord-Chunks einer Page in Lesereihenfolge sammeln.
        // Der Clump verweist ueber Indizes in die Page-Indexliste,
        // nicht ueber Namen.
        for (const XfbinChunk& chunk : page.chunks) {
            if (!chunk.type || *chunk.type != "nuccChunkClump") continue;

            Clump clump;
            clump.name          = chunk.name ? *chunk.name : RawString();
            clump.pageIndex     = p;
            clump.localMapIndex = chunk.localMapIndex;

            ChunkReader br(chunk.data);

            clump.field00 = br.u32();

            const uint16_t coordCount = br.u16();
            clump.modelGroupCount = br.u8();
            clump.extraGroupCount = br.u8();

            if (clump.field00 == 2) {
                clump.hasBoundingBox = true;
                br.f32v(clump.boundingBox, 6);
                br.u32();                       // Fuellwort
            }

            if (br.failed() || coordCount == 0) {
                warn << "Clump '" << clump.name
                     << "' hat keine oder unlesbare Kopfdaten.\n";
                allOk = false;
                continue;
            }

            // Elternindizes sind vorzeichenbehaftet: die Wurzel
            // traegt -1.
            std::vector<int>      parents(coordCount);
            std::vector<uint32_t> coordIndices(coordCount);

            for (uint16_t i = 0; i < coordCount; ++i) parents[i] = br.i16();
            for (uint16_t i = 0; i < coordCount; ++i) coordIndices[i] = br.u32();

            if (br.failed()) {
                warn << "Clump '" << clump.name
                     << "': Index-Listen abgeschnitten.\n";
                allOk = false;
                continue;
            }

            // Chunk-Index (page-lokal) -> Chunk. Der Clump zeigt
            // mit coordIndices genau auf diese Nummern.
            clump.nodes.resize(coordCount);

            for (uint16_t i = 0; i < coordCount; ++i) {
                const uint32_t want = coordIndices[i];

                const XfbinChunk* found = nullptr;
                for (const XfbinChunk& c : page.chunks) {
                    if (c.localMapIndex == want && c.type &&
                        *c.type == "nuccChunkCoord") {
                        found = &c;
                        break;
                    }
                }

                if (found == nullptr) {
                    warn << "Clump '" << clump.name << "': Coord-Index "
                         << want << " zeigt auf keinen Coord-Chunk.\n";
                    allOk = false;
                    continue;
                }

                std::string why;
                if (!ReadCoord(*found, clump.nodes[i], why)) {
                    warn << "Clump '" << clump.name << "': "
                         << (found->name ? *found->name : RawString())
                         << " - " << why << ".\n";
                    allOk = false;
                }
            }

            // Hierarchie verdrahten.
            for (uint16_t i = 0; i < coordCount; ++i) {
                const int par = parents[i];

                if (par < 0) {
                    clump.nodes[i].parent = -1;
                    clump.roots.push_back(i);
                } else if (par >= static_cast<int>(coordCount)) {
                    warn << "Clump '" << clump.name << "': Knoten " << i
                         << " verweist auf Elternteil " << par
                         << ", es gibt aber nur " << coordCount << ".\n";
                    clump.nodes[i].parent = -1;
                    clump.roots.push_back(i);
                    allOk = false;
                } else if (par == static_cast<int>(i)) {
                    warn << "Clump '" << clump.name << "': Knoten " << i
                         << " ist sein eigener Elternteil.\n";
                    clump.nodes[i].parent = -1;
                    clump.roots.push_back(i);
                    allOk = false;
                } else {
                    clump.nodes[i].parent = par;
                    clump.nodes[static_cast<size_t>(par)].children.push_back(i);
                }
            }

            // Tiefensuche von den Wurzeln aus. Ergebnis: eine
            // Reihenfolge, in der jeder Elternteil vor seinen
            // Kindern steht.
            std::vector<bool> seen(coordCount, false);
            std::vector<int>  stack;

            for (size_t r = clump.roots.size(); r > 0; --r) {
                stack.push_back(clump.roots[r - 1]);
            }

            while (!stack.empty()) {
                const int idx = stack.back();
                stack.pop_back();

                if (seen[static_cast<size_t>(idx)]) continue;
                seen[static_cast<size_t>(idx)] = true;

                clump.depthFirst.push_back(idx);

                const std::vector<int>& kids =
                    clump.nodes[static_cast<size_t>(idx)].children;
                for (size_t k = kids.size(); k > 0; --k) {
                    stack.push_back(kids[k - 1]);
                }
            }

            if (clump.depthFirst.size() != coordCount) {
                warn << "Clump '" << clump.name << "': "
                     << (coordCount - clump.depthFirst.size())
                     << " Knoten sind von keiner Wurzel aus erreichbar "
                        "(Zyklus in der Hierarchie?).\n";
                allOk = false;

                // Nicht erreichbare Knoten trotzdem anhaengen, damit
                // sie nicht stillschweigend verschwinden.
                for (uint16_t i = 0; i < coordCount; ++i) {
                    if (!seen[i]) clump.depthFirst.push_back(i);
                }
            }

            // Matrizen. In depthFirst-Reihenfolge, damit der
            // Elternteil garantiert schon gerechnet ist.
            for (int idx : clump.depthFirst) {
                CoordNode& n = clump.nodes[static_cast<size_t>(idx)];
                n.local = MakeLocal(n.position, n.rotation, n.scale);

                if (n.parent < 0) {
                    n.world = n.local;
                } else {
                    // Zeilenvektoren: Kind LINKS, Elternteil RECHTS.
                    n.world = n.local *
                              clump.nodes[static_cast<size_t>(n.parent)].world;
                }
            }

            out.push_back(std::move(clump));
        }
    }

    warnings = warn.str();

    if (out.empty()) {
        error = "Kein nuccChunkClump in der Datei - sie enthaelt kein Skelett.";
        return false;
    }

    return allOk;
}

// ============================================================
//  Dump
// ============================================================

namespace {

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

// Feste Nachkommastellen. Ohne das lassen sich zwei
// Implementierungen nicht vergleichen - und -0 wird zu 0
// normalisiert, weil die beiden Seiten sich sonst genau dort
// unterscheiden, wo es egal ist.
//
// ACHTUNG, hier lag ein Fehler bis 0.2.0: die Umwandlung lief
// ueber snprintf("%.6f"). Das folgt der Prozess-Locale, und
// 3ds Max stellt die auf die Systemsprache um. Auf einem
// deutschen Windows kam deshalb "0,000000" statt "0.000000"
// heraus - der Dump war damit gegen die Python-Referenz nicht
// mehr vergleichbar, obwohl jede einzelne Zahl stimmte.
//
// std::to_chars ist per Definition locale-unabhaengig; genau
// dafuer gibt es die Funktion. Der Austausch danach ist ein
// Sicherheitsnetz und sollte nie greifen.
std::string Num(double v) {
    if (v == 0.0) v = 0.0;          // -0 zu 0 normalisieren

    char buf[64];
    const std::to_chars_result r =
        std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, 6);

    std::string out;
    if (r.ec == std::errc()) {
        out.assign(buf, r.ptr);
    } else {
        // Kann bei sechs Nachkommastellen und 64 Bytes Puffer
        // nicht passieren - aber lieber eine Zahl als gar nichts.
        std::snprintf(buf, sizeof(buf), "%.6f", v);
        out = buf;
    }

    for (char& c : out) {
        if (c == ',') c = '.';
    }
    return out;
}

} // namespace

void WriteBoneDump(const std::vector<Clump>& clumps, const std::string& label,
                   std::ostream& o) {
    // Auch die Ganzzahlen locale-fest ausgeben. Ein Stream
    // uebernimmt zwar nicht die C-Locale, koennte aber ueber
    // std::locale::global() eine mit Tausendertrennung
    // mitbekommen haben.
    o.imbue(std::locale::classic());

    o << "# XFBIN bone dump v1\n";
    o << "file " << label << "\n";
    o << "clumps " << clumps.size() << "\n";

    for (size_t c = 0; c < clumps.size(); ++c) {
        const Clump& cl = clumps[c];

        o << "clump[" << c << "] name=" << Escape(cl.name)
          << " nodes="  << cl.nodes.size()
          << " roots="  << cl.roots.size()
          << " modelGroups=" << cl.modelGroupCount
          << " extraGroups=" << cl.extraGroupCount
          << " field00=" << cl.field00 << "\n";

        // Nicht in depthFirst-Reihenfolge ausgeben, sondern in
        // Dateireihenfolge: die ist zwischen beiden
        // Implementierungen garantiert gleich, die Reihenfolge
        // einer Tiefensuche muesste man erst angleichen.
        for (size_t i = 0; i < cl.nodes.size(); ++i) {
            const CoordNode& n = cl.nodes[i];

            o << "bone[" << c << "." << i << "] name=" << Escape(n.name)
              << " parent=" << n.parent
              << " depth="  << cl.Depth(static_cast<int>(i))
              << " flags="  << n.flags
              << "\n";

            o << "  pos " << Num(n.position[0]) << " " << Num(n.position[1])
              << " "      << Num(n.position[2]) << "\n";
            o << "  rot " << Num(n.rotation[0]) << " " << Num(n.rotation[1])
              << " "      << Num(n.rotation[2]) << "\n";
            o << "  scl " << Num(n.scale[0]) << " " << Num(n.scale[1])
              << " "      << Num(n.scale[2])
              << "  opacity " << Num(n.opacity) << "\n";

            for (int r = 0; r < 4; ++r) {
                o << "  world" << r
                  << " " << Num(n.world.m[r][0])
                  << " " << Num(n.world.m[r][1])
                  << " " << Num(n.world.m[r][2]) << "\n";
            }
        }
    }

    o << "end\n";
}

std::string MakeBoneSummary(const std::vector<Clump>& clumps) {
    std::ostringstream s;

    s << "Clumps: " << clumps.size() << "\n";

    for (const Clump& cl : clumps) {
        int maxDepth = 0;
        for (size_t i = 0; i < cl.nodes.size(); ++i) {
            const int d = cl.Depth(static_cast<int>(i));
            if (d > maxDepth) maxDepth = d;
        }

        int negScale = 0;
        for (const CoordNode& n : cl.nodes) {
            if (n.scaleSigns[0] < 0 || n.scaleSigns[1] < 0 || n.scaleSigns[2] < 0) {
                ++negScale;
            }
        }

        s << "  " << cl.name
          << ": " << cl.nodes.size() << " Bones"
          << ", " << cl.roots.size() << " Wurzel(n)"
          << ", Tiefe " << maxDepth;
        if (negScale > 0) s << ", " << negScale << " mit negativer Skalierung";
        s << "\n";
    }

    return s.str();
}

} // namespace xfbin

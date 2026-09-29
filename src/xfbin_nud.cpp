// ============================================================
//  XFBIN NUD - Implementierung
//
//  Siehe xfbin_nud.h fuer die Beschreibung des Vertexformats.
// ============================================================

#include "xfbin_nud.h"

#include <charconv>
#include <cstring>
#include <locale>
#include <ostream>
#include <sstream>

namespace xfbin {

namespace {

// ------------------------------------------------------------
//  Big-Endian-Leser mit freier Positionierung.
//  Der NUD-Block ist voller Offsets, die kreuz und quer in
//  denselben Puffer zeigen - ohne seek() geht hier nichts.
// ------------------------------------------------------------
class NudReader {
public:
    NudReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool   failed() const { return failed_; }
    size_t pos()    const { return pos_; }
    size_t size()   const { return size_; }

    void seek(size_t p) {
        if (p > size_) { failed_ = true; return; }
        pos_ = p;
    }
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
    float f32() {
        const uint32_t bits = u32();
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }
    float f16() { return HalfToFloat(u16()); }

    RawString cstrAt(size_t offset) {
        RawString s;
        size_t p = offset;
        while (p < size_) {
            const uint8_t c = data_[p++];
            if (c == 0) return s;
            s.push_back(static_cast<char>(c));
        }
        return s;
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
    out.reserve(s.size());
    static const char* kHex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c >= 0x20 && c < 0x7F) out.push_back(static_cast<char>(c));
        else { out += "\\x"; out.push_back(kHex[c >> 4]); out.push_back(kHex[c & 0x0F]); }
    }
    return out;
}

// Locale-feste Zahlenausgabe - siehe Num() in xfbin_clump.cpp.
std::string Num(double v, int digits = 6) {
    if (v == 0.0) v = 0.0;
    char buf[64];
    const std::to_chars_result r =
        std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::fixed, digits);
    std::string out = (r.ec == std::errc()) ? std::string(buf, r.ptr) : std::string("0");
    for (char& c : out) if (c == ',') c = '.';
    return out;
}

} // namespace

// ============================================================
//  Halbgleitkomma
// ============================================================

float HalfToFloat(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h >> 15) & 1u;
    uint32_t exp  = static_cast<uint32_t>(h >> 10) & 0x1Fu;
    uint32_t mant = static_cast<uint32_t>(h) & 0x3FFu;

    uint32_t bits = 0;

    if (exp == 0) {
        if (mant == 0) {
            bits = sign << 31;                 // +/- 0
        } else {
            // Subnormale Halbzahl: normalisieren, bis das
            // implizite Bit gesetzt ist.
            exp = 127u - 15u + 1u;
            while ((mant & 0x400u) == 0u) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x3FFu;
            bits = (sign << 31) | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        bits = (sign << 31) | 0x7F800000u | (mant << 13);   // Inf / NaN
    } else {
        bits = (sign << 31) | ((exp - 15u + 127u) << 23) | (mant << 13);
    }

    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// ============================================================
//  NudModel
// ============================================================

size_t NudModel::VertexCount() const {
    size_t n = 0;
    for (const NudMeshGroup& g : groups)
        for (const NudMesh& m : g.meshes) n += m.VertexCount();
    return n;
}

size_t NudModel::TriangleCount() const {
    size_t n = 0;
    for (const NudMeshGroup& g : groups)
        for (const NudMesh& m : g.meshes) n += m.TriangleCount();
    return n;
}

// ============================================================
//  Streifen aufloesen
// ============================================================

namespace {

// Triangle-Strips mit 0xFFFF als Neustart-Marker in eine
// Dreiecksliste umwandeln.
//
// Die Wickelrichtung wechselt innerhalb eines Streifens mit
// jedem Dreieck; entartete Dreiecke (zwei gleiche Indizes)
// dienen nur zum Verbinden und fliegen raus.
void StripsToTriangles(const std::vector<uint16_t>& indices, NudMesh& mesh) {
    std::vector<uint32_t> strip;
    strip.reserve(64);

    mesh.stripCount = 0;
    mesh.degenerateDropped = 0;

    auto flush = [&]() {
        if (strip.size() >= 3) {
            ++mesh.stripCount;
            const size_t tris = strip.size() - 2;
            for (size_t i = 0; i < tris; ++i) {
                uint32_t a = strip[i];
                uint32_t b = strip[i + 1];
                uint32_t c = strip[i + 2];

                if (a == b || b == c || a == c) {
                    ++mesh.degenerateDropped;
                    continue;
                }

                if ((i % 2) == 0) {
                    mesh.triangles.push_back(a);
                    mesh.triangles.push_back(b);
                    mesh.triangles.push_back(c);
                } else {
                    mesh.triangles.push_back(a);
                    mesh.triangles.push_back(c);
                    mesh.triangles.push_back(b);
                }
            }
        }
        strip.clear();
    };

    for (uint16_t v : indices) {
        if (v == 0xFFFF) {
            flush();
        } else {
            strip.push_back(v);
        }
    }
    flush();
}

// ------------------------------------------------------------
//  Ein einzelnes Mesh lesen.
// ------------------------------------------------------------
bool ReadMesh(NudReader& br, NudMesh& mesh,
              size_t polyClumpStart, size_t vertClumpStart,
              size_t vertAddClumpStart, std::string& why) {
    const size_t polyStart    = polyClumpStart    + br.u32();
    const size_t vertStart    = vertClumpStart    + br.u32();
    const size_t vertAddStart = vertAddClumpStart + br.u32();

    const uint16_t vertexCount = br.u16();
    mesh.vertexFlags  = br.u8();
    mesh.uvColorFlags = br.u8();

    br.skip(16);                       // texProps[4] - Stufe 4

    const uint16_t faceCount = br.u16();
    br.u8();                           // faceSize, bei CC2 immer 0
    mesh.faceFlag = br.u8();
    br.skip(0xC);                      // Fuellbytes

    if (br.failed()) {
        why = "Mesh-Kopf abgeschnitten";
        return false;
    }

    // ---------------- Indizes ----------------
    const size_t back = br.pos();

    br.seek(polyStart);
    std::vector<uint16_t> indices;
    indices.reserve(faceCount);
    for (uint16_t i = 0; i < faceCount && !br.failed(); ++i) {
        indices.push_back(br.u16());
    }
    if (br.failed()) {
        why = "Indexdaten abgeschnitten";
        return false;
    }
    StripsToTriangles(indices, mesh);

    // ---------------- Vertexlayout ----------------
    const uint8_t vType = static_cast<uint8_t>(mesh.vertexFlags & 0x0F);
    const uint8_t bType = static_cast<uint8_t>(mesh.vertexFlags & 0xF0);
    const bool    half  = (vType & 0x04) != 0;

    const int  uvCount     = mesh.uvColorFlags >> 4;
    const uint8_t uvcType  = static_cast<uint8_t>(mesh.uvColorFlags & 0x0F);
    const bool floatUV     = (uvcType & 0x01) != 0;
    const bool byteColor   = (uvcType & 0x02) != 0;
    const bool halfColor   = !byteColor && ((uvcType & 0x04) != 0);

    mesh.uvCount = uvCount;
    mesh.uv.assign(static_cast<size_t>(uvCount), {});

    mesh.position.resize(vertexCount);
    if (( half && (vType & 0x02)) || (!half && (vType & 0x01))) {
        mesh.normal.resize(vertexCount);
    }
    if (byteColor || halfColor) mesh.color.resize(vertexCount);
    for (int u = 0; u < uvCount; ++u) mesh.uv[static_cast<size_t>(u)].resize(vertexCount);
    if (bType != 0) {
        mesh.boneIds.resize(vertexCount);
        mesh.boneWeights.resize(vertexCount);
    }

    // Zwei Leseschritte, weil Positionen und UVs bei geskinnten
    // Meshes in getrennten Bloecken liegen. Bei ungeskinnten
    // stehen beide Bloecke direkt hintereinander im selben
    // Datensatz - deshalb dieselbe Startadresse.
    const bool   skinned  = (bType != 0);
    const size_t geomBase = skinned ? vertAddStart : vertStart;
    const size_t uvBase   = vertStart;

    // --- Geometrie ---
    br.seek(geomBase);
    for (uint16_t i = 0; i < vertexCount && !br.failed(); ++i) {
        Vec3 p;
        p.x = br.f32(); p.y = br.f32(); p.z = br.f32();
        if (!half) br.f32();                      // Position hat ein w
        mesh.position[i] = p;

        if (half) {
            if (vType & 0x02) {
                Vec3 n;
                n.x = br.f16(); n.y = br.f16(); n.z = br.f16();
                br.f16();                          // Normale hat ein w
                mesh.normal[i] = n;
            }
            if (vType & 0x01) br.skip(16);         // bitangent + tangent, f16x4 je
        } else {
            if (vType & 0x01) {
                Vec3 n;
                n.x = br.f32(); n.y = br.f32(); n.z = br.f32();
                br.f32();
                mesh.normal[i] = n;
            }
            if (vType & 0x02) br.skip(32);         // bitangent + tangent, f32x4 je
        }

        if (bType & 0x10) {                        // u32-IDs, f32-Gewichte
            for (int k = 0; k < 4; ++k) mesh.boneIds[i][static_cast<size_t>(k)] = br.u32();
            for (int k = 0; k < 4; ++k) mesh.boneWeights[i][static_cast<size_t>(k)] = br.f32();
        } else if (bType & 0x20) {                 // u16-IDs, f16-Gewichte
            for (int k = 0; k < 4; ++k) mesh.boneIds[i][static_cast<size_t>(k)] = br.u16();
            for (int k = 0; k < 4; ++k) mesh.boneWeights[i][static_cast<size_t>(k)] = br.f16();
        } else if (bType & 0x40) {                 // u8-IDs, u8-Gewichte
            for (int k = 0; k < 4; ++k) mesh.boneIds[i][static_cast<size_t>(k)] = br.u8();
            for (int k = 0; k < 4; ++k) {
                mesh.boneWeights[i][static_cast<size_t>(k)] =
                    static_cast<float>(br.u8()) / 255.0f;
            }
        }

        // Bei ungeskinnten Meshes folgt der UV-/Farbblock direkt
        // im selben Datensatz - Farbe zuerst, dann die UVs.
        if (!skinned) {
            if (byteColor) {
                Color4 c;
                c.r = br.u8(); c.g = br.u8(); c.b = br.u8(); c.a = br.u8();
                mesh.color[i] = c;
            } else if (halfColor) {
                Color4 c;
                const float cr = br.f16(), cg = br.f16(), cb = br.f16(), ca = br.f16();
                c.r = static_cast<uint8_t>(cr * 255.0f);
                c.g = static_cast<uint8_t>(cg * 255.0f);
                c.b = static_cast<uint8_t>(cb * 255.0f);
                c.a = static_cast<uint8_t>(ca * 255.0f);
                mesh.color[i] = c;
            }
            for (int u = 0; u < uvCount; ++u) {
                Vec2 t;
                if (floatUV) { t.x = br.f32(); t.y = br.f32(); }
                else         { t.x = br.f16(); t.y = br.f16(); }
                mesh.uv[static_cast<size_t>(u)][i] = t;
            }
        }
    }

    // --- UV und Farbe, getrennter Block ---
    if (skinned) {
        br.seek(uvBase);
        for (uint16_t i = 0; i < vertexCount && !br.failed(); ++i) {
            if (byteColor) {
                Color4 c;
                c.r = br.u8(); c.g = br.u8(); c.b = br.u8(); c.a = br.u8();
                mesh.color[i] = c;
            } else if (halfColor) {
                Color4 c;
                const float cr = br.f16(), cg = br.f16(), cb = br.f16(), ca = br.f16();
                c.r = static_cast<uint8_t>(cr * 255.0f);
                c.g = static_cast<uint8_t>(cg * 255.0f);
                c.b = static_cast<uint8_t>(cb * 255.0f);
                c.a = static_cast<uint8_t>(ca * 255.0f);
                mesh.color[i] = c;
            }
            for (int u = 0; u < uvCount; ++u) {
                Vec2 t;
                if (floatUV) { t.x = br.f32(); t.y = br.f32(); }
                else         { t.x = br.f16(); t.y = br.f16(); }
                mesh.uv[static_cast<size_t>(u)][i] = t;
            }
        }
    }

    if (br.failed()) {
        why = "Vertexdaten abgeschnitten";
        return false;
    }

    br.seek(back);
    return true;
}

// ------------------------------------------------------------
//  Den NUD-Block eines Modells lesen.
// ------------------------------------------------------------
bool ReadNud(const uint8_t* data, size_t size, NudModel& model, std::string& why) {
    NudReader br(data, size);

    char magic[5] = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < 4; ++i) magic[i] = static_cast<char>(br.u8());
    if (std::strcmp(magic, "NDP3") != 0) {
        why = "NUD-Magic ist nicht 'NDP3'";
        return false;
    }

    br.u32();                                   // fileSize
    br.u16();                                   // NUD-Version
    const uint16_t groupCount = br.u16();

    model.boneStart = br.u16();
    model.boneEnd   = br.u16();

    // Alle Bloecke liegen hintereinander; ihre Startadressen
    // ergeben sich durch Aufaddieren der Groessen.
    const size_t polyClumpStart   = br.u32() + 0x30;
    const size_t polyClumpSize    = br.u32();
    const size_t vertClumpStart   = polyClumpStart + polyClumpSize;
    const size_t vertClumpSize    = br.u32();
    const size_t vertAddStart     = vertClumpStart + vertClumpSize;
    const size_t vertAddSize      = br.u32();
    const size_t nameStart        = vertAddStart + vertAddSize;

    for (int i = 0; i < 4; ++i) model.boundingSphere[i] = br.f32();

    if (br.failed()) {
        why = "NUD-Kopf abgeschnitten";
        return false;
    }

    model.groups.resize(groupCount);

    // Erst alle Gruppenkoepfe, dann die Meshes - so steht es auch
    // in der Datei.
    std::vector<uint16_t> meshCounts(groupCount, 0);

    for (uint16_t g = 0; g < groupCount && !br.failed(); ++g) {
        NudMeshGroup& grp = model.groups[g];

        br.skip(16);                            // boundingSphere
        br.skip(16);                            // unkValues
        const uint32_t nameOffset = br.u32();
        grp.name = br.cstrAt(nameStart + nameOffset);

        br.u16();                               // unk
        grp.boneFlags  = br.u16();
        grp.singleBind = br.i16();
        meshCounts[g]  = br.u16();
        br.u32();                               // Startposition der Mesh-Koepfe
    }

    if (br.failed()) {
        why = "Gruppenkoepfe abgeschnitten";
        return false;
    }

    for (uint16_t g = 0; g < groupCount; ++g) {
        NudMeshGroup& grp = model.groups[g];
        grp.meshes.resize(meshCounts[g]);

        for (uint16_t m = 0; m < meshCounts[g]; ++m) {
            std::string sub;
            if (!ReadMesh(br, grp.meshes[m], polyClumpStart, vertClumpStart,
                          vertAddStart, sub)) {
                why = "Gruppe '" + Escape(grp.name) + "', Mesh " +
                      std::to_string(m) + ": " + sub;
                return false;
            }
        }
    }

    return true;
}

} // namespace

// ============================================================
//  nuccChunkModel
// ============================================================

bool ParseModels(const XfbinFile& file, std::vector<NudModel>& out,
                 std::string& error, std::string& warnings) {
    out.clear();
    error.clear();
    warnings.clear();

    std::ostringstream warn;
    bool allOk = true;

    for (size_t pi = 0; pi < file.pages.size(); ++pi) {
        const XfbinPage& page = file.pages[pi];
        for (const XfbinChunk& chunk : page.chunks) {
            if (!chunk.type || *chunk.type != "nuccChunkModel") continue;

            NudModel model;
            model.name          = chunk.name ? *chunk.name : RawString();
            model.version       = chunk.version;
            model.pageIndex     = pi;
            model.localMapIndex = chunk.localMapIndex;

            NudReader br(chunk.data.data(), chunk.data.size());

            uint32_t nudSize = 0;

            // Zwei Feldanordnungen, abhaengig von der Version.
            // Genau so steht es in br_nucc.py - nicht vereinfachen,
            // sonst brechen Storm-1-Dateien oder JoJo-Dateien.
            if (chunk.version > 0x73 && chunk.version < 0x76) {
                br.u16();                        // field00
                model.riggingFlag = br.u16();
                model.attributes  = br.u16();
                br.u16();
                model.clumpIndex    = br.u32();
                model.hitIndex      = br.u32();
                model.meshBoneIndex = br.u32();
                nudSize             = br.u32();
                br.u16();                        // lightCategoryFlag
                model.renderLayer = br.u8();
                model.lightModeId = br.u8();
            } else {
                br.u16();                        // field00
                model.riggingFlag = br.u16();
                model.attributes  = br.u16();
                model.renderLayer = br.u8();
                model.lightModeId = br.u8();
                if (chunk.version > 0x73) br.u32();   // lightCategoryFlag u32
                model.clumpIndex    = br.u32();
                model.hitIndex      = br.u32();
                model.meshBoneIndex = br.u32();
                nudSize             = br.u32();
            }

            if (model.attributes & 0x04) {
                model.hasBoundingBox = true;
                for (int i = 0; i < 6; ++i) model.boundingBox[i] = br.f32();
            }

            if (br.failed() || nudSize == 0) {
                warn << "Modell '" << Escape(model.name)
                     << "' hat keinen NUD-Block und wird uebersprungen.\n";
                continue;
            }

            const size_t nudPos = br.pos();
            if (nudPos + nudSize > chunk.data.size()) {
                warn << "Modell '" << Escape(model.name)
                     << "': NUD-Block ragt ueber das Chunk-Ende hinaus.\n";
                allOk = false;
                continue;
            }

            std::string why;
            if (!ReadNud(chunk.data.data() + nudPos, nudSize, model, why)) {
                warn << "Modell '" << Escape(model.name) << "': " << why << ".\n";
                allOk = false;
                continue;
            }

            // Materialindizes stehen hinter dem NUD-Block.
            br.seek(nudPos + nudSize);
            const uint16_t matCount = br.u16();
            for (uint16_t i = 0; i < matCount && !br.failed(); ++i) {
                model.materialIndices.push_back(br.u32());
            }

            out.push_back(std::move(model));
        }
    }

    warnings = warn.str();

    if (out.empty()) {
        error = "Kein auswertbares nuccChunkModel in der Datei.";
        return false;
    }

    return allOk;
}

// ============================================================
//  Dump
// ============================================================

void WriteMeshDump(const std::vector<NudModel>& models, const std::string& label,
                   std::ostream& o, bool includeVertices) {
    o.imbue(std::locale::classic());

    o << "# XFBIN mesh dump v1\n";
    o << "file " << label << "\n";
    o << "models " << models.size() << "\n";

    for (size_t mi = 0; mi < models.size(); ++mi) {
        const NudModel& mo = models[mi];

        o << "model[" << mi << "] name=" << Escape(mo.name)
          << " groups="  << mo.groups.size()
          << " verts="   << mo.VertexCount()
          << " tris="    << mo.TriangleCount()
          << " rigging=" << mo.riggingFlag
          << " attrs="   << mo.attributes
          << " clumpIdx="  << mo.clumpIndex
          << " meshBone="  << mo.meshBoneIndex
          << " boneRange=" << mo.boneStart << ".." << mo.boneEnd
          << " materials=" << mo.materialIndices.size()
          << "\n";

        for (size_t gi = 0; gi < mo.groups.size(); ++gi) {
            const NudMeshGroup& g = mo.groups[gi];

            o << "group[" << mi << "." << gi << "] name=" << Escape(g.name)
              << " meshes="     << g.meshes.size()
              << " boneFlags="  << g.boneFlags
              << " singleBind=" << g.singleBind << "\n";

            for (size_t si = 0; si < g.meshes.size(); ++si) {
                const NudMesh& m = g.meshes[si];

                o << "mesh[" << mi << "." << gi << "." << si << "]"
                  << " verts="  << m.VertexCount()
                  << " tris="   << m.TriangleCount()
                  << " vFlags=" << static_cast<int>(m.vertexFlags)
                  << " uvFlags=" << static_cast<int>(m.uvColorFlags)
                  << " uvCh="   << m.uvCount
                  << " normal=" << (m.HasNormal() ? 1 : 0)
                  << " color="  << (m.HasColor()  ? 1 : 0)
                  << " bones="  << (m.HasBones()  ? 1 : 0)
                  << " strips=" << m.stripCount
                  << " degen="  << m.degenerateDropped
                  << "\n";

                if (!includeVertices) continue;

                for (size_t v = 0; v < m.position.size(); ++v) {
                    o << "  v" << v
                      << " p " << Num(m.position[v].x) << " "
                               << Num(m.position[v].y) << " "
                               << Num(m.position[v].z);
                    if (m.HasNormal()) {
                        o << " n " << Num(m.normal[v].x) << " "
                                   << Num(m.normal[v].y) << " "
                                   << Num(m.normal[v].z);
                    }
                    if (m.HasColor()) {
                        o << " c " << static_cast<int>(m.color[v].r) << " "
                                   << static_cast<int>(m.color[v].g) << " "
                                   << static_cast<int>(m.color[v].b) << " "
                                   << static_cast<int>(m.color[v].a);
                    }
                    for (int u = 0; u < m.uvCount; ++u) {
                        o << " t" << u << " "
                          << Num(m.uv[static_cast<size_t>(u)][v].x) << " "
                          << Num(m.uv[static_cast<size_t>(u)][v].y);
                    }
                    if (m.HasBones()) {
                        o << " b";
                        for (int k = 0; k < 4; ++k) o << " " << m.boneIds[v][static_cast<size_t>(k)];
                        o << " w";
                        for (int k = 0; k < 4; ++k) o << " " << Num(m.boneWeights[v][static_cast<size_t>(k)]);
                    }
                    o << "\n";
                }

                for (size_t t = 0; t + 2 < m.triangles.size(); t += 3) {
                    o << "  f " << m.triangles[t] << " "
                                << m.triangles[t + 1] << " "
                                << m.triangles[t + 2] << "\n";
                }
            }
        }
    }

    o << "end\n";
}

std::string MakeMeshSummary(const std::vector<NudModel>& models) {
    std::ostringstream s;
    s.imbue(std::locale::classic());

    size_t verts = 0, tris = 0, meshes = 0, skinned = 0;
    for (const NudModel& m : models) {
        verts += m.VertexCount();
        tris  += m.TriangleCount();
        for (const NudMeshGroup& g : m.groups) {
            meshes += g.meshes.size();
            for (const NudMesh& sm : g.meshes) if (sm.HasBones()) ++skinned;
        }
    }

    s << "Modelle: " << models.size()
      << " | Submeshes: " << meshes
      << " | Vertices: " << verts
      << " | Dreiecke: " << tris
      << " | geskinnt: " << skinned << "\n";

    for (const NudModel& m : models) {
        s << "  " << m.name << ": " << m.VertexCount() << " V, "
          << m.TriangleCount() << " T\n";
    }

    return s.str();
}

} // namespace xfbin

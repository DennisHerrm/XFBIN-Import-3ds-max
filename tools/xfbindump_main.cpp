// ============================================================
//  xfbindump - eigenstaendiges Kommandozeilenwerkzeug
//
//  Baut OHNE 3ds Max SDK. Zweck: den Parser gegen die Python-Lib
//  gegenpruefen, bevor irgendetwas in eine Max-Szene geht.
//
//    xfbindump datei.xfbin                 -> Kurzfassung
//    xfbindump datei.xfbin -o dump.txt     -> voller Dump in Datei
//    xfbindump datei.xfbin --no-tables     -> Dump ohne String-/
//                                             Index-Tabellen
// ============================================================

#include "../src/xfbin_reader.h"
#include "../src/xfbin_clump.h"
#include "../src/xfbin_nud.h"
#include "../src/xfbin_anm.h"
#include "../src/xfbin_tex.h"
#include "../src/xfbin_cpk.h"
#include "../src/xfbin_spiel.h"
#include "../src/xfbin_particle.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string BaseName(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

void PrintUsage() {
    std::cout <<
        "xfbindump - XFBIN-Container inspizieren\n"
        "\n"
        "  xfbindump <datei.xfbin> [-o <dump.txt>] [--no-tables]\n"
        "                          [--bones] [--bones-o <bones.txt>]\n"
        "\n"
        "  -o <datei>        vollen Container-Dump in diese Datei\n"
        "  --no-tables       String- und Indextabellen im Dump weglassen\n"
        "  --bones           Skelett auswerten und zusammenfassen\n"
        "  --bones-o <datei> Skelett-Dump in diese Datei (impliziert --bones)\n"
        "  --meshes          Meshes auswerten und zusammenfassen\n"
        "  --meshes-o <datei> Mesh-Dump in diese Datei (impliziert --meshes)\n"
        "  --no-verts        im Mesh-Dump nur Kopfzeilen, keine Vertexdaten\n"
        "  --anims           Animationen auswerten und zusammenfassen\n"
        "  --anims-o <datei> Anim-Dump in diese Datei (impliziert --anims)\n"
        "  --no-keys         im Anim-Dump nur Kopfzeilen, keine Keyframes\n"
        "  --tex             Texturen und Materialien zusammenfassen\n"
        "  --tex-o <ordner>  Texturen als DDS in diesen Ordner (impliziert --tex)\n";
}

// ------------------------------------------------------------
//  Spieldaten - direkt aus den CPK-Archiven eines Spiels
//
//    xfbindump --spiele
//    xfbindump --spiel <ordner> katalog [suchwort]
//    xfbindump --spiel <ordner> test [pfadanfang]
//    xfbindump --spiel <ordner> extract <pfad> <zieldatei>
//    xfbindump --spiel <ordner> figur <code> [kostuem]
// ------------------------------------------------------------
std::wstring Breit(const std::string& s) {
    std::wstring w;
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    w.resize(static_cast<size_t>(n > 0 ? n : 0));
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string Schmal(const std::wstring& w) {
    std::string s;
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    s.resize(static_cast<size_t>(n > 0 ? n : 0));
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

int SpielBefehl(int argc, char** argv) {
    if (std::string(argv[1]) == "--spiele") {
        for (const xfbin::SpielOrt& s : xfbin::FindeSpiele())
            std::cout << Schmal(s.name) << "\t" << Schmal(s.ordner) << "\n";
        return 0;
    }
    if (argc < 4) { PrintUsage(); return 2; }
    const std::string befehl = argv[3];

    const auto t0 = std::chrono::steady_clock::now();
    xfbin::Spieldaten daten;
    std::string fehler;
    if (!daten.Oeffne(Breit(argv[2]), fehler)) { std::cerr << "FEHLER: " << fehler << "\n"; return 1; }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << daten.ArchivZahl() << " Archive, " << daten.DateiZahl() << " Dateien, " << static_cast<int>(ms) << " ms\n";

    if (befehl == "katalog") {
        const std::string such = argc > 4 ? xfbin::Klein(argv[4]) : std::string();
        const xfbin::Katalog k = xfbin::BaueKatalog(daten);
        size_t n = 0;
        for (const xfbin::FigurEintrag& f : k.figuren) {
            if (!such.empty() && xfbin::Klein(f.code + " " + f.name).find(such) == std::string::npos) continue;
            ++n;
            std::cout << f.code << " bod" << f.kostuem << "  " << f.name << (f.zusatz.empty() ? "" : " (" + f.zusatz + ")")
                      << "  art=" << f.art << "  anims=" << f.anims.size() << "  gemeinsam=" << f.gemeinsam << "\n";
            if (!such.empty())
                for (const std::string& a : f.anims) std::cout << "    " << xfbin::AnimGruppe(f, a) << "\t" << a << "\n";
        }
        std::cout << n << " von " << k.figuren.size() << " Figuren\n";
        return 0;
    }

    if (befehl == "extract" && argc >= 6) {
        std::vector<uint8_t> b;
        if (!daten.Lies(argv[4], b, fehler)) { std::cerr << "FEHLER: " << fehler << "\n"; return 1; }
        std::ofstream o(argv[5], std::ios::binary);
        o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
        std::cout << b.size() << " Bytes -> " << argv[5] << "\n";
        return o ? 0 : 1;
    }

    if (befehl == "test") {
        // Jede Datei auspacken und als XFBIN lesen. Zaehlt, was
        // scheitert, und nennt die ersten Faelle.
        const std::string anfang = argc > 4 ? xfbin::Klein(argv[4]) : std::string("data/");
        size_t gut = 0, schlecht = 0, bytes = 0;
        for (const std::string& p : daten.Pfade()) {
            if (p.compare(0, anfang.size(), anfang) != 0) continue;
            std::vector<uint8_t> b;
            std::string f;
            bool ok = daten.Lies(p, b, f);
            if (ok) {
                xfbin::XfbinFile x;
                const xfbin::ReadResult r = xfbin::ReadXfbinBuffer(b.data(), b.size(), x);
                ok = r.ok;
                if (!ok) f = p + ": " + r.error;
            }
            if (ok) { ++gut; bytes += b.size(); }
            else if (++schlecht <= 20) std::cout << "  FEHLER " << f << "\n";
        }
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << gut << " gelesen (" << bytes / (1024 * 1024) << " MB), " << schlecht << " gescheitert, "
                  << static_cast<int>(s) << " s\n";
        return schlecht == 0 ? 0 : 1;
    }

    if (befehl == "partikel" && argc >= 5) {
        // Partikel aller Dateien unter einem Pfadanfang lesen:
        //   partikel <pfadanfang> [einzeln]
        const std::string anfang = xfbin::Klein(argv[4]);
        const bool einzeln = argc > 5;
        size_t dateien = 0, systeme = 0, emitter = 0, warn = 0;
        std::map<uint32_t, size_t> arten;
        for (const std::string& p : daten.Pfade()) {
            if (p.compare(0, anfang.size(), anfang) != 0) continue;
            std::vector<uint8_t> b;
            xfbin::XfbinFile x;
            if (!daten.Lies(p, b, fehler) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), x).ok) continue;
            if (x.CountOfType("nuccChunkParticle") == 0) continue;
            ++dateien;
            std::vector<xfbin::ParticleSystem> ps;
            std::string w;
            xfbin::ParseParticles(x, ps, w);
            if (!w.empty()) { ++warn; std::cout << w; }
            for (const xfbin::ParticleSystem& s : ps) {
                ++systeme;
                for (const xfbin::ParticleEmitter& e : s.emitters) {
                    ++emitter;
                    ++arten[e.materialKind];
                    if (!einzeln) continue;
                    std::cout << s.name << " #" << e.entry << " art=" << e.materialKind << " " << e.materialName << " @" << e.attachBone
                              << " (" << e.attachOwner << ") n=" << e.count << " r=" << e.radiusStart << " life=" << e.life
                              << " blow=" << e.blowout << " loop=" << int(e.stretchLoop) << " zeit=";
                    for (const auto& z : e.timeline) std::cout << (z.first ? "+" : "-") << z.second << " ";
                    std::cout << '\n';
                }
            }
        }
        std::cout << dateien << " Dateien, " << systeme << " Partikelsysteme, " << emitter << " Emitter; Arten:";
        for (const auto& kv : arten) std::cout << " " << kv.first << "=" << kv.second;
        std::cout << "; " << warn << " Dateien mit Hinweisen" << '\n';
        return 0;
    }

    if (befehl == "chunks" && argc >= 7) {
        // Rohdaten aller Chunks eines Typs in einen Ordner, dazu je
        // Chunk die Verweistabelle seiner Page (lokaler Index ->
        // Typ:Name), damit Chunk-Verweise im Inhalt lesbar werden.
        //   chunks <pfad> <typ> <zielordner>
        std::vector<uint8_t> b;
        xfbin::XfbinFile x;
        if (!daten.Lies(argv[4], b, fehler) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), x).ok) {
            std::cerr << "FEHLER: " << fehler << "\n";
            return 1;
        }
        const std::string typ = argv[5], ziel = argv[6];
        size_t n = 0;
        for (size_t pi = 0; pi < x.pages.size(); ++pi) {
            const xfbin::XfbinPage& pg = x.pages[pi];
            for (const xfbin::XfbinChunk& c : pg.chunks) {
                if (!c.type || *c.type != typ) continue;
                const std::string basis = ziel + "/" + std::to_string(n++) + "_v" + std::to_string(c.version) + "_" + (c.name ? *c.name : std::string("?"));
                std::ofstream o(basis + ".bin", std::ios::binary);
                o.write(reinterpret_cast<const char*>(c.data.data()), static_cast<std::streamsize>(c.data.size()));
                std::ofstream t(basis + ".map.txt");
                for (size_t li = 0; li < pg.pageMapIndices.size(); ++li) {
                    const uint32_t gi = pg.pageMapIndices[li];
                    if (gi >= x.table.maps.size()) continue;
                    const xfbin::ChunkMap& m = x.table.maps[gi];
                    t << li << "\t" << (m.typeIndex < x.table.types.size() ? x.table.types[m.typeIndex] : "?") << "\t"
                      << (m.nameIndex < x.table.names.size() ? x.table.names[m.nameIndex] : "?") << "\n";
                }
            }
        }
        std::cout << n << " Chunks vom Typ " << typ << " -> " << ziel << "\n";
        return 0;
    }

    if (befehl == "clumps" && argc >= 5) {
        // Alle Dateien unter einem Pfadanfang nach ihren Clumps
        // fragen: "welche Datei enthaelt 1efc_ring12?"
        const std::string anfang = xfbin::Klein(argv[4]);
        const std::string such = argc > 5 ? xfbin::Klein(argv[5]) : std::string();
        for (const std::string& p : daten.Pfade()) {
            if (p.compare(0, anfang.size(), anfang) != 0) continue;
            std::vector<uint8_t> b;
            std::string fe;
            xfbin::XfbinFile x;
            if (!daten.Lies(p, b, fe) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), x).ok) continue;
            std::string namen;
            size_t n = 0, anm = x.CountOfType("nuccChunkAnm"), par = x.CountOfType("nuccChunkParticle");
            for (const xfbin::XfbinPage& pg : x.pages)
                for (const xfbin::XfbinChunk& c : pg.chunks)
                    if (c.type && *c.type == "nuccChunkClump" && c.name) { ++n; namen += " " + *c.name; }
            if (!such.empty() && xfbin::Klein(namen).find(such) == std::string::npos) continue;
            std::cout << p << "  clumps=" << n << " anm=" << anm << " particle=" << par << " :" << namen.substr(0, 400) << "\n";
        }
        return 0;
    }

    if (befehl == "figur" && argc >= 5) {
        // Alle Animationen einer Figur laden und zeigen, welche ihr
        // Skelett bewegen.
        const int kostuem = argc > 5 ? std::atoi(argv[5]) : 1;
        const xfbin::Katalog k = xfbin::BaueKatalog(daten);
        for (const xfbin::FigurEintrag& f : k.figuren) {
            if (f.code != argv[4] || f.kostuem != kostuem) continue;
            const std::string clump = xfbin::ClumpName(f);
            std::cout << f.name << " - Modell " << f.modell << ", Clump " << clump << "\n";
            std::vector<std::string> dateien = f.anims;
            if (!f.gemeinsam.empty()) dateien.push_back(f.gemeinsam);
            for (const std::string& a : dateien) {
                std::vector<uint8_t> b;
                std::string fe;
                xfbin::XfbinFile x;
                if (!daten.Lies(a, b, fe) || !xfbin::ReadXfbinBuffer(b.data(), b.size(), x).ok) {
                    std::cout << "  " << a << ": nicht lesbar " << fe << "\n";
                    continue;
                }
                std::vector<xfbin::Anm> anims;
                std::string ae, aw;
                xfbin::ParseAnims(x, anims, ae, aw);
                size_t eigen = 0;
                std::string liste;
                for (const xfbin::Anm& an : anims) {
                    bool trifft = false;
                    for (const xfbin::AnmClumpRef& c : an.clumps)
                        if (c.clumpName == clump || c.clumpName.find("cmnbod") != std::string::npos) trifft = true;
                    if (trifft) { ++eigen; if (liste.size() < 300) liste += " " + an.name; }
                }
                std::cout << "  " << xfbin::AnimGruppe(f, a) << "\t" << a << ": " << anims.size() << " Anims, " << eigen
                          << " fuer die Figur:" << liste << "\n";
                // Welche Clumps sprechen die Animationen an, und wo
                // liegt ein Modell dazu?
                std::vector<std::string> clumps;
                for (const xfbin::Anm& an : anims)
                    for (const xfbin::AnmClumpRef& c : an.clumps)
                        if (std::find(clumps.begin(), clumps.end(), c.clumpName) == clumps.end()) clumps.push_back(c.clumpName);
                for (const std::string& c : clumps) {
                    std::string wo;
                    for (const std::string& p : daten.Pfade())
                        if (xfbin::DateiName(p) == xfbin::Klein(c) + ".xfbin") wo += " " + p;
                    std::cout << "      clump " << c << (wo.empty() ? "  (kein Modell)" : "  ->" + wo) << "\n";
                }
            }
            return 0;
        }
        std::cerr << "Figur nicht gefunden\n";
        return 1;
    }

    PrintUsage();
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && (std::string(argv[1]) == "--spiel" || std::string(argv[1]) == "--spiele")) return SpielBefehl(argc, argv);

    std::string input;
    std::string output;
    std::string bonesOut;
    std::string meshesOut;
    bool includeTables = true;
    bool includeVerts  = true;
    std::string animsOut;
    bool includeKeys = true;
    bool doBones  = false;
    bool doMeshes = false;
    std::string texOut;
    bool doAnims  = false;
    bool doTex    = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (a == "--no-tables") {
            includeTables = false;
        } else if (a == "--bones") {
            doBones = true;
        } else if (a == "--bones-o" && i + 1 < argc) {
            bonesOut = argv[++i];
            doBones = true;
        } else if (a == "--meshes") {
            doMeshes = true;
        } else if (a == "--meshes-o" && i + 1 < argc) {
            meshesOut = argv[++i];
            doMeshes = true;
        } else if (a == "--no-verts") {
            includeVerts = false;
        } else if (a == "--anims") {
            doAnims = true;
        } else if (a == "--anims-o" && i + 1 < argc) {
            animsOut = argv[++i];
            doAnims = true;
        } else if (a == "--no-keys") {
            includeKeys = false;
        } else if (a == "--tex") {
            doTex = true;
        } else if (a == "--tex-o" && i + 1 < argc) {
            texOut = argv[++i];
            doTex = true;
        } else if (a == "-h" || a == "--help") {
            PrintUsage();
            return 0;
        } else if (input.empty()) {
            input = a;
        } else {
            std::cerr << "Unbekanntes Argument: " << a << "\n";
            return 2;
        }
    }

    if (input.empty()) {
        PrintUsage();
        return 2;
    }

    xfbin::XfbinFile file;
    const xfbin::ReadResult r = xfbin::ReadXfbinFile(input, file);

    if (!r.ok) {
        std::cerr << "FEHLER: " << r.error << "\n";
        return 1;
    }
    if (!r.warnings.empty()) {
        std::cerr << "WARNUNG:\n" << r.warnings;
    }

    if (output.empty()) {
        std::cout << xfbin::MakeSummary(file);
    } else {
        std::ofstream f(output, std::ios::binary);
        if (!f) {
            std::cerr << "FEHLER: Ausgabedatei nicht schreibbar: "
                      << output << "\n";
            return 1;
        }
        xfbin::WriteDump(file, BaseName(input), f, includeTables);
        std::cout << "Dump geschrieben: " << output << "\n";
        std::cout << xfbin::MakeSummary(file);
    }

    // ------------------------------------------------------------
    //  Stufe 1: Skelett
    // ------------------------------------------------------------
    if (doBones) {
        std::vector<xfbin::Clump> clumps;
        std::string boneErr, boneWarn;

        const bool ok = xfbin::ParseClumps(file, clumps, boneErr, boneWarn);

        if (!boneWarn.empty()) {
            std::cerr << "WARNUNG (Skelett):\n" << boneWarn;
        }

        if (clumps.empty()) {
            std::cerr << "FEHLER: " << boneErr << "\n";
            return 1;
        }

        std::cout << "\n" << xfbin::MakeBoneSummary(clumps);

        if (!bonesOut.empty()) {
            std::ofstream bf(bonesOut, std::ios::binary);
            if (!bf) {
                std::cerr << "FEHLER: Skelett-Dump nicht schreibbar: "
                          << bonesOut << "\n";
                return 1;
            }
            xfbin::WriteBoneDump(clumps, BaseName(input), bf);
            std::cout << "Skelett-Dump geschrieben: " << bonesOut << "\n";
        }

        if (!ok) return 1;
    }

    // ------------------------------------------------------------
    //  Stufe 2: Meshes
    // ------------------------------------------------------------
    if (doMeshes) {
        std::vector<xfbin::NudModel> models;
        std::string meshErr, meshWarn;

        const bool ok = xfbin::ParseModels(file, models, meshErr, meshWarn);

        if (!meshWarn.empty()) {
            std::cerr << "WARNUNG (Meshes):\n" << meshWarn;
        }

        if (models.empty()) {
            std::cerr << "FEHLER: " << meshErr << "\n";
            return 1;
        }

        std::cout << "\n" << xfbin::MakeMeshSummary(models);

        if (!meshesOut.empty()) {
            std::ofstream mf(meshesOut, std::ios::binary);
            if (!mf) {
                std::cerr << "FEHLER: Mesh-Dump nicht schreibbar: "
                          << meshesOut << "\n";
                return 1;
            }
            xfbin::WriteMeshDump(models, BaseName(input), mf, includeVerts);
            std::cout << "Mesh-Dump geschrieben: " << meshesOut << "\n";
        }

        if (!ok) return 1;
    }

    // ------------------------------------------------------------
    //  Stufe 5: Animationen
    // ------------------------------------------------------------
    if (doAnims) {
        std::vector<xfbin::Anm> anims;
        std::string animErr, animWarn;

        const bool ok = xfbin::ParseAnims(file, anims, animErr, animWarn);

        if (!animWarn.empty()) {
            std::cerr << "WARNUNG (Animationen):\n" << animWarn;
        }
        if (anims.empty()) {
            std::cerr << "FEHLER: " << animErr << "\n";
            return 1;
        }

        std::cout << "\n" << xfbin::MakeAnimSummary(anims);

        if (!animsOut.empty()) {
            std::ofstream af(animsOut, std::ios::binary);
            if (!af) {
                std::cerr << "FEHLER: Anim-Dump nicht schreibbar: "
                          << animsOut << "\n";
                return 1;
            }
            xfbin::WriteAnimDump(anims, BaseName(input), af, includeKeys);
            std::cout << "Anim-Dump geschrieben: " << animsOut << "\n";
        }

        if (!ok) return 1;
    }

    // ------------------------------------------------------------
    //  Stufe 4: Texturen und Materialien
    // ------------------------------------------------------------
    if (doTex) {
        std::vector<xfbin::XfbinTexture>  textures;
        std::vector<xfbin::XfbinMaterial> materials;
        std::string texErr, texWarn, matErr, matWarn;

        xfbin::ParseTextures(file, textures, texErr, texWarn);
        xfbin::ParseMaterials(file, materials, matErr, matWarn);

        if (!texWarn.empty()) std::cerr << "WARNUNG (Texturen):\n" << texWarn;
        if (!matWarn.empty()) std::cerr << "WARNUNG (Materialien):\n" << matWarn;

        if (textures.empty() && materials.empty()) {
            std::cerr << "FEHLER: " << texErr << "\n";
            return 1;
        }

        std::cout << "\n" << xfbin::MakeTextureSummary(textures, materials);

        if (!texOut.empty()) {
            std::vector<std::string> names;
            std::string warn2;
            const int n = xfbin::ExportTextures(textures, texOut, names, warn2);
            if (!warn2.empty()) std::cerr << warn2;
            std::cout << n << " DDS-Datei(en) geschrieben nach " << texOut << "\n";
        }
    }

    return 0;
}

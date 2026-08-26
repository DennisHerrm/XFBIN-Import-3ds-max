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

} // namespace

int main(int argc, char** argv) {
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

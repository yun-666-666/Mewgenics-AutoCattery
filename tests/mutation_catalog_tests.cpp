#include "auto_cattery/snapshot/detail/visual_traits.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "test_support.hpp"

namespace autocattery::tests {
namespace {

template<class T>
void Write(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::filesystem::path WriteCatalogGpak() {
    const auto path = std::filesystem::temp_directory_path() /
        "auto_cattery_mutation_catalog_test.gpak";
    const std::string name = "data/mutations/body.gon";
    const std::string content =
        "2 { // base\n}\n"
        "300 { // normal\n}\n"
        "700 { // defect\n tag birth_defect\n}\n"
        "-2 { // missing\n tag birth_defect\n}\n";
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    Write(stream, std::uint32_t{1});
    Write(stream, static_cast<std::uint16_t>(name.size()));
    stream.write(name.data(), static_cast<std::streamsize>(name.size()));
    Write(stream, static_cast<std::uint32_t>(content.size()));
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return path;
}

}  // namespace

void RunMutationCatalogTests() {
    const auto path = WriteCatalogGpak();
    snapshot::detail::MutationCatalog catalog;
    std::string error;
    AC_CHECK(snapshot::detail::LoadMutationCatalog(path, catalog, error));
    AC_CHECK(catalog.size() == 3);

    snapshot::CatSnapshot cat;
    cat.raw_visual_part_slots = {
        {"body", "body", 300},
        {"head", "body", 2},
        {"tail", "body", 700},
        {"leg_L", "body", 0xFFFFFFFEU}
    };
    snapshot::detail::ApplyMutationCatalog(cat, catalog);
    AC_CHECK(cat.visual_traits.size() == 3);
    AC_CHECK(
        cat.visual_traits[0].kind == snapshot::VisualTraitKind::Mutation);
    AC_CHECK(
        cat.visual_traits[1].kind == snapshot::VisualTraitKind::BirthDefect);
    AC_CHECK(
        cat.visual_traits[2].kind == snapshot::VisualTraitKind::BirthDefect);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

}  // namespace autocattery::tests

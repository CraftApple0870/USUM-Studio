#pragma once
#include "formats/archive.h"
#include "core/game_profile.h"
#include "core/binary.h"
#include <map>
#include <filesystem>
#include <stdexcept>
#include <cstdint>

namespace studio {
// Creates a new Personal entry for an additional Pokemon form, preserving all
// prior form entries. Never edits the source archive directly.
inline void create_pokemon_personal_form(const std::filesystem::path &input,
                                         const std::filesystem::path &output,
                                         unsigned species, unsigned new_form) {
    Archive archive(input);
    const auto total = archive.size();
    const unsigned base_limit = total >= 1026 ? 1026u : 808u;
    if (!species || species >= base_limit)
        throw std::runtime_error("Target species has no supported Personal Data base row");
    if (total > 65535)
        throw std::runtime_error("Personal archive index exceeds 16-bit range");
    auto base = archive.decoded(species);
    if (base.size() != 0x54)
        throw std::runtime_error("Target Personal Data entry is not 84 bytes");
    const unsigned count = base[0x20];
    if (!count || new_form == 0 || new_form >= 255)
        throw std::runtime_error("Invalid new form number (expected 1..254)");
    if (new_form < count)
        throw std::runtime_error("This Personal form already exists; refusing to overwrite it. "
                                 "Use an existing-form association workflow instead.");
    const unsigned pointer = unsigned(base[0x1c]) | (unsigned(base[0x1d]) << 8);
    unsigned start = pointer;
    // pk3DS Expansion compatibility: old form block originally starting at 808..1025.
    if (base_limit > 808 && species <= 807 && pointer >= 808 && pointer < 1026)
        start += base_limit - 808;
    if (count > 1 && (!pointer || start < base_limit || start + count - 1 > total))
        throw std::runtime_error("Existing Personal form pointers are inconsistent");
    if (total + new_form > 65535)
        throw std::runtime_error("Personal Data expansion exceeds 16-bit range");
    std::map<std::pair<std::size_t,unsigned>,Bytes> changes;
    // The new contiguous form block is appended to avoid shifting existing rows.
    for (unsigned form = 1; form < count; ++form) {
        const auto index = start + form - 1;
        auto copy = archive.decoded(index);
        if (copy.size() != 0x54 || copy != archive.raw(index))
            throw std::runtime_error("Compressed or invalid existing Personal form cannot be cloned safely");
        changes[{total + form - 1, 0}] = copy;
    }
    // Fill missing Personal forms (if graphics had more forms already), then
    // append the newly requested form. All entries are 84-byte templates.
    // Existing form data are copied unchanged above.
    for (unsigned form = count; form <= new_form; ++form)
        changes[{total + form - 1, 0}] = base;
    base[0x1c] = std::uint8_t(total & 255u);
    base[0x1d] = std::uint8_t((total >> 8) & 255u);
    base[0x20] = std::uint8_t(new_form + 1);
    if (archive.raw(species) != archive.decoded(species))
        throw std::runtime_error("Compressed Personal species base row needs special handling");
    changes[{species,0}] = base;
    archive.export_appended(output, changes);
    Archive check(output);
    if (check.size() != total + new_form || check.decoded(species) != base ||
        check.decoded(total + new_form - 1).size() != 0x54)
        throw std::runtime_error("Personal Data export verification failed");
}
}

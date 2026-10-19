#pragma once
// USUM Studio localisation foundation.
// Add interface strings to french_strings; untranslated strings remain in English.
// The active language is shared by all translation units (C++17 inline variable).
#include <string>
#include <unordered_map>

namespace studio::localization {
enum class Language { English, French };
inline Language current = Language::English;

inline void set_language(const std::string &code) {
    current = code == "fr" ? Language::French : Language::English;
}
inline const char *language_code() {
    return current == Language::French ? "fr" : "en";
}
inline const char *tr(const char *english) {
    if (current != Language::French)
        return english;
    static const std::unordered_map<std::string, const char *> french_strings = {
        {"Maps", "Cartes"},
        {"Models", "Modèles"},
        {"Studio", "Studio"},
        {"Authoring", "Création"},
        {"Collision", "Collisions"},
        {"Cameras", "Caméras"},
        {"Images", "Images"},
        {"Audio", "Audio"},
        {"Warp Ride", "Ultra-Brèche"},
        {"View", "Affichage"},
        {"Project", "Projet"},
        {"Language", "Langue"},
        {"English", "Anglais"},
        {"French", "Français"},
        {"Model browser", "Navigateur de modèles"},
        {"Pokemon", "Pokémon"},
        {"Battle characters", "Personnages de combat"},
        {"Field characters & props", "Personnages et objets du terrain"},
        {"Poke Balls", "Poké Balls"},
        {"Battle props", "Objets de combat"},
        {"Poke Beans", "Poké Fèves"},
        {"Battle arena parts", "Éléments d'arènes de combat"},
        {"Clothing", "Vêtements"},
        {"Battle effects", "Effets de combat"},
        {"Library options", "Options de la bibliothèque"},
        {"Choose source archive", "Choisir une archive source"},
        {"Refresh catalog", "Actualiser le catalogue"},
        {"External GARC...", "GARC externe..."},
        {"Use dump archive", "Utiliser l'archive extraite"},
        {"Cancel", "Annuler"},
        {"Shiny textures", "Textures chromatiques"},
        {"Search resource name or member", "Rechercher un nom ou un membre"},
        {"Search name, species number or form", "Rechercher un nom, numéro ou forme"},
        {"No matching models.", "Aucun modèle correspondant."},
        {"No models found for this category. Check the source archive.",
         "Aucun modèle dans cette catégorie. Vérifiez l'archive source."},
        {"3DS preview (400 x 240)", "Aperçu 3DS (400 x 240)"},
        {"3DS preview at native size", "Aperçu 3DS en taille native"},
        {"Map diagnostics", "Diagnostic des cartes"},
        {"Reset this workspace layout", "Réinitialiser la disposition"}
    };
    const auto found = french_strings.find(english);
    return found == french_strings.end() ? english : found->second;
}
} // namespace studio::localization

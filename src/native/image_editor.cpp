#include "native/activity_log.h"
#include "native/localization.h"
#include "native/canvas_navigation.h"
#include "inspector_selector.h"
#include "native/tutorial_widgets.h"
#include "native/image_editor.h"
#include "native/imgui_renderer.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>
namespace studio {
namespace {
bgfx::TextureHandle upload(const TextureImage &image) {
    return bgfx::createTexture2D(image.width, image.height, false, 1, bgfx::TextureFormat::RGBA8,
                                 BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                 bgfx::copy(image.rgba.data(), narrow(image.rgba.size())));
}
TextureImage png_read(const std::filesystem::path &file, const NativeImageInfo &expected,
                      bool auto_resize, SDL_ScaleMode scale_mode) {
    require(std::filesystem::file_size(file) <= 64 * 1024 * 1024,
            "PNG exceeds the 64 MiB import limit");
    auto b = read_file(file);
    require(b.size() >= 24 && u32(b, 0) == 0x474e5089, "Choose a PNG image");
    auto big = [&](unsigned p) {
        return (unsigned(b[p]) << 24) | (unsigned(b[p + 1]) << 16) | (unsigned(b[p + 2]) << 8) |
               b[p + 3];
    };
    auto w = big(16), h = big(20);
    require(w > 0 && h > 0 && w <= 16384 && h <= 16384, "Invalid PNG dimensions");
    require(auto_resize || (w == expected.width && h == expected.height),
            "Expected " + std::to_string(expected.width) + " x " + std::to_string(expected.height) +
                " pixels; received " + std::to_string(w) + " x " + std::to_string(h));
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> input(
        SDL_LoadPNG_IO(SDL_IOFromConstMem(b.data(), b.size()), true), SDL_DestroySurface);
    require(bool(input), SDL_GetError());
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> rgba(
        SDL_ConvertSurface(input.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    require(bool(rgba) && unsigned(rgba->w) == w && unsigned(rgba->h) == h,
            "PNG pixel conversion failed");
    SDL_Surface *pixels = rgba.get();
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> scaled(nullptr, SDL_DestroySurface);
    if (w != expected.width || h != expected.height) {
        scaled.reset(SDL_ScaleSurface(rgba.get(), expected.width, expected.height, scale_mode));
        require(bool(scaled), std::string("PNG resize failed: ") + SDL_GetError());
        pixels = scaled.get();
    }
    TextureImage image{expected.width, expected.height,
                       Bytes(std::size_t(expected.width) * expected.height * 4)};
    for (unsigned y = 0; y < expected.height; ++y)
        std::memcpy(image.rgba.data() + std::size_t(y) * expected.width * 4,
                    static_cast<const std::uint8_t *>(pixels->pixels) + std::size_t(y) * pixels->pitch,
                    std::size_t(expected.width) * 4);
    return image;
}
TextureImage resized_image(const TextureImage &source, int width, int height,
                           SDL_ScaleMode mode) {
    require(width > 0 && height > 0 && width <= 4096 && height <= 4096,
            "Resize dimensions must be between 1 and 4096 pixels");
    require(std::size_t(width) * std::size_t(height) <= 4096u * 4096u,
            "Resized image is too large");
    auto *input = SDL_CreateSurfaceFrom(source.width, source.height,
                                        SDL_PIXELFORMAT_RGBA32,
                                        const_cast<std::uint8_t *>(source.rgba.data()),
                                        int(source.width * 4));
    require(input != nullptr, SDL_GetError());
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> original(input, SDL_DestroySurface);
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> output(
        SDL_ScaleSurface(original.get(), width, height, mode), SDL_DestroySurface);
    require(bool(output), SDL_GetError());
    TextureImage result{unsigned(width), unsigned(height), Bytes(std::size_t(width) * height * 4)};
    for (int y = 0; y < height; ++y)
        std::memcpy(result.rgba.data() + std::size_t(y) * width * 4,
                    static_cast<const std::uint8_t *>(output->pixels) + std::size_t(y) * output->pitch,
                    std::size_t(width) * 4);
    return result;
}
void png_write(const std::filesystem::path &file, const TextureImage &image) {
    auto *surface =
        SDL_CreateSurfaceFrom(image.width, image.height, SDL_PIXELFORMAT_RGBA32,
                              const_cast<std::uint8_t *>(image.rgba.data()), image.width * 4);
    require(surface != nullptr, SDL_GetError());
    auto *io = SDL_IOFromDynamicMem();
    if (!io) {
        SDL_DestroySurface(surface);
        throw std::runtime_error(SDL_GetError());
    }
    bool ok = SDL_SavePNG_IO(surface, io, false);
    SDL_DestroySurface(surface);
    Bytes bytes;
    if (ok) {
        auto size = SDL_GetIOSize(io);
        auto *data = static_cast<const std::uint8_t *>(SDL_GetPointerProperty(
            SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
        if (data && size > 0)
            bytes.assign(data, data + size);
    }
    SDL_CloseIO(io);
    require(!bytes.empty(), "PNG encoding failed");
    write_file_atomic(file, bytes);
}
const char *languages[] = {"Default",
                           "Japanese",
                           "English",
                           "French",
                           "Italian",
                           "German",
                           "Unused",
                           "Spanish",
                           "Korean",
                           "Chinese simplified",
                           "Chinese traditional"};
std::string lower(std::string s) {
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}
}
ImageEditor::~ImageEditor() {
    cancel_ = true;
    if (scan_.valid())
        scan_.wait();
    if (export_.valid())
        export_.wait();
    if (encoding_.valid())
        encoding_.wait();
    clear_textures();
}
void ImageEditor::clear_textures() {
    if (bgfx::isValid(compare_original_)) bgfx::destroy(compare_original_);
    if (bgfx::isValid(compare_current_)) bgfx::destroy(compare_current_);
    compare_original_ = BGFX_INVALID_HANDLE;
    compare_current_ = BGFX_INVALID_HANDLE;
    compare_index_ = std::size_t(-1);
    if (bgfx::isValid(preview_))
        bgfx::destroy(preview_);
    preview_ = BGFX_INVALID_HANDLE;
    for (auto &[key, t] : thumbnails_) {
        (void)key;
        if (bgfx::isValid(t))
            bgfx::destroy(t);
    }
    thumbnails_.clear();
}
void ImageEditor::request_leave(std::function<void()> action) {
    if (project_store()) {
        try {
            save_editor_project();
            action();
        } catch (const std::exception &e) {
            error_ = e.what();
        }
        return;
    }
    if (leave_)
        return;
    if (export_.valid() || encoding_.valid()) {
        notice_ = "Wait for image processing to finish before continuing.";
        return;
    }
    if (document_ && document_->dirty())
        leave_ = std::move(action);
    else
        action();
}
void ImageEditor::scan(const std::filesystem::path &dump) {
    cancel_ = false;
    progress_ = 0;
    total_ = 1;
    error_.clear();
    scan_ = std::async(std::launch::async, [this, dump] {
        return scan_image_catalog(dump, [this](unsigned done, unsigned total) {
            progress_ = done;
            total_ = total;
            return !cancel_;
        });
    });
}
void ImageEditor::filter() {
    filtered_.clear();
    if (!document_)
        return;
    auto query = lower(search_);
    for (std::size_t n = 0; n < document_->catalog.images.size(); ++n) {
        auto &i = document_->catalog.images[n];
        if (category_ != "All" && category_ != i.category)
            continue;
        if (language_ >= 0 && i.subfile != unsigned(language_))
            continue;
        if (!query.empty() && lower(i.name).find(query) == std::string::npos)
            continue;
        filtered_.push_back(n);
    }
    if (!filtered_.empty() &&
        std::find(filtered_.begin(), filtered_.end(), selected_) == filtered_.end())
        select(filtered_[0]);
}
void ImageEditor::select(std::size_t index) {
    selected_ = index;
    if (compare_index_ != index) {
        if (bgfx::isValid(compare_original_)) bgfx::destroy(compare_original_);
        if (bgfx::isValid(compare_current_)) bgfx::destroy(compare_current_);
        compare_original_ = BGFX_INVALID_HANDLE;
        compare_current_ = BGFX_INVALID_HANDLE;
        compare_index_ = std::size_t(-1);
    }
    activity_log::add("Images", "Selected image entry " + std::to_string(index));
    manual_width_ = 0;
    manual_height_ = 0;
    imported_.clear();
    refresh();
}
void ImageEditor::refresh() {
    if (bgfx::isValid(compare_original_)) bgfx::destroy(compare_original_);
    if (bgfx::isValid(compare_current_)) bgfx::destroy(compare_current_);
    compare_original_ = compare_current_ = BGFX_INVALID_HANDLE;
    compare_index_ = std::size_t(-1);
    if (bgfx::isValid(preview_))
        bgfx::destroy(preview_);
    preview_ = BGFX_INVALID_HANDLE;
    auto thumb = thumbnails_.find(selected_);
    if (thumb != thumbnails_.end()) {
        if (bgfx::isValid(thumb->second))
            bgfx::destroy(thumb->second);
        thumbnails_.erase(thumb);
    }
    if (!document_ || selected_ >= document_->catalog.images.size())
        return;
    try {
        auto image = decode_native_image(original_ ? document_->original(selected_)
                                                   : document_->current(selected_));
        if (channel_)
            for (std::size_t p = 0; p < image.rgba.size(); p += 4) {
                if (channel_ >= 2) {
                    auto value = image.rgba[p + unsigned(channel_ - 2)];
                    image.rgba[p] = image.rgba[p + 1] = image.rgba[p + 2] = value;
                }
                image.rgba[p + 3] = 255;
            }
        preview_ = upload(image);
        error_.clear();
    } catch (const std::exception &e) {
        error_ = e.what();
    }
}
void ImageEditor::save(const std::filesystem::path &file) {
    document_->save(file);
    saved_ = file;
    notice_ = "Image project saved.";
    activity_log::add("Images", notice_);
    if (leave_) {
        auto action = std::move(leave_);
        leave_ = {};
        action();
    }
}
void ImageEditor::import_file(const std::filesystem::path &file) {
    auto image = png_read(file, document_->catalog.images.at(selected_).info,
                          auto_resize_png_, resize_filter_ == 0 ? SDL_SCALEMODE_NEAREST
                                                                 : SDL_SCALEMODE_LINEAR);
    auto source = document_->current(selected_);
    pending_import_ = file;
    error_.clear();
    encoding_ =
        std::async(std::launch::async, [source = std::move(source), image = std::move(image)] {
            return replace_native_image(source, image);
        });
}
void ImageEditor::choose(Action action) {
    if (action == Action::Save && project_store()) {
        try {
            save_editor_project();
            notice_ = "Project saved. Stage Project builds the overlay.";
            error_.clear();
        } catch (const std::exception &e) {
            error_ = e.what();
        }
        return;
    }
    action_ = action;
    if (action == Action::Export || action == Action::ExportSelectedPng) {
        choose_folder(window_, dialog_, nullptr);
        return;
    }
    if (action == Action::Import) {
        choose_png(window_, dialog_);
        return;
    }
    {
        std::lock_guard lock(dialog_->mutex);
        dialog_->pending = true;
        dialog_->ready = false;
        dialog_->path.clear();
        dialog_->error.clear();
    }
    auto *owner = new std::shared_ptr<FolderSelection>(dialog_);
    static const SDL_DialogFileFilter png[] = {{"PNG image", "png"}},
                                      project[] = {{"Image replacement project", "usum-images"}};
    auto callback = [](void *data, const char *const *files, int) {
        std::unique_ptr<std::shared_ptr<FolderSelection>> owner(
            static_cast<std::shared_ptr<FolderSelection> *>(data));
        auto &state = **owner;
        std::lock_guard lock(state.mutex);
        if (!files)
            state.error = SDL_GetError();
        else if (files[0])
            state.path = files[0];
        state.pending = false;
        state.ready = true;
    };
    if (action == Action::Load)
        SDL_ShowOpenFileDialog(callback, owner, window_, project, 1, nullptr, false);
    else
        SDL_ShowSaveFileDialog(callback, owner, window_,
                               (action == Action::ExportPng || action == Action::ResizePng) ? png : project, 1,
                               (action == Action::ExportPng || action == Action::ResizePng) ? "image.png"
                               : saved_.empty()            ? "images.usum-images"
                                                           : saved_.string().c_str());
}
void ImageEditor::update() {
    if (scan_.valid() && scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        try {
            auto catalog = scan_.get();
            clear_textures();
            document_ = std::make_unique<ImageDocument>(std::move(catalog));
            bind_project();
            saved_.clear();
            selected_ = std::size_t(-1);
            category_ = "All";
            categories_ = {"All"};
            std::set<std::string> categories;
            for (auto &i : document_->catalog.images)
                categories.insert(i.category);
            categories_.insert(categories_.end(), categories.begin(), categories.end());
            filter();
            if (!filtered_.empty())
                select(filtered_[0]);
            notice_ = "Image library ready. Replacement dimensions and native format are fixed.";
        } catch (const std::exception &e) {
            error_ = e.what();
        }
    std::string path;
    {
        std::lock_guard lock(dialog_->mutex);
        if (dialog_->ready) {
            dialog_->ready = false;
            path = dialog_->path;
            if (!dialog_->error.empty())
                error_ = dialog_->error;
        }
    }
    if (!path.empty() && document_)
        try {
            auto file = std::filesystem::u8path(path);
            if (action_ == Action::Import) {
                import_file(file);
            } else if (action_ == Action::ExportPng || action_ == Action::ResizePng) {
                if (file.extension().empty())
                    file += ".png";
                auto image = decode_native_image(original_ ? document_->original(selected_)
                                                              : document_->current(selected_));
                if (action_ == Action::ResizePng)
                    image = resized_image(image, manual_width_, manual_height_,
                                          manual_filter_ == 0 ? SDL_SCALEMODE_NEAREST
                                                              : SDL_SCALEMODE_LINEAR);
                png_write(file, image);
                activity_log::add("Images", "Saved PNG: " + file.string());
                notice_ = action_ == Action::ResizePng
                              ? studio::localization::bilingual("Resized PNG saved.", "PNG redimensionné enregistré.")
                              : studio::localization::bilingual("PNG exported.", "PNG exporté.");
            } else if (action_ == Action::ExportSelectedPng) {
                require(!multiple_selection_.empty(), "Select images before batch export");
                auto snapshot = *document_;
                auto selection = multiple_selection_;
                export_ = std::async(std::launch::async,
                    [snapshot = std::move(snapshot), selection = std::move(selection), file] {
                        std::filesystem::create_directories(file);
                        for (auto index : selection) {
                            auto image = decode_native_image(snapshot.current(index));
                            png_write(file / ("image_" + std::to_string(index) + ".png"), image);
                        }
                        return std::to_string(selection.size()) + " PNG images exported to " + file.string();
                    });
                activity_log::add("Images", "Started batch PNG export: " + std::to_string(multiple_selection_.size()) + " images");
            } else if (action_ == Action::Save) {
                if (file.extension().empty())
                    file += ".usum-images";
                save(file);
            } else if (action_ == Action::Load) {
                document_->load(file);
                project_.imported();
                saved_ = file;
                clear_textures();
                refresh();
                notice_ = "Image project opened.";
            } else {
                auto snapshot = *document_;
                export_ = std::async(std::launch::async, [snapshot = std::move(snapshot), file] {
                    snapshot.export_to(file);
                    return "Game images exported to " + file.string();
                });
            }
            error_.clear();
        } catch (const std::exception &e) {
            error_ = e.what();
        }
    if (encoding_.valid() &&
        encoding_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        try {
            document_->replace_encoded(selected_, encoding_.get());
            imported_ = pending_import_;
            original_ = false;
            refresh();
            notice_ = "Replacement applied. Preview shows the native encoded pixels.";
            activity_log::add("Images", "Replaced image entry " + std::to_string(selected_));
        } catch (const std::exception &e) {
            error_ = e.what();
        }
    if (export_.valid() && export_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        try {
            notice_ = export_.get();
            activity_log::add("Images", notice_);
            error_.clear();
        } catch (const std::exception &e) {
            error_ = e.what();
        }
    if (leave_ && !ImGui::IsPopupOpen("Unsaved image replacements"))
        ImGui::OpenPopup("Unsaved image replacements");
    if (ImGui::BeginPopupModal("Unsaved image replacements", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!leave_) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::TextUnformatted(studio::localization::tr("Save image replacements before continuing?"));
        if (!error_.empty())
            ImGui::TextWrapped("%s", error_.c_str());
        bool pending;
        {
            std::lock_guard lock(dialog_->mutex);
            pending = dialog_->pending;
        }
        ImGui::BeginDisabled(pending);
        if (studio::TutorialWidgets::Button("image_editor", "Save image project"))
            try {
                if (saved_.empty())
                    choose(Action::Save);
                else {
                    save(saved_);
                    ImGui::CloseCurrentPopup();
                }
            } catch (const std::exception &e) {
                error_ = e.what();
            }
        ImGui::SameLine();
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::tr("Discard image changes"))) {
            document_->discard();
            clear_textures();
            refresh();
            auto action = std::move(leave_);
            leave_ = {};
            ImGui::CloseCurrentPopup();
            action();
        }
        ImGui::SameLine();
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::tr("Cancel"))) {
            leave_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}
void ImageEditor::draw(const std::filesystem::path &dump) {
    bool pending;
    {
        std::lock_guard lock(dialog_->mutex);
        pending = dialog_->pending;
    }
    bool busy = pending || scan_.valid() || export_.valid() || encoding_.valid() || bool(leave_);
    ImGui::Begin("Image library");
    ImGui::BeginDisabled(busy);
    if (studio::TutorialWidgets::Button("image_editor",
                                        document_ ? "Rescan image library" : "Load image library"))
        request_leave([this, dump] {
            scan(dump);
        });
    ImGui::EndDisabled();
    if (scan_.valid()) {
        ImGui::ProgressBar(float(progress_) / float(std::max(1u, total_.load())));
        ImGui::TextUnformatted(studio::localization::tr("Reading image sources..."));
    }
    if (document_) {
        if (document_->catalog.dump != dump)
            ImGui::TextWrapped("Using the previously scanned dump. Rescan to switch sources.");
        ImGui::BeginDisabled(busy);
        InspectorSelectorStyle category_style("Choose image category");
        if (ImGui::BeginCombo("##image-category", category_.c_str())) {
            for (auto &c : categories_)
                if (ImGui::Selectable(c.c_str(), c == category_)) {
                    category_ = c;
                    filter();
                }
            ImGui::EndCombo();
        }
        category_style.end();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##image-search", "Search image name or number", search_,
                                     sizeof(search_)))
            filter();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##image-language",
                              language_ < 0 ? "All language subfiles" : languages[language_])) {
            if (ImGui::Selectable(studio::localization::tr("All language subfiles"), language_ < 0)) {
                language_ = -1;
                filter();
            }
            for (int n = 0; n < 11; ++n)
                if (ImGui::Selectable(languages[n], language_ == n)) {
                    language_ = n;
                    filter();
                }
            ImGui::EndCombo();
        }
        ImGui::Text("%zu images", filtered_.size());
        if (!multiple_selection_.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("| %zu %s", multiple_selection_.size(),
                studio::localization::bilingual("selected (Ctrl / Shift)", "selectionnees (Ctrl / Shift)"));
            ImGui::SameLine();
            if (ImGui::SmallButton(studio::localization::bilingual("Export selection as PNG...", "Exporter selection en PNG...")))
                choose(Action::ExportSelectedPng);
            ImGui::SameLine();
            if (ImGui::SmallButton(studio::localization::bilingual("Clear selection", "Effacer la selection")))
                multiple_selection_.clear();
        }
        ImGui::BeginChild("Image entries", {0, 0});
        ImGuiListClipper clipper;
        float row = 40;
        clipper.Begin(int(filtered_.size()), row);
        unsigned loaded = 0;
        while (clipper.Step())
            for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n) {
                auto index = filtered_[n];
                auto &item = document_->catalog.images[index];
                ImGui::PushID(int(index));
                auto start = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##image-row", multiple_selection_.contains(index) || selected_ == index, 0,
                                      {0, row - ImGui::GetStyle().ItemSpacing.y})) {
                    const auto &io = ImGui::GetIO();
                    if (io.KeyCtrl) {
                        if (!multiple_selection_.erase(index)) multiple_selection_.insert(index);
                        selection_anchor_ = index;
                    } else if (io.KeyShift && selection_anchor_ != std::size_t(-1)) {
                        auto first = std::find(filtered_.begin(), filtered_.end(), selection_anchor_);
                        auto last = std::find(filtered_.begin(), filtered_.end(), index);
                        if (first != filtered_.end() && last != filtered_.end()) {
                            if (first > last) std::swap(first, last);
                            for (; first <= last; ++first) multiple_selection_.insert(*first);
                        }
                    } else {
                        multiple_selection_.clear();
                        selection_anchor_ = index;
                        select(index);
                    }
                }
                if (ImGui::BeginPopupContextItem("##image-context")) {
                    if (ImGui::MenuItem(studio::localization::bilingual("Open image", "Ouvrir l'image"))) {
                        multiple_selection_.clear(); select(index);
                    }
                    if (ImGui::MenuItem(studio::localization::bilingual("Compare original / modified", "Comparer original / modifie"))) {
                        select(index); compare_open_ = true;
                    }
                    if (ImGui::MenuItem(studio::localization::bilingual("Copy resource name", "Copier le nom")))
                        ImGui::SetClipboardText(item.name.c_str());
                    ImGui::EndPopup();
                }
                if (!thumbnails_.contains(index) && loaded < 2) {
                    ++loaded;
                    try {
                        thumbnails_[index] = upload(decode_native_image(document_->current(index)));
                    } catch (...) {
                        thumbnails_[index] = BGFX_INVALID_HANDLE;
                    }
                }
                auto t = thumbnails_.find(index);
                if (t != thumbnails_.end() && bgfx::isValid(t->second)) {
                    float scale = 32.f / std::max(item.info.width, item.info.height);
                    ImGui::GetWindowDrawList()->AddImage(
                        ImTextureID(ImGuiRenderer::image_id(t->second)), {start.x + 2, start.y + 2},
                        {start.x + 2 + item.info.width * scale,
                         start.y + 2 + item.info.height * scale});
                }
                auto name = (document_->edited(index) ? "* " : "") + item.name;
                ImGui::GetWindowDrawList()->AddText(
                    {start.x + 40, start.y + 2}, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%u x %u | %s | subfile %u", item.name.c_str(),
                                      item.info.width, item.info.height,
                                      native_image_format_name(item.info.format), item.subfile);
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::Begin("Image preview");
    if (document_ && selected_ < document_->catalog.images.size()) {
        ImGui::BeginDisabled(busy);
        if (ImGui::Button(studio::localization::bilingual("Compare in separate window...", "Comparer dans une fenetre...")))
            compare_open_ = true;
        ImGui::SameLine();
        if (studio::TutorialWidgets::Checkbox("image_editor", "Original", &original_))
            refresh();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        if (ImGui::Combo("Channels", &channel_, "RGBA\0RGB\0Red\0Green\0Blue\0Alpha\0"))
            refresh();
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Zoom", &zoom_, .1f, 64, "%.2fx");
        ImGui::SameLine();
        if (ImGui::Button(studio::localization::tr("Frame image (F)"))) {
            zoom_ = 1;
            pan_ = {};
        }
        ImGui::SameLine();
        studio::TutorialWidgets::Checkbox("image_editor", "Pixel grid", &grid_);
        if (ImGui::Button(studio::localization::tr("Display...")))
            ImGui::OpenPopup("Image display");
        if (ImGui::BeginPopup("Image display")) {
            studio::TutorialWidgets::Checkbox("image_editor", "Checkerboard", &checker_);
            if (!checker_) {
                ImGui::SameLine();
                ImGui::ColorEdit3("Background", background_, ImGuiColorEditFlags_NoInputs);
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        auto origin = ImGui::GetCursorScreenPos();
        ImVec2 area{std::max(32.f, ImGui::GetContentRegionAvail().x),
                    std::max(96.f, ImGui::GetContentRegionAvail().y)};
        ImGui::InvisibleButton("##image-canvas", area,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        navigate_canvas(origin, area, ImGui::IsItemHovered(), zoom_, pan_);
        auto &info = document_->catalog.images[selected_].info;
        float pixel_scale = std::min(area.x / info.width, area.y / info.height) * zoom_;
        ImVec2 size{info.width * pixel_scale, info.height * pixel_scale};
        ImVec2 start{origin.x + (area.x - size.x) * .5f + pan_.x,
                     origin.y + (area.y - size.y) * .5f + pan_.y};
        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, {origin.x + area.x, origin.y + area.y}, true);
        if (checker_) {
            auto clip_min = origin, clip_size = area;
            float x0 = std::max(0.f, std::floor((clip_min.x - start.x) / 16) * 16),
                  y0 = std::max(0.f, std::floor((clip_min.y - start.y) / 16) * 16);
            for (float y = y0; y < std::min(size.y, clip_min.y + clip_size.y - start.y); y += 16)
                for (float x = x0; x < std::min(size.x, clip_min.x + clip_size.x - start.x);
                     x += 16)
                    draw->AddRectFilled(
                        {start.x + x, start.y + y},
                        {start.x + std::min(x + 16, size.x), start.y + std::min(y + 16, size.y)},
                        (int(x / 16 + y / 16) & 1) ? IM_COL32(66, 72, 79, 255)
                                                   : IM_COL32(43, 48, 55, 255));
        } else
            draw->AddRectFilled(start, {start.x + size.x, start.y + size.y},
                                ImGui::ColorConvertFloat4ToU32(
                                    {background_[0], background_[1], background_[2], 1}));
        if (bgfx::isValid(preview_))
            draw->AddImage(ImTextureID(ImGuiRenderer::image_id(preview_)), start,
                           {start.x + size.x, start.y + size.y});
        if (grid_ && pixel_scale >= 4) {
            for (unsigned x = 0; x <= info.width; ++x)
                draw->AddLine({start.x + x * pixel_scale, start.y},
                              {start.x + x * pixel_scale, start.y + size.y}, IM_COL32(0, 0, 0, 80));
            for (unsigned y = 0; y <= info.height; ++y)
                draw->AddLine({start.x, start.y + y * pixel_scale},
                              {start.x + size.x, start.y + y * pixel_scale}, IM_COL32(0, 0, 0, 80));
        }
        draw->PopClipRect();
    } else
        ImGui::TextWrapped("Load the image library to begin.");
    ImGui::End();
    if (compare_open_) {
        ImGui::SetNextWindowSize(ImVec2(800, 480), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(studio::localization::bilingual("Image comparison - Original / Modified", "Comparaison d'images - Original / Modifie"), &compare_open_)) {
            if (document_ && selected_ < document_->catalog.images.size()) {
                const auto &info = document_->catalog.images[selected_].info;
                ImGui::TextUnformatted(document_->catalog.images[selected_].name.c_str());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::SliderFloat("Zoom##comparison", &compare_zoom_, .2f, 6.f, "%.1fx");
                if (compare_index_ != selected_ || !bgfx::isValid(compare_original_) || !bgfx::isValid(compare_current_)) {
                    if (bgfx::isValid(compare_original_)) bgfx::destroy(compare_original_);
                    if (bgfx::isValid(compare_current_)) bgfx::destroy(compare_current_);
                    compare_original_ = compare_current_ = BGFX_INVALID_HANDLE;
                    compare_index_ = selected_;
                    try {
                        compare_original_ = upload(decode_native_image(document_->original(selected_)));
                        compare_current_ = upload(decode_native_image(document_->current(selected_)));
                    } catch (const std::exception &e) {
                        error_ = e.what();
                    }
                }
                const float panel_width = std::max(100.f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * .5f);
                auto panel = [&](const char *label, bgfx::TextureHandle texture) {
                    ImGui::BeginChild(label, ImVec2(panel_width, 0), ImGuiChildFlags_Borders);
                    ImGui::TextUnformatted(label);
                    const ImVec2 avail = ImGui::GetContentRegionAvail();
                    if (bgfx::isValid(texture) && info.width && info.height) {
                        const float scale = std::min(avail.x / info.width, avail.y / info.height) * compare_zoom_;
                        ImGui::Image(ImTextureID(ImGuiRenderer::image_id(texture)),
                                     ImVec2(info.width * scale, info.height * scale));
                    }
                    ImGui::EndChild();
                };
                panel(studio::localization::bilingual("Original", "Original"), compare_original_);
                ImGui::SameLine();
                panel(studio::localization::bilingual("Modified", "Modifie"), compare_current_);
            } else ImGui::TextDisabled("No image selected.");
        }
        ImGui::End();
    }
    ImGui::Begin("Image properties");
    ImGui::BeginDisabled(busy);
    if (document_) {
        ImGui::Separator();
        if (studio::TutorialWidgets::Button("image_editor", "Undo")) {
            if (document_->undo()) {
                clear_textures();
                refresh();
            }
        }
        ImGui::SameLine();
        if (studio::TutorialWidgets::Button("image_editor", "Redo")) {
            if (document_->redo()) {
                clear_textures();
                refresh();
            }
        }
        ImGui::Text("%zu replacements%s", document_->size(),
                    document_->dirty() ? " | Unsaved" : "");
        if (studio::TutorialWidgets::Button(
                "image_editor", project_store() ? "Save Project" : "Save image project..."))
            choose(Action::Save);
        if (studio::TutorialWidgets::Button("image_editor", "Open image project..."))
            request_leave([this] {
                choose(Action::Load);
            });
        ImGui::BeginDisabled(!document_->size());
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::tr("Export game images...")))
            choose(Action::Export);
        ImGui::EndDisabled();
        if (studio::TutorialWidgets::TreeNode("image_editor", "Scan coverage")) {
            ImGui::TextWrapped("Standalone images and images embedded in known UI packages. Model "
                               "textures stay in Studio. Unsupported sources are skipped.");
            for (auto &d : document_->catalog.diagnostics)
                ImGui::TextWrapped("%s", d.c_str());
            ImGui::TreePop();
        }
    }
    ImGui::Separator();
    if (document_ && selected_ < document_->catalog.images.size()) {
        auto &item = document_->catalog.images[selected_];
        ImGui::TextWrapped("%s", item.name.c_str());
        ImGui::Text("%u x %u | %s", item.info.width, item.info.height,
                    native_image_format_name(item.info.format));
        ImGui::SeparatorText("Edit image");
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::bilingual("Extract texture as PNG...", "Extraire la texture en PNG...")))
            choose(Action::ExportPng);
        ImGui::SeparatorText(studio::localization::bilingual("Manual PNG resize", "Redimensionnement manuel du PNG"));
        if (manual_width_ == 0 || manual_height_ == 0) {
            manual_width_ = int(item.info.width);
            manual_height_ = int(item.info.height);
        }
        ImGui::SetNextItemWidth(130.0f);
        ImGui::InputInt(studio::localization::bilingual("Width (px)##manual-width", "Largeur (px)##manual-width"), &manual_width_);
        ImGui::SetNextItemWidth(130.0f);
        ImGui::InputInt(studio::localization::bilingual("Height (px)##manual-height", "Hauteur (px)##manual-height"), &manual_height_);
        ImGui::SetNextItemWidth(260.0f);
        ImGui::Combo("##manual-resize-filter", &manual_filter_,
                     studio::localization::bilingual(
                         "Nearest (sharp pixels)\0Linear (smooth)\0",
                         "Voisin le plus proche (pixels nets)\0Linéaire (lissé)\0"));
        ImGui::BeginDisabled(manual_width_ <= 0 || manual_height_ <= 0 ||
                             manual_width_ > 4096 || manual_height_ > 4096);
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::bilingual(
                "Resize and save PNG...", "Redimensionner et enregistrer le PNG...")))
            choose(Action::ResizePng);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s", studio::localization::bilingual(
            "Exports a separate resized PNG. Does not modify the game texture.",
            "Exporte un PNG redimensionné séparé. Ne modifie pas la texture du jeu."));
        ImGui::SeparatorText(studio::localization::bilingual("Resize on import", "Redimensionner à l'import"));
        ImGui::Checkbox(studio::localization::bilingual(
                            "Automatically resize PNG to texture dimensions",
                            "Redimensionner automatiquement le PNG aux dimensions de la texture"),
                        &auto_resize_png_);
        if (auto_resize_png_) {
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##resize-filter", &resize_filter_,
                         studio::localization::bilingual(
                             "Nearest (sharp pixels)\0Linear (smooth)\0",
                             "Voisin le plus proche (pixels nets)\0Linéaire (lissé)\0"));
            ImGui::TextWrapped("%s: %u x %u",
                studio::localization::bilingual("Target size", "Taille cible"),
                item.info.width, item.info.height);
            ImGui::TextWrapped("%s",
                studio::localization::bilingual(
                    "The source image is resized before conversion. Its aspect ratio may change.",
                    "L'image source est redimensionnée avant conversion. Ses proportions peuvent changer."));
        }
        if (studio::TutorialWidgets::Button("image_editor", studio::localization::bilingual(
                "Import / replace PNG...", "Importer / remplacer le PNG..."), {-1, 0}))
            choose(Action::Import);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s %u x %u",
                studio::localization::bilingual("Native texture size:", "Taille native de la texture :"),
                item.info.width, item.info.height);
        ImGui::BeginDisabled(imported_.empty());
        if (studio::TutorialWidgets::Button("image_editor", "Reload last PNG"))
            try {
                import_file(imported_);
            } catch (const std::exception &e) {
                error_ = e.what();
            }
        ImGui::EndDisabled();
        if (studio::TutorialWidgets::Button("image_editor", "Reset image")) {
            document_->reset(selected_);
            refresh();
        }
        if (studio::TutorialWidgets::TreeNode("image_editor", "Source")) {
            ImGui::TextWrapped("%s\nMember %u / subfile %u", item.archive.c_str(), item.member,
                               item.subfile);
            ImGui::TextWrapped("Only this source image is replaced. Matching names in other "
                               "packages remain separate.");
            ImGui::TreePop();
        }
    }
    ImGui::EndDisabled();
    if (encoding_.valid())
        ImGui::TextUnformatted(studio::localization::tr("Encoding replacement image..."));
    if (export_.valid())
        ImGui::TextUnformatted(studio::localization::tr("Exporting image archives..."));
    if (!notice_.empty())
        ImGui::TextWrapped("%s", notice_.c_str());
    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, {1, .45f, .4f, 1});
        ImGui::TextWrapped("%s", error_.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::End();
    if (thumbnails_.size() > 256) {
        for (auto it = thumbnails_.begin(); it != thumbnails_.end() && thumbnails_.size() > 128;) {
            if (it->first == selected_) {
                ++it;
                continue;
            }
            if (bgfx::isValid(it->second))
                bgfx::destroy(it->second);
            it = thumbnails_.erase(it);
        }
    }
}
void ImageEditor::bind_project() {
    if (!document_)
        return;
    project_.bind(
        "images", "images", "Game images", "",
        [this] {
            return document_ && document_->dirty();
        },
        [this] {
            require(!encoding_.valid() && !export_.valid(),
                    "Wait for image processing before saving");
            return project_encode_file([copy = *document_](const auto &path) mutable {
                copy.save(path);
            });
        },
        [this] {
            document_->mark_saved();
        });
    project_.ready([this] {
        if (document_) {
            require(!encoding_.valid() && !export_.valid(),
                    "Wait for image processing before saving");
        }
    });
    if (auto file = project_.document(); !file.empty()) {
        document_->load(file);
    }
}
}

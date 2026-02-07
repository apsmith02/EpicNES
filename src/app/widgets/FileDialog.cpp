#include "FileDialog.h"

namespace fs = std::filesystem;

FileDialog::FileDialog()
{
    FileDialog("FileDialog");
}

FileDialog::FileDialog(std::string &&title)
{
    ChangeDir(fs::current_path());
    filepath = fs::path();
    this->title = title;
}

bool FileDialog::Update()
{
    bool fileConfirmed = false;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail(), ImGuiCond_Appearing);
    if (active) {
        if (ImGui::Begin(title.c_str(), &active)) {
            //Directory path text input
            if (ImGui::InputText("Current Directory", inputTextBuffer, sizeof(inputTextBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                ChangeDir(fs::path(inputTextBuffer));
            }
            
            // Parent directory button (grayed out when at root directory)
            fs::path parent = dir.parent_path();
            bool isRoot = parent.empty();

            if (isRoot)
                ImGui::BeginDisabled();

            if (ImGui::Button("<-")) {
                ChangeDir(std::move(parent));
            }

            if (isRoot)
                ImGui::EndDisabled();

            //Directory contents
            if (ImGui::BeginListBox("##ListBox")) {
                // Iterate all entries in directory
                for (const auto& entry : fs::directory_iterator(dir)) {
                    fs::path entryPath = entry.path();
                    std::string entryStr = entryPath.filename().string().c_str();

                    bool selected = entryPath == filepath;

                    if (ImGui::Selectable(entryStr.c_str(), selected)) {
                        //Update selection on click
                        filepath = entryPath;
                    }
                }
                ImGui::EndListBox();
            }

            //Confirm/cancel buttons
            if (ImGui::Button("Confirm")) {
                if (fs::is_directory(filepath)) {
                    //Open selected directory
                    ChangeDir(filepath);
                } else if (fs::is_regular_file(filepath)) {
                    //Confirm selected file
                    fileConfirmed = true;
                    active = false;
                }
            }
            if (ImGui::Button("Cancel")) {
                filepath = fs::path();
                active = false;
            }
        }
        ImGui::End();
    }

    return fileConfirmed;
}

void FileDialog::Show()
{
    filepath = fs::path();
    active = true;
}

void FileDialog::ChangeDir(std::filesystem::path &newDir)
{
    if (fs::exists(newDir))
        dir = newDir;
    filepath = fs::path();
    ResetInputTextBuffer();
}

void FileDialog::ChangeDir(fs::path &&newDir)
{
    if (fs::exists(newDir))
        dir = newDir;
    filepath = fs::path();
    ResetInputTextBuffer();
}

void FileDialog::ResetInputTextBuffer()
{
    strcpy(inputTextBuffer, dir.string().c_str());
}

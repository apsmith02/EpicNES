#pragma once
#include <imgui.h>
#include <filesystem>
#include <string>

class FileDialog {
private:
    std::filesystem::path dir; //Path of current directory
    std::filesystem::path filepath; //Path of currently selected file

    std::string title;

    bool active = false;
    char inputTextBuffer[4096];
public:
    FileDialog();
    FileDialog(std::string&& title);

    /*
    * If file dialog is open, draw and process file dialog input.
    *
    * @return True when a file is confirmed.
    */
    bool Update();

    void Show();

    const std::filesystem::path& GetFilePath() { return filepath; }
private:
    //Change directory path if it exists, clear filepath selection, update inputTextBuffer with new path
    void ChangeDir(std::filesystem::path& newDir);
    void ChangeDir(std::filesystem::path&& newDir);
    void ResetInputTextBuffer();
};
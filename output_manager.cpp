#include "output_manager.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gst
{

OutputManager::OutputManager(std::string result_dir, std::string main_result_filename)
    : result_dir_(std::move(result_dir)), main_result_filename_(std::move(main_result_filename))
{
    fs::create_directories(result_dir_);
}

void OutputManager::BeginResultRun(const std::string& filename, const std::string& header) const
{
    fs::path out_path = fs::path(result_dir_) / filename;
    const bool existed = fs::exists(out_path) && fs::file_size(out_path) > 0;
    std::ofstream fout(out_path, std::ios::app);
    if (!fout)
    {
        throw std::runtime_error("Failed to append result file: " + out_path.string());
    }
    if (existed)
    {
        fout << "\n\n";
    }
    fout << header << '\n';
}

void OutputManager::AppendMainResultLine(const std::string& line) const
{
    AppendResultLine(main_result_filename_, line);
}

void OutputManager::AppendResultLine(const std::string& filename, const std::string& line) const
{
    fs::path out_path = fs::path(result_dir_) / filename;
    std::ofstream fout(out_path, std::ios::app);
    if (!fout)
    {
        throw std::runtime_error("Failed to append result file: " + out_path.string());
    }
    fout << line << '\n';
}

}  // namespace gst

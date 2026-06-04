#include "output_manager.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gst
{

OutputManager::OutputManager(std::string result_dir, std::string debug_dir, std::string main_result_filename)
    : result_dir_(std::move(result_dir)),
      debug_dir_(std::move(debug_dir)),
      main_result_filename_(std::move(main_result_filename))
{
    // 构造时确保输出目录存在，避免后续写文件失败。
    fs::create_directories(result_dir_);
    fs::create_directories(debug_dir_);
}

void OutputManager::WriteMainResultFile(const std::vector<std::string>& lines) const
{
    fs::path out_path = fs::path(result_dir_) / main_result_filename_;
    std::ofstream fout(out_path, std::ios::app);
    if (!fout)
    {
        throw std::runtime_error("Failed to append result file: " + out_path.string());
    }
    for (const auto& line : lines)
    {
        fout << line << '\n';
    }
}

void OutputManager::AppendMainResultLine(const std::string& line) const
{
    AppendResultLine(main_result_filename_, line);
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

void OutputManager::WriteAnswerDetail(int query_index, const std::string& mode_name, const std::string& content) const
{
    fs::path out_path = fs::path(result_dir_) / ("query_" + std::to_string(query_index + 1) + "_" + mode_name + ".txt");
    const bool existed = fs::exists(out_path) && fs::file_size(out_path) > 0;
    std::ofstream fout(out_path, std::ios::app);
    if (!fout)
    {
        throw std::runtime_error("Failed to append answer detail file: " + out_path.string());
    }
    if (existed)
    {
        fout << "\n\n";
    }
    fout << content;
}

void OutputManager::WriteDebugFile(const std::string& filename, const std::string& content) const
{
    fs::path out_path = fs::path(debug_dir_) / filename;
    std::ofstream fout(out_path);
    if (!fout)
    {
        throw std::runtime_error("Failed to create debug file: " + out_path.string());
    }
    fout << content;
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

#ifndef GST_OUTPUT_MANAGER_H
#define GST_OUTPUT_MANAGER_H

#include <string>
#include <vector>

namespace gst
{

class OutputManager
{
public:
    OutputManager(std::string result_dir, std::string debug_dir,
                  std::string main_result_filename = "weights.txt");

    void WriteMainResultFile(const std::vector<std::string>& lines) const;
    void AppendMainResultLine(const std::string& line) const;
    const std::string& main_result_filename() const
    {
        return main_result_filename_;
    }
    void BeginResultRun(const std::string& filename, const std::string& header) const;
    void WriteAnswerDetail(int query_index, const std::string& mode_name, const std::string& content) const;
    void WriteDebugFile(const std::string& filename, const std::string& content) const;
    void AppendResultLine(const std::string& filename, const std::string& line) const;

    const std::string& result_dir() const
    {
        return result_dir_;
    }
    const std::string& debug_dir() const
    {
        return debug_dir_;
    }

private:
    std::string result_dir_;
    std::string debug_dir_;
    std::string main_result_filename_;
};

}  // namespace gst

#endif  // GST_OUTPUT_MANAGER_H

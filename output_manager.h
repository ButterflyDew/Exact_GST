#ifndef GST_OUTPUT_MANAGER_H
#define GST_OUTPUT_MANAGER_H

#include <string>

namespace gst
{

class OutputManager
{
public:
    OutputManager(std::string result_dir, std::string main_result_filename);

    void BeginResultRun(const std::string& filename, const std::string& header) const;
    void AppendMainResultLine(const std::string& line) const;
    void AppendResultLine(const std::string& filename, const std::string& line) const;

private:
    std::string result_dir_;
    std::string main_result_filename_;
};

}  // namespace gst

#endif  // GST_OUTPUT_MANAGER_H

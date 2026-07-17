#include <NowAnalysis/NowAnalysis.hpp>
#include <iostream>
#include <filesystem>

int main() {
    try {
        // Paths to python and the script
        // In a real app, these might be relative to the .exe or from config
        std::wstring pythonPath = L"python"; 
        std::wstring scriptPath = std::filesystem::absolute("create_now_analysis.py").wstring();

        std::cout << "Starting NowAnalysis daemon..." << std::endl;
        now_analysis::NowAnalysis na(pythonPath, scriptPath);
        std::cout << "Daemon started and healthy." << std::endl;

        now_analysis::AnalysisRequest req;
        req.data_dir = std::filesystem::absolute("data").string();
        req.type = "ap";
        req.open_viewer = false; // Don't pop up windows during test

        std::cout << "Posting analysis request for 'ap'..." << std::endl;
        auto resp = na.Analyze(req);

        std::cout << "Status: " << resp.status << std::endl;
        for (const auto& result : resp.results) {
            std::cout << " - Type: " << result.type << std::endl;
            if (result.generated) {
                std::cout << "   PDF: " << result.pdf.value_or("N/A") << std::endl;
            } else {
                std::cout << "   Error: " << result.generation_error.value_or("Unknown") << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

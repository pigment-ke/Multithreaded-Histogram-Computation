#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <string>
#include <iomanip> // For std::setw

const int HISTOGRAM_SIZE = 256; // Constant for histogram size

void fileToMemoryTransfer(const char* fileName, char** data, size_t& numOfBytes);

std::mutex hist_mutex; // Mutex to ensure thread-safe access
int global_histogram[HISTOGRAM_SIZE] = { 0 }; // Global histogram

// Function to process a chunk of the file and update the global histogram
void processChunkGlobal(char* data, size_t start, size_t end) {
    std::cout << "Processing Global Histogram Chunk: Start = " << start << ", End = " << end << std::endl;
    for (size_t i = start; i < end; ++i) {
        unsigned char byteValue = static_cast<unsigned char>(data[i]);
        std::lock_guard<std::mutex> lock(hist_mutex); // Locking access
        global_histogram[byteValue]++;
    }
}

// Function to process a chunk of the file and update the local histogram
void processChunkLocal(char* data, size_t start, size_t end, int* local_histogram) {
    std::cout << "Processing Local Histogram Chunk: Start = " << start << ", End = " << end << std::endl;

    for (size_t i = start; i < end; ++i) {
        unsigned char byteValue = static_cast<unsigned char>(data[i]);
        local_histogram[byteValue]++;
    }
}

// Function to merge local histograms into the global histogram
void mergeHistograms(int* local_histogram) {
    if (local_histogram) { // Check if local histogram is not null
        std::lock_guard<std::mutex> lock(hist_mutex); // Locking access
        for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
            global_histogram[i] += local_histogram[i];
        }
    }
}

// Function to create an output file name based on the input file name
std::string createOutputFileName(const std::string& inputFileName) {
    std::string baseName = inputFileName.substr(0, inputFileName.find_last_of('.'));
    if (baseName.empty()) baseName = inputFileName; // Handle case with no extension
    std::string fileName = baseName + "_output.txt";
    int counter = 1;
    while (std::ifstream(fileName)) {
        fileName = baseName + "_output_" + std::to_string(counter) + ".txt";
        counter++;
    }
    return fileName;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }

    char* fileData = nullptr;
    size_t fileSize = 0;

    // Read the file into memory
    fileToMemoryTransfer(argv[1], &fileData, fileSize);
    std::cout << "File read successfully. Size: " << fileSize << " bytes." << std::endl;

    unsigned int num_threads = std::thread::hardware_concurrency();
    size_t chunk_size = fileSize / num_threads;

    std::cout << "\nNumber of threads: " << num_threads << ", Chunk size: " << chunk_size << " bytes." << std::endl;

    // --- Solution 1: Global Histogram ---
    std::cout << "\nStarting calculation of Global Histogram..." << std::endl;

    // Measure time for global histogram
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;

    // Create and run threads for the global histogram
    for (unsigned int i = 0; i < num_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? fileSize : start + chunk_size;
        threads.push_back(std::thread(processChunkGlobal, fileData, start, end));
    }

    for (std::thread& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_global = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    std::cout << "Global Histogram calculation completed in " << duration_global << " microseconds.\n";

    // Print global histogram
    std::cout << "\nGlobal Histogram:" << std::endl;
    for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
        std::cout << std::setw(3) << i << ": " << global_histogram[i] << std::endl;
    }

    // --- Solution 2: Local Histograms ---
    // Reset global histogram for local histogram run
    std::fill(std::begin(global_histogram), std::end(global_histogram), 0);
    threads.clear();

    std::cout << "\nStarting calculation of Local Histograms..." << std::endl;

    // Measure time for local histograms
    start_time = std::chrono::high_resolution_clock::now();
    std::vector<int*> local_histograms(num_threads, nullptr);

    // Create and run threads for the local histograms
    for (unsigned int i = 0; i < num_threads; ++i) {
        local_histograms[i] = new int[HISTOGRAM_SIZE](); // Local histogram for each thread
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? fileSize : start + chunk_size;
        threads.push_back(std::thread(processChunkLocal, fileData, start, end, local_histograms[i]));
    }

    for (std::thread& t : threads) {
        t.join();
    }

    // Merge local histograms into the global histogram
    std::cout << "Merging local histograms into global histogram..." << std::endl;
    for (unsigned int i = 0; i < num_threads; ++i) {
        mergeHistograms(local_histograms[i]);
        delete[] local_histograms[i]; // Clean up
    }

    end_time = std::chrono::high_resolution_clock::now();
    auto duration_local = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    std::cout << "Local Histograms merged in " << duration_local << " microseconds.\n";

    // Print merged global histogram
    std::cout << "\nMerged Global Histogram:" << std::endl;
    for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
        std::cout << std::setw(3) << i << ": " << global_histogram[i] << std::endl;
    }

    // Efficiency analysis
    std::cout << "\nTime taken for Global Histogram approach: " << duration_global << " microseconds\n";
    std::cout << "Time taken for Local Histograms Merged approach: " << duration_local << " microseconds\n";

    std::string efficiency_result;
    if (duration_global < duration_local) {
        efficiency_result = "Global Histogram approach is more time-efficient.";
    }
    else if (duration_local < duration_global) {
        efficiency_result = "Local Histograms Merged approach is more time-efficient.";
    }
    else {
        efficiency_result = "Both approaches took the same amount of time.";
    }

    // Print efficiency result to console
    std::cout << "\n" << efficiency_result << std::endl;

    // Write output to a file
    std::string outputFileName = createOutputFileName(argv[1]);
    std::ofstream outFile(outputFileName);
    outFile << "Global Histogram Calculation Time: " << duration_global << " microseconds\n";
    outFile << "Global Histogram:\n";
    for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
        outFile << std::setw(3) << i << ": " << global_histogram[i] << std::endl;
    }

    outFile << "Local Histograms Merged Calculation Time: " << duration_local << " microseconds\n";
    outFile << "Local Histograms Merged into Global Histogram:\n";
    for (int i = 0; i < HISTOGRAM_SIZE; ++i) {
        outFile << std::setw(3) << i << ": " << global_histogram[i] << std::endl;
    }

    outFile << "Time taken for Global Histogram approach: " << duration_global << " microseconds\n";
    outFile << "Time taken for Local Histograms Merged approach: " << duration_local << " microseconds\n";
    outFile << "\n" << efficiency_result << "\n"; // Add efficiency result to the output file

    outFile.close();
    std::cout << "\nResults written to " << outputFileName << std::endl;

    delete[] fileData;
    return 0;
}

// File-to-memory transfer function
void fileToMemoryTransfer(const char* fileName, char** data, size_t& numOfBytes) {
    std::ifstream inFile(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile) { 
        std::cerr << "Cannot open " << fileName << std::endl;
        exit(1);
    }
    size_t size = inFile.tellg();
    char* buffer = new char[size];
    inFile.seekg(0, std::ios::beg);
    inFile.read(buffer, size);
    if (!inFile) {
        std::cerr << "Error reading the file." << std::endl;
        delete[] buffer;
        exit(1);
    }
    inFile.close();
    *data = buffer; 
    numOfBytes = size;
}

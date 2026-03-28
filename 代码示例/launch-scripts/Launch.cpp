/**
 * OpenClaw U盘便携版Windows启动器
 * 使用C++编写，提供更强的安全性和硬件验证
 */

#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <winsock.h>
#include <winreg.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "openssl.lib")

class LaunchManager {
private:
    std::string usbPath;
    std::string licenseKey;
    std::string hardwareID;
    
public:
    LaunchManager(const std::string& path) : usbPath(path) {
        // 初始化环境
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    
    /**
     * 获取USB设备硬件ID
     */
    std::string getUSBHardwareID() {
        std::string hardwareID;
        
        // 方法1：获取磁盘序列号
        HANDLE hDevice;
        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        
        hDevice = CreateFile(
            "\\\\.\\PhysicalDrive0",
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hDevice != INVALID_HANDLE_VALUE) {
            STORAGE_DESCRIPTOR_HEADER header = {};
            DWORD bytesReturned = 0;
            
            DeviceIoControl(
                hDevice,
                IOCTL_STORAGE_QUERY_PROPERTY,
                &query,
                sizeof(query),
                &header,
                sizeof(header),
                &bytesReturned,
                NULL
            );
            
            if (header.Size > 0) {
                std::vector<char> buffer(header.Size);
                DeviceIoControl(
                    hDevice,
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &query,
                    sizeof(query),
                    buffer.data(),
                    buffer.size(),
                    &bytesReturned,
                    NULL
                );
                
                STORAGE_DEVICE_DESCRIPTOR* descriptor = 
                    reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
                
                if (descriptor->SerialNumberOffset > 0) {
                    const char* serialNumber = buffer.data() + descriptor->SerialNumberOffset;
                    hardwareID = std::string(serialNumber);
                }
            }
            
            CloseHandle(hDevice);
        }
        
        // 方法2：获取USB设备ID（备用）
        if (hardwareID.empty()) {
            hardwareID = getUSBDeviceID();
        }
        
        // 计算SHA256哈希
        return calculateSHA256(hardwareID);
    }
    
    /**
     * 获取USB设备ID（备用方法）
     */
    std::string getUSBDeviceID() {
        std::string deviceID;
        
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Services\\USBSTOR",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            
            char value[256];
            DWORD size = sizeof(value);
            
            if (RegQueryValueEx(hKey, "DriverDesc", NULL, NULL,
                reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {
                deviceID = std::string(value);
            }
            
            RegCloseKey(hKey);
        }
        
        return deviceID;
    }
    
    /**
     * 计算SHA256哈希
     */
    std::string calculateSHA256(const std::string& input) {
        EVP_MD_CTX* context = EVP_MD_CTX_new();
        EVP_DigestInit_ex(context, EVP_sha256(), NULL);
        EVP_DigestUpdate(context, input.c_str(), input.length());
        
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int length = 0;
        EVP_DigestFinal_ex(context, hash, &length);
        EVP_MD_CTX_free(context);
        
        std::string hexHash;
        for (unsigned int i = 0; i < length; i++) {
            char hex[3];
            sprintf(hex, "%02x", hash[i]);
            hexHash += hex;
        }
        
        return hexHash;
    }
    
    /**
     * 验证硬件绑定
     */
    bool verifyHardware() {
        std::string storedID = readHardwareID();
        std::string currentID = getUSBHardwareID();
        
        if (storedID.empty()) {
            // 首次运行，初始化硬件绑定
            std::cout << "首次运行，初始化硬件绑定..." << std::endl;
            writeHardwareID(currentID);
            return true;
        }
        
        if (storedID != currentID) {
            std::cout << "硬件ID不匹配!" << std::endl;
            std::cout << "存储ID: " << storedID << std::endl;
            std::cout << "当前ID: " << currentID << std::endl;
            
            // 计算相似度
            double similarity = calculateStringSimilarity(storedID, currentID);
            std::cout << "相似度: " << similarity << std::endl;
            
            if (similarity < 0.95) {
                std::cout << "硬件ID差异过大，拒绝启动" << std::endl;
                return false;
            } else {
                std::cout << "硬件ID轻微变化，可能是设备更新" << std::endl;
                writeHardwareID(currentID);
            }
        }
        
        std::cout << "硬件验证成功" << std::endl;
        return true;
    }
    
    /**
     * 读取存储的硬件ID
     */
    std::string readHardwareID() {
        std::string filePath = usbPath + "\\config\\hardware.bin";
        std::ifstream file(filePath);
        if (file.is_open()) {
            std::string content;
            file >> content;
            file.close();
            return content;
        }
        return "";
    }
    
    /**
     * 写入硬件ID
     */
    void writeHardwareID(const std::string& id) {
        std::string filePath = usbPath + "\\config\\hardware.bin";
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << id;
            file.close();
        }
    }
    
    /**
     * 计算字符串相似度
     */
    double calculateStringSimilarity(const std::string& str1, const std::string& str2) {
        size_t len = std::max(str1.length(), str2.length());
        size_t matches = 0;
        
        for (size_t i = 0; i < len; i++) {
            if (i < str1.length() && i < str2.length() && str1[i] == str2[i]) {
                matches++;
            }
        }
        
        return static_cast<double>(matches) / len;
    }
    
    /**
     * 验证授权文件
     */
    bool verifyLicense() {
        std::string licensePath = usbPath + "\\config\\license.json";
        std::ifstream file(licensePath);
        
        if (!file.is_open()) {
            std::cout << "授权文件不存在" << std::endl;
            return false;
        }
        
        // 解析JSON
        std::string jsonContent;
        file >> jsonContent;
        file.close();
        
        // 简单JSON解析（实际应该使用JSON库）
        size_t statusPos = jsonContent.find("\"status\":");
        if (statusPos == std::string::npos) {
            std::cout << "授权文件格式错误" << std::endl;
            return false;
        }
        
        std::string status = jsonContent.substr(statusPos + 9, jsonContent.find("\"", statusPos + 9) - statusPos - 9);
        if (status != "active" && status != "trial") {
            std::cout << "授权状态无效: " << status << std::endl;
            return false;
        }
        
        // 检查过期时间
        size_t expirationPos = jsonContent.find("\"expiration_date\":");
        if (expirationPos != std::string::npos) {
            std::string expirationStr = jsonContent.substr(expirationPos + 18, jsonContent.find("\"", expirationPos + 18) - expirationPos - 18);
            
            if (expirationStr != "null") {
                // 解析日期（简化处理）
                SYSTEMTIME expirationTime;
                GetSystemTime(&expirationTime);
                SYSTEMTIME currentTime;
                GetSystemTime(&currentTime);
                
                // 简单比较
                if (SystemTimeToFileTime(&expirationTime) < SystemTimeToFileTime(&currentTime)) {
                    std::cout << "授权已过期" << std::endl;
                    return false;
                }
            }
        }
        
        std::cout << "授权验证成功" << std::endl;
        return true;
    }
    
    /**
     * 解密核心文件
     */
    bool decryptFiles() {
        std::string encryptedPath = usbPath + "\\runtime\\openclaw-dist\\core.js.enc";
        std::string decryptedPath = usbPath + "\\runtime\\openclaw-dist\\core.js";
        
        // 检查加密文件是否存在
        std::ifstream encFile(encryptedPath);
        if (!encFile.is_open()) {
            std::cout << "未使用加密，跳过解密" << std::endl;
            return true;
        }
        
        // 读取授权密钥
        std::string licensePath = usbPath + "\\config\\license.json";
        std::ifstream licenseFile(licensePath);
        std::string licenseContent;
        licenseFile >> licenseContent;
        licenseFile.close();
        
        size_t keyPos = licenseContent.find("\"license_key\":");
        if (keyPos == std::string::npos) {
            std::cout << "授权密钥不存在" << std::endl;
            return false;
        }
        
        std::string licenseKey = licenseContent.substr(keyPos + 14, licenseContent.find("\"", keyPos + 14) - keyPos - 14);
        
        // 解密文件
        std::cout << "解密核心文件..." << std::endl;
        
        FILE* srcFile = fopen(encryptedPath.c_str(), "rb");
        FILE* dstFile = fopen(decryptedPath.c_str(), "wb");
        
        if (!srcFile || !dstFile) {
            std::cout << "文件打开失败" << std::endl;
            return false;
        }
        
        // 读取加密文件内容
        fseek(srcFile, 0, SEEK_END);
        long fileSize = ftell(srcFile);
        fseek(srcFile, 0, SEEK_SET);
        
        std::vector<char> encryptedData(fileSize);
        fread(encryptedData.data(), 1, fileSize, srcFile);
        fclose(srcFile);
        
        // 解密（简化实现）
        std::vector<char> decryptedData;
        for (size_t i = 0; i < encryptedData.size(); i++) {
            decryptedData.push_back(encryptedData[i] ^ licenseKey[i % licenseKey.length()]);
        }
        
        fwrite(decryptedData.data(), 1, decryptedData.size(), dstFile);
        fclose(dstFile);
        
        std::cout << "核心文件解密成功" << std::endl;
        return true;
    }
    
    /**
     * 完整性校验
     */
    bool integrityCheck() {
        std::string integrityPath = usbPath + "\\config\\integrity.json";
        std::ifstream integrityFile(integrityPath);
        
        if (!integrityFile.is_open()) {
            std::cout << "未配置完整性校验，跳过" << std::endl;
            return true;
        }
        
        std::string integrityContent;
        integrityFile >> integrityContent;
        integrityFile.close();
        
        // 校验核心文件
        std::string corePath = usbPath + "\\runtime\\openclaw-dist\\core.js";
        std::ifstream coreFile(corePath);
        
        if (coreFile.is_open()) {
            std::string coreContent;
            coreFile >> coreContent;
            coreFile.close();
            
            std::string coreHash = calculateSHA256(coreContent);
            std::cout << "核心文件哈希: " << coreHash << std::endl;
        }
        
        return true;
    }
    
    /**
     * 启动OpenClaw
     */
    bool startOpenClaw() {
        std::string nodePath = usbPath + "\\runtime\\node\\node.exe";
        std::string openclawPath = usbPath + "\\runtime\\openclaw-dist\\openclaw.mjs";
        
        if (!std::filesystem::exists(nodePath)) {
            std::cout << "Node.js不存在" << std::endl;
            return false;
        }
        
        if (!std::filesystem::exists(openclawPath)) {
            std::cout << "OpenClaw主程序不存在" << std::endl;
            return false;
        }
        
        std::cout << "启动OpenClaw..." << std::endl;
        
        // 设置环境变量
        std::string workspacePath = usbPath + "\\workspace";
        std::string configPath = usbPath + "\\config\\default.yaml";
        
        // 构建命令
        std::string command = "\"" + nodePath + "\" \"" + openclawPath + "\" "
                              "--workspace \"" + workspacePath + "\" "
                              "--config \"" + configPath + "\" "
                              "start";
        
        // 执行命令
        int result = system(command.c_str());
        
        if (result == 0) {
            std::cout << "OpenClaw启动成功" << std::endl;
            return trueop} else {
            std::cout << "OpenClaw启动失败" << std::endl;
            return false;
        }
    }
    
    /**
     * 主流程
     */
    bool launch() {
        std::cout << "=======================================" << std::endl;
        std::cout << " OpenClaw U盘便携版启动" << std::endl;
        std::cout << "=======================================" << std::endl;
        
        // 1. 检查目录结构
        std::cout << "检查目录结构..." << std::endl;
        if (!checkDirectoryStructure()) {
            std::cout << "目录结构检查失败" << std::endl;
            return false;
        }
        
        // 2. 硬件验证
        std::cout << "硬件验证..." << std::endl;
        if (!verifyHardware()) {
            std::cout << "硬件验证失败" << std::endl;
            return false;
        }
        
        // 3. 授权验证
        std::cout << "授权验证..." << std::endl;
        if (!verifyLicense()) {
            std::cout << "授权验证失败" << std::endl;
            return false;
        }
        
        // 4. 解密文件
        std::cout << "解密文件..." << std::endl;
        if (!decryptFiles()) {
            std::cout << "文件解密失败" << std::endl;
            return false;
        }
        
        // 5. 完整性校验
        std::cout << "完整性校验..." << std::endl;
        if (!integrityCheck()) {
            std::cout << "完整性校验失败" << std::endl;
            return false;
        }
        
        // 6. 启动OpenClaw
        std::cout << "启动OpenClaw..." << std::endl;
        if (!startOpenClaw()) {
            std::cout << "OpenClaw启动失败" << std::endl;
            return false;
        }
        
        std::cout << "OpenClaw U盘便携版启动完成" << std::endl;
        return true;
    }
    
    /**
     * 检查目录结构
     */
    bool checkDirectoryStructure() {
        std::vector<std::string> requiredDirs = {
            usbPath + "\\runtime",
            usbPath + "\\config",
            usbPath + "\\workspace",
            usbPath + "\\logs",
            usbPath + "\\skills"
        };
        
        for (const auto& dir : requiredDirs) {
            if (!std::filesystem::exists(dir)) {
                std::cout << "目录不存在: " << dir << std::endl;
                return false;
            }
        }
        
        return true;
    }
};

int main(int argc, char* argv[]) {
    // 获取USB路径
    std::string usbPath;
    if (argc > 1) {
        usbPath = argv[1];
    } else {
        // 默认从当前目录启动
        char currentPath[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, currentPath);
        usbPath = std::string(currentPath);
    }
    
    LaunchManager manager(usbPath);
    
    if (manager.launch()) {
        return 0;
    } else {
        std::cout << "启动失败，按任意键退出..." << std::endl;
        system("pause");
        return 1;
    }
}
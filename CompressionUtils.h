#ifndef COMPRESSION_UTILS_H
#define COMPRESSION_UTILS_H

#include "FS.h"
#include "SD_MMC.h"
#include <vector>
#include <cstring>

#define COMPRESSION_BUFFER_SIZE 1024

class CompressionUtils {
private:
    fs::FS& _fs;
    String _basePath;
    
    struct FileInfo {
        String path;
        size_t size;
    };
    
    std::vector<FileInfo> _listAllFiles(const String& dirPath, std::vector<FileInfo>& fileList) {
        #ifdef useDebug
        DebugSerial.printf("🔍 Abriendo directorio: %s\n", dirPath.c_str());
        #endif
        
        File root = _fs.open(dirPath);
        if (!root) {
            #ifdef useDebug
            DebugSerial.printf("❌ No se pudo abrir: %s\n", dirPath.c_str());
            #endif
            return fileList;
        }
        
        if (!root.isDirectory()) {
            #ifdef useDebug
            DebugSerial.println("❌ No es un directorio");
            #endif
            root.close();
            return fileList;
        }
        
        #ifdef useDebug
        DebugSerial.println("📂 Listando archivos...");
        #endif
        
        File file = root.openNextFile();
        while (file) {
            String fileName = file.name();
            
            String filePath;
            if (dirPath.endsWith("/")) {
                filePath = dirPath + fileName;
            } else {
                filePath = dirPath + "/" + fileName;
            }
            
            #ifdef useDebug
            DebugSerial.printf("  📄 %s (dir=%d, size=%d)\n", filePath.c_str(), file.isDirectory(), file.size());
            #endif
            
            if (file.isDirectory()) {
                _listAllFiles(filePath, fileList);
            } else {
                FileInfo info;
                info.path = filePath;
                info.size = file.size();
                fileList.push_back(info);
            }
            file.close();
            file = root.openNextFile();
        }
        
        root.close();
        return fileList;
    }
    
    uint32_t _getFileCrc32(File* file, size_t len) {
        uint32_t crc = 0xFFFFFFFF;
        
        size_t originalPos = file->position();
        file->seek(0);
        
        uint8_t* buffer = (uint8_t*)malloc(512);
        if (!buffer) {
            file->seek(originalPos);
            return 0;
        }
        
        size_t totalRead = 0;
        while (totalRead < len) {
            size_t toRead = (len - totalRead) > 512 ? 512 : (len - totalRead);
            size_t read = file->read(buffer, toRead);
            if (read == 0) break;
            
            for (size_t i = 0; i < read; i++) {
                crc ^= buffer[i];
                for (uint8_t j = 0; j < 8; j++) {
                    crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
                }
            }
            totalRead += read;
        }
        
        free(buffer);
        file->seek(originalPos);
        
        return ~crc;
    }

public:
    CompressionUtils(fs::FS& fs, const String& basePath) : _fs(fs), _basePath(basePath) {}
    
    void setBasePath(const String& basePath) {
        _basePath = basePath;
        #ifdef useDebug
        DebugSerial.printf("📂 Directorio base establecido: %s\n", _basePath.c_str());
        #endif
    }
    
    String getBasePath() {
        return _basePath;
    }
    
    bool compressDirectory(const String& outputZipPath, bool deleteOriginal = false) {
        #ifdef useDebug
        DebugSerial.printf("📂 Directorio base: %s\n", _basePath.c_str());
        #endif
        
        std::vector<FileInfo> fileList;
        _listAllFiles(_basePath, fileList);
        
        #ifdef useDebug
        DebugSerial.printf("📄 Archivos encontrados: %d\n", fileList.size());
        #endif
        
        if (fileList.empty()) {
            #ifdef useDebug
            DebugSerial.println("❌ No hay archivos para comprimir");
            #endif
            return false;
        }
        
        #ifdef useDebug
        DebugSerial.printf("📦 Creando ZIP: %s\n", outputZipPath.c_str());
        #endif
        
        File zipFile = _fs.open(outputZipPath, FILE_WRITE);
        if (!zipFile) {
            #ifdef useDebug
            DebugSerial.println("❌ Error creando archivo ZIP");
            #endif
            return false;
        }
        
        uint16_t diskNumber = 0;
        uint16_t versionNeeded = 20;
        uint16_t compressionMethod = 0;
        
        std::vector<uint32_t> centralDirectoryOffsets;
        std::vector<uint32_t> fileCrcs;
        std::vector<size_t> fileSizes;
        std::vector<String> relativePaths;
        
        for (const auto& fileInfo : fileList) {
            File inputFile = _fs.open(fileInfo.path, FILE_READ);
            if (!inputFile) continue;
            
            size_t fileSize = inputFile.size();
            centralDirectoryOffsets.push_back(zipFile.position());
            
            String relativePath = fileInfo.path;
            if (relativePath.startsWith(_basePath)) {
                relativePath = relativePath.substring(_basePath.length());
            }
            if (relativePath.startsWith("/")) {
                relativePath = relativePath.substring(1);
            }
            
            uint32_t crc32 = _getFileCrc32(&inputFile, fileSize);
            
            fileCrcs.push_back(crc32);
            fileSizes.push_back(fileSize);
            relativePaths.push_back(relativePath);
            
            uint8_t header[30];
            header[0] = 0x50; header[1] = 0x4B; header[2] = 0x03; header[3] = 0x04;
            header[4] = 0x14; header[5] = 0x00;
            header[6] = 0x00; header[7] = 0x00;
            header[8] = 0x00; header[9] = 0x00;
            header[10] = 0x00; header[11] = 0x00;
            header[12] = 0x00; header[13] = 0x00;
            
            memcpy(header + 14, &crc32, 4);
            memcpy(header + 18, &fileSize, 4);
            memcpy(header + 22, &fileSize, 4);
            
            uint16_t filenameLen = relativePath.length();
            memcpy(header + 26, &filenameLen, 2);
            header[28] = 0; header[29] = 0;
            
            zipFile.write(header, 30);
            zipFile.write((const uint8_t*)relativePath.c_str(), filenameLen);
            
            uint8_t* fileBuffer = (uint8_t*)malloc(COMPRESSION_BUFFER_SIZE);
            if (fileBuffer) {
                size_t totalRead = 0;
                while (totalRead < fileSize) {
                    size_t toRead = (fileSize - totalRead) > COMPRESSION_BUFFER_SIZE ? COMPRESSION_BUFFER_SIZE : (fileSize - totalRead);
                    size_t read = inputFile.read(fileBuffer, toRead);
                    if (read == 0) break;
                    zipFile.write(fileBuffer, read);
                    totalRead += read;
                }
                free(fileBuffer);
            }
            
            inputFile.close();
            
            #ifdef useDebug
            DebugSerial.printf("✅ Comprimido: %s (%d bytes) CRC: 0x%08X\n", relativePath.c_str(), fileSize, crc32);
            #endif
        }
        
        size_t centralDirStart = zipFile.position();
        
        for (size_t i = 0; i < relativePaths.size(); i++) {
            uint32_t crc32 = fileCrcs[i];
            size_t fileSize = fileSizes[i];
            String& relativePath = relativePaths[i];
            uint16_t filenameLen = relativePath.length();
            
            uint8_t cdHeader[46];
            cdHeader[0] = 0x50; cdHeader[1] = 0x4B; cdHeader[2] = 0x01; cdHeader[3] = 0x02;
            cdHeader[4] = 0x14; cdHeader[5] = 0x00;
            cdHeader[6] = 0x14; cdHeader[7] = 0x00;
            cdHeader[8] = 0x00; cdHeader[9] = 0x00;
            cdHeader[10] = 0x00; cdHeader[11] = 0x00;
            cdHeader[12] = 0x00; cdHeader[13] = 0x00;
            cdHeader[14] = 0x00; cdHeader[15] = 0x00;
            cdHeader[16] = 0x00; cdHeader[17] = 0x00;
            
            memcpy(cdHeader + 18, &crc32, 4);
            memcpy(cdHeader + 22, &fileSize, 4);
            memcpy(cdHeader + 26, &fileSize, 4);
            memcpy(cdHeader + 28, &filenameLen, 2);
            
            cdHeader[30] = 0; cdHeader[31] = 0;
            cdHeader[32] = 0; cdHeader[33] = 0;
            cdHeader[34] = 0; cdHeader[35] = 0;
            cdHeader[36] = 0; cdHeader[37] = 0x20;
            
            memcpy(cdHeader + 38, &centralDirectoryOffsets[i], 4);
            
            zipFile.write(cdHeader, 46);
            zipFile.write((const uint8_t*)relativePath.c_str(), filenameLen);
        }
        
        size_t centralDirEnd = zipFile.position();
        
        uint8_t eocd[22];
        eocd[0] = 0x50; eocd[1] = 0x4B; eocd[2] = 0x05; eocd[3] = 0x06;
        eocd[4] = 0x00; eocd[5] = 0x00;
        eocd[6] = 0x00; eocd[7] = 0x00;
        eocd[8] = 0x00; eocd[9] = 0x00;
        
        uint16_t numEntries = fileList.size();
        memcpy(eocd + 10, &numEntries, 2);
        memcpy(eocd + 12, &numEntries, 2);
        
        uint32_t centralDirSize = centralDirEnd - centralDirStart;
        uint32_t centralDirOffset = centralDirStart;
        
        memcpy(eocd + 14, &centralDirSize, 4);
        memcpy(eocd + 18, &centralDirOffset, 4);
        
        eocd[20] = 0; eocd[21] = 0;
        
        zipFile.write(eocd, 22);
        zipFile.close();
        
        #ifdef useDebug
        DebugSerial.printf("✅ Archivo ZIP creado: %s (%d archivos)\n", 
                          outputZipPath.c_str(), fileList.size());
        DebugSerial.printf("📍 Central Dir Offset: %d\n", centralDirStart);
        DebugSerial.printf("📍 Central Dir Size: %d\n", centralDirEnd - centralDirStart);
        DebugSerial.printf("📍 Total archivos: %d\n", relativePaths.size());
        #endif
        
        if (deleteOriginal) {
            deleteDirectoryContents(_basePath);
        }
        
        return true;
    }
    
    bool deleteDirectoryContents(const String& dirPath) {
        return _deleteRecursive(dirPath, true);
    }
    
    bool _deleteRecursive(const String& dirPath, bool keepRoot) {
        File root = _fs.open(dirPath);
        if (!root) {
            #ifdef useDebug
            DebugSerial.printf("❌ Error abriendo directorio: %s\n", dirPath.c_str());
            #endif
            return false;
        }
        
        if (!root.isDirectory()) {
            root.close();
            return false;
        }
        
        bool success = true;
        File file = root.openNextFile();
        
        while (file) {
            String fileName = file.name();
            bool isDir = file.isDirectory();
            file.close();
            
            String fullPath = dirPath;
            if (!fullPath.endsWith("/")) fullPath += "/";
            fullPath += fileName;
            
            if (isDir) {
                if (!_deleteRecursive(fullPath, true)) {
                    success = false;
                }
                if (_fs.rmdir(fullPath)) {
                    #ifdef useDebug
                    DebugSerial.printf("🗑️ Eliminado directorio: %s\n", fullPath.c_str());
                    #endif
                } else {
                    #ifdef useDebug
                    DebugSerial.printf("❌ Error rmdir: %s\n", fullPath.c_str());
                    #endif
                    success = false;
                }
            } else {
                if (_fs.remove(fullPath)) {
                    #ifdef useDebug
                    DebugSerial.printf("🗑️ Eliminado archivo: %s\n", fullPath.c_str());
                    #endif
                } else {
                    #ifdef useDebug
                    DebugSerial.printf("❌ Error eliminando: %s\n", fullPath.c_str());
                    #endif
                    success = false;
                }
            }
            
            file = root.openNextFile();
        }
        
        root.close();
        
        if (!keepRoot) {
            if (_fs.rmdir(dirPath)) {
                #ifdef useDebug
                DebugSerial.printf("🗑️ Eliminado directorio raiz: %s\n", dirPath.c_str());
                #endif
            }
        }
        
        return success;
    }
    
    bool deleteAllInDirectory(const String& dirPath) {
        return _deleteRecursive(dirPath, false);
    }
    
    bool extractFileFromZip(const String& zipPath, const String& fileToExtract, 
                           uint8_t*& buffer, size_t& fileSize) {
        File zipFile = _fs.open(zipPath, FILE_READ);
        if (!zipFile) {
            #ifdef useDebug
            DebugSerial.println("❌ Error abriendo archivo ZIP");
            #endif
            return false;
        }
        
        uint8_t header[30];
        uint32_t signature;
        
        while (zipFile.available()) {
            size_t pos = zipFile.position();
            
            if (zipFile.read(header, 4) != 4) break;
            memcpy(&signature, header, 4);
            
            if (signature == 0x06064b50) {
                zipFile.close();
                #ifdef useDebug
                DebugSerial.println("❌ Archivo no encontrado en el ZIP");
                #endif
                return false;
            }
            
            if (signature != 0x04034b50) {
                zipFile.close();
                return false;
            }
            
            if (zipFile.read(header + 4, 26) != 26) break;
            
            uint16_t filenameLen, extraLen;
            memcpy(&filenameLen, header + 26, 2);
            memcpy(&extraLen, header + 28, 2);
            
            char* filename = (char*)malloc(filenameLen + 1);
            if (!filename) {
                zipFile.close();
                return false;
            }
            
            if (zipFile.read((uint8_t*)filename, filenameLen) != filenameLen) {
                free(filename);
                zipFile.close();
                return false;
            }
            filename[filenameLen] = '\0';
            
            memcpy(&fileSize, header + 18, 4);
            
            if (String(filename) == fileToExtract) {
                buffer = (uint8_t*)malloc(fileSize);
                if (buffer) {
                    size_t read = zipFile.read(buffer, fileSize);
                    free(filename);
                    zipFile.close();
                    
                    if (read == fileSize) {
                        #ifdef useDebug
                        DebugSerial.printf("✅ Archivo extraido: %s (%d bytes)\n", 
                                          fileToExtract.c_str(), fileSize);
                        #endif
                        return true;
                    }
                    free(buffer);
                    buffer = nullptr;
                }
            }
            
            free(filename);
            zipFile.seek(pos + 30 + filenameLen + extraLen + fileSize);
        }
        
        zipFile.close();
        
        #ifdef useDebug
        DebugSerial.printf("❌ Archivo no encontrado: %s\n", fileToExtract.c_str());
        #endif
        return false;
    }
    
    bool extractFileFromZipToFile(const String& zipPath, const String& fileToExtract,
                                  const String& outputPath) {
        uint8_t* buffer = nullptr;
        size_t fileSize = 0;
        
        if (!extractFileFromZip(zipPath, fileToExtract, buffer, fileSize)) {
            return false;
        }
        
        File outFile = _fs.open(outputPath, FILE_WRITE);
        if (!outFile) {
            free(buffer);
            #ifdef useDebug
            DebugSerial.println("❌ Error creando archivo de salida");
            #endif
            return false;
        }
        
        size_t written = outFile.write(buffer, fileSize);
        outFile.close();
        free(buffer);
        
        if (written == fileSize) {
            #ifdef useDebug
            DebugSerial.printf("✅ Archivo guardado en: %s\n", outputPath.c_str());
            #endif
            return true;
        }
        
        return false;
    }
    
    std::vector<String> listFilesInZip(const String& zipPath) {
        std::vector<String> files;
        
        File zipFile = _fs.open(zipPath, FILE_READ);
        if (!zipFile) {
            return files;
        }
        
        uint8_t header[30];
        uint32_t signature;
        
        while (zipFile.available()) {
            if (zipFile.read(header, 4) != 4) break;
            memcpy(&signature, header, 4);
            
            if (signature == 0x06054b50) break;
            
            if (signature != 0x04034b50) continue;
            
            if (zipFile.read(header + 4, 26) != 26) break;
            
            uint16_t filenameLen, extraLen;
            size_t fileSize;
            
            memcpy(&filenameLen, header + 26, 2);
            memcpy(&extraLen, header + 28, 2);
            memcpy(&fileSize, header + 18, 4);
            
            char* filename = (char*)malloc(filenameLen + 1);
            if (!filename) continue;
            
            if (zipFile.read((uint8_t*)filename, filenameLen) != filenameLen) {
                free(filename);
                continue;
            }
            filename[filenameLen] = '\0';
            
            files.push_back(String(filename));
            free(filename);
            
            size_t pos = zipFile.position();
            zipFile.seek(pos + fileSize + extraLen);
        }
        
        zipFile.close();
        return files;
    }
    
    bool fileExistsInZip(const String& zipPath, const String& filename) {
        std::vector<String> files = listFilesInZip(zipPath);
        for (const auto& f : files) {
            if (f == filename) return true;
        }
        return false;
    }
    
    void countContents(const String& dirPath, int& fileCount, int& dirCount, int64_t& totalSize) {
        fileCount = 0;
        dirCount = 0;
        totalSize = 0;
        _countRecursive(dirPath, fileCount, dirCount, totalSize);
    }
    
private:
    void _countRecursive(const String& dirPath, int& fileCount, int& dirCount, int64_t& totalSize) {
        File root = _fs.open(dirPath);
        if (!root || !root.isDirectory()) {
            if (root) root.close();
            return;
        }
        
        File file = root.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                dirCount++;
                String subPath = dirPath;
                if (!subPath.endsWith("/")) subPath += "/";
                subPath += file.name();
                _countRecursive(subPath, fileCount, dirCount, totalSize);
            } else {
                fileCount++;
                totalSize += file.size();
            }
            file.close();
            file = root.openNextFile();
        }
        root.close();
    }
    
public:
    std::vector<String> listZipFiles(const String& dirPath) {
        std::vector<String> zipFiles;
        
        File root = _fs.open(dirPath);
        if (!root || !root.isDirectory()) {
            if (root) root.close();
            return zipFiles;
        }
        
        File file = root.openNextFile();
        while (file) {
            String fileName = file.name();
            
            if (!file.isDirectory()) {
                if (fileName.endsWith(".zip") || fileName.endsWith(".ZIP")) {
                    String fullPath = dirPath;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += fileName;
                    zipFiles.push_back(fullPath);
                }
            }
            file.close();
            file = root.openNextFile();
        }
        root.close();
        
        return zipFiles;
    }
};

#endif

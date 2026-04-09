#define useDebug
#ifdef useDebug
#define DebugSerial Serial
#endif

#include "FS.h"
#include "SD_MMC.h"
#include "CompressionUtils.h"

#define eMMC_CLK 16
#define eMMC_CMD 15
#define eMMC_D0 39
#define eMMC_D1 40
#define eMMC_D2 38
#define eMMC_D3 41

#define INPUT_BUFFER_SIZE 128

CompressionUtils* compressUtils = nullptr;

bool boInicializa_eMMC_EMergencia(void){
  if (!SD_MMC.setPins(eMMC_CLK, eMMC_CMD, eMMC_D0, eMMC_D1, eMMC_D2, eMMC_D3)) {
    #ifdef useDebug
    DebugSerial.println("❌Error Iniciando PinOut Memoria eMMC!");
    #endif
    return false;
  }
  if (!SD_MMC.begin("/sdcard", false, true, 10000, 5)) {
    #ifdef useDebug
    DebugSerial.println("❌Error Iniciando Memoria eMMC!");
    #endif
    return false;
  }
  #ifdef useDebug
  DebugSerial.println("✅ Iniciando Memoria eMMC con Exito!");
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    DebugSerial.println("No SD_MMC card attached");
    return false;
  }
  DebugSerial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) DebugSerial.println("MMC");
  else if (cardType == CARD_SD) DebugSerial.println("SDSC");
  else if (cardType == CARD_SDHC) DebugSerial.println("SDHC");
  else DebugSerial.println("UNKNOWN");
  DebugSerial.printf("Card Size: %lld Bytes\n", SD_MMC.cardSize());
  DebugSerial.printf("Total: %lld MB\n", SD_MMC.totalBytes() / (1024 * 1024));
  DebugSerial.printf("Used: %lld MB\n", SD_MMC.usedBytes() / (1024 * 1024));
  #endif
  return true;
}

String readLine() {
  String input = "";
  unsigned long startTime = millis();
  
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n') {
        if (input.length() > 0) {
          DebugSerial.println();
          return input;
        }
      } else if (c == 127 || c == 8) {
        if (input.length() > 0) {
          input.remove(input.length() - 1);
          DebugSerial.print("\b \b");
        }
      } else if (c >= 32 && input.length() < INPUT_BUFFER_SIZE - 1) {
        input += c;
        DebugSerial.print(c);
      }
      startTime = millis();
    }
    
    if (input.length() > 0 && (millis() - startTime) > 30000) {
      DebugSerial.println("\n⏱️ Timeout");
      return "";
    }
    
    delay(1);
  }
}

void printPrompt(const char* text) {
  DebugSerial.print(text);
  DebugSerial.print(": ");
}

bool isNumericFile(const String& filename) {
  if (filename.length() == 0) return false;
  for (int i = 0; i < filename.length(); i++) {
    if (!isDigit(filename[i])) return false;
  }
  return filename.length() <= 8;
}

int getNumericValue(const String& filename) {
  return atoi(filename.c_str());
}

void getFileRange(const String& dirPath, int& firstNum, int& lastNum) {
  firstNum = -1;
  lastNum = -1;
  
  File root = SD_MMC.open(dirPath);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory() && isNumericFile(file.name())) {
      int num = getNumericValue(file.name());
      if (firstNum == -1 || num < firstNum) firstNum = num;
      if (lastNum == -1 || num > lastNum) lastNum = num;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

String generateZipName(const String& dirPath) {
  int firstNum, lastNum;
  getFileRange(dirPath, firstNum, lastNum);
  
  String dirName = dirPath;
  int lastSlash = dirName.lastIndexOf('/');
  if (lastSlash >= 0) {
    dirName = dirName.substring(lastSlash + 1);
  }
  
  char zipName[64];
  if (firstNum != -1 && lastNum != -1) {
    snprintf(zipName, sizeof(zipName), "%s_%08d-%08d.zip", dirName.c_str(), firstNum, lastNum);
  } else {
    snprintf(zipName, sizeof(zipName), "%s.zip", dirName.c_str());
  }
  
  String zipPath = dirPath;
  lastSlash = zipPath.lastIndexOf('/');
  if (lastSlash >= 0) {
    zipPath = zipPath.substring(0, lastSlash);
  }
  
  if (!zipPath.endsWith("/")) zipPath += "/";
  zipPath += zipName;
  
  return zipPath;
}

void setup() {
  DebugSerial.begin(115200);
  delay(1000);
  
  DebugSerial.println("=== ESP32-S3 eMMC ZIP Compression ===");
  
  if (boInicializa_eMMC_EMergencia()) {
    compressUtils = new CompressionUtils(SD_MMC, "/");
    DebugSerial.println("✅ eMMC Iniciada correctamente!");
  }
  
  DebugSerial.println();
  printMenu();
}

void printMenu() {
  DebugSerial.println();
  DebugSerial.println("=== MENU PRINCIPAL ===");
  DebugSerial.println("1 - Comprimir directorio (conserva original)");
  DebugSerial.println("2 - Comprimir y eliminar original");
  DebugSerial.println("3 - Eliminar contenido del directorio");
  DebugSerial.println("4 - Eliminar directorio completo");
  DebugSerial.println("5 - Listar archivos en ZIP");
  DebugSerial.println("6 - Leer archivo especifico del ZIP");
  DebugSerial.println("7 - Extraer archivo del ZIP a eMMC");
  DebugSerial.println("8 - Contar archivos y directorios");
  DebugSerial.println("9 - Listar archivos ZIP en directorio");
  DebugSerial.println("h - Mostrar ayuda");
  DebugSerial.println();
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    
    if (cmd == '\r' || cmd == '\n') return;
    
    if (cmd == 'h' || cmd == 'H') {
      printMenu();
      return;
    }
    
    if (!compressUtils) {
      DebugSerial.println("❌ eMMC no inicializada!");
      return;
    }
    
    String inputDir;
    String zipPath;
    String fileToRead;
    String fileToExtract;
    String outputPath;
    uint32_t timeProcess;
    
    switch (cmd) {
      case '1':
      case '2': {
        DebugSerial.println();
        DebugSerial.println("=== COMPRIMIR DIRECTORIO ===");
        printPrompt("Directorio a comprimir");
        inputDir = readLine();
        if (inputDir.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!inputDir.startsWith("/")) inputDir = "/" + inputDir;
        
        if (!SD_MMC.exists(inputDir)) {
          DebugSerial.println("❌ El directorio no existe");
          break;
        }
        
        zipPath = generateZipName(inputDir);
        DebugSerial.printf("📦 Archivo ZIP: %s\n", zipPath.c_str());
        
        compressUtils->setBasePath(inputDir);
        
        DebugSerial.println("⏳ Comprimiendo...");
        bool deleteOriginal = (cmd == '2');
        timeProcess = millis();
        bool success = compressUtils->compressDirectory(zipPath, deleteOriginal);
        DebugSerial.printf("🟡⏱️ El Tiempo de Compresion es = %d mSeg\n", millis() - timeProcess);
        if (success) {
          DebugSerial.println("✅ Compresion completada!");
        } else {
          DebugSerial.println("❌ Error en compresion");
        }
        
        break;
      }
      
      case '3': {
        DebugSerial.println();
        DebugSerial.println("=== ELIMINAR CONTENIDO DEL DIRECTORIO ===");
        printPrompt("Directorio a limpiar");
        inputDir = readLine();
        if (inputDir.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!inputDir.startsWith("/")) inputDir = "/" + inputDir;
        
        if (!SD_MMC.exists(inputDir)) {
          DebugSerial.println("❌ El directorio no existe");
          break;
        }
        
        DebugSerial.println("⚠️ Confirma eliminar todo el contenido? (s/n)");
        printPrompt("Respuesta");
        String confirm = readLine();
        
        if (confirm != "s" && confirm != "S") {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        DebugSerial.println("⏳ Eliminando...");
        timeProcess = millis();
        bool success = compressUtils->deleteDirectoryContents(inputDir);
        DebugSerial.printf("🟡⏱️ El Tiempo de Eliminar es = %d mSeg\n", millis() - timeProcess);
        if (success) {
          DebugSerial.println("✅ Contenido eliminado!");
        } else {
          DebugSerial.println("❌ Error al eliminar");
        }
        break;
      }
      
      case '4': {
        DebugSerial.println();
        DebugSerial.println("=== ELIMINAR DIRECTORIO COMPLETO ===");
        printPrompt("Directorio a eliminar");
        inputDir = readLine();
        if (inputDir.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!inputDir.startsWith("/")) inputDir = "/" + inputDir;
        
        if (!SD_MMC.exists(inputDir)) {
          DebugSerial.println("❌ El directorio no existe");
          break;
        }
        
        DebugSerial.println("⚠️ Confirma eliminar el directorio y todo su contenido? (s/n)");
        printPrompt("Respuesta");
        String confirm = readLine();
        
        if (confirm != "s" && confirm != "S") {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        DebugSerial.println("⏳ Eliminando...");
        timeProcess = millis();
        bool success = compressUtils->deleteAllInDirectory(inputDir);
        DebugSerial.printf("🟡⏱️ El Tiempo de Eliminar Todo es = %d mSeg\n", millis() - timeProcess);
        if (success) {
          DebugSerial.println("✅ Directorio eliminado!");
        } else {
          DebugSerial.println("❌ Error al eliminar");
        }
        break;
      }
      
      case '5': {
        DebugSerial.println();
        DebugSerial.println("➖➖ LISTAR ARCHIVOS EN ZIP ➖➖");
        printPrompt("Archivo ZIP (con ruta)");
        zipPath = readLine();
        if (zipPath.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!zipPath.startsWith("/")) zipPath = "/" + zipPath;
        
        if (!SD_MMC.exists(zipPath)) {
          DebugSerial.println("❌ El archivo ZIP no existe");
          break;
        }
        
        DebugSerial.println("⏳ Listando archivos...");
        timeProcess = millis();
        std::vector<String> files = compressUtils->listFilesInZip(zipPath);
        DebugSerial.printf("🟡⏱️ El Tiempo de Buscar es = %d mSeg\n", millis() - timeProcess);
        DebugSerial.printf("\nArchivos en ZIP (%d):\n", files.size());
        for (size_t i = 0; i < files.size(); i++) {
          DebugSerial.printf("  %d - %s\n", i + 1, files[i].c_str());
        }
        break;
      }
      
      case '6': {
        DebugSerial.println();
        DebugSerial.println("=== LEER ARCHIVO DEL ZIP ===");
        printPrompt("Archivo ZIP (con ruta)");
        zipPath = readLine();
        if (zipPath.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!zipPath.startsWith("/")) zipPath = "/" + zipPath;
        
        if (!SD_MMC.exists(zipPath)) {
          DebugSerial.println("❌ El archivo ZIP no existe");
          break;
        }
        
        printPrompt("Archivo a leer dentro del ZIP");
        fileToRead = readLine();
        if (fileToRead.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        DebugSerial.println("⏳ Leyendo archivo...");
        
        uint8_t* buffer = nullptr;
        size_t fileSize = 0;
        timeProcess = millis();
        bool success = compressUtils->extractFileFromZip(zipPath, fileToRead, buffer, fileSize);
        DebugSerial.printf("🟡⏱️ El Tiempo de Lectura del Archivo es = %d mSeg\n", millis() - timeProcess);
        if (success && buffer) {
          DebugSerial.printf("\n✅ Archivo '%s' (%d bytes):\n", fileToRead.c_str(), fileSize);
          DebugSerial.write(buffer, fileSize);
          DebugSerial.println();
          free(buffer);
        } else {
          DebugSerial.println("❌ Archivo no encontrado");
        }
        break;
      }
      
      case '7': {
        DebugSerial.println();
        DebugSerial.println("=== EXTRAER ARCHIVO A eMMC ===");
        printPrompt("Archivo ZIP (con ruta)");
        zipPath = readLine();
        if (zipPath.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!zipPath.startsWith("/")) zipPath = "/" + zipPath;
        
        if (!SD_MMC.exists(zipPath)) {
          DebugSerial.println("❌ El archivo ZIP no existe");
          break;
        }
        
        printPrompt("Archivo a extraer dentro del ZIP");
        fileToExtract = readLine();
        if (fileToExtract.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        printPrompt("Ruta de destino");
        outputPath = readLine();
        if (outputPath.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!outputPath.startsWith("/")) outputPath = "/" + outputPath;
        
        DebugSerial.println("⏳ Extrayendo...");
        timeProcess = millis();
        bool success = compressUtils->extractFileFromZipToFile(zipPath, fileToExtract, outputPath);
        DebugSerial.printf("🟡⏱️ El Tiempo de Extraer del Archivo es = %d mSeg\n", millis() - timeProcess);
        if (success) {
          DebugSerial.printf("✅ Archivo extraido a: %s\n", outputPath.c_str());
        } else {
          DebugSerial.println("❌ Error al extraer");
        }
        break;
      }
      
      case '8': {
        DebugSerial.println();
        DebugSerial.println("=== CONTAR ARCHIVOS Y DIRECTORIOS ===");
        printPrompt("Directorio a analizar");
        inputDir = readLine();
        if (inputDir.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!inputDir.startsWith("/")) inputDir = "/" + inputDir;
        
        if (!SD_MMC.exists(inputDir)) {
          DebugSerial.println("❌ El directorio no existe");
          break;
        }
        
        int countFiles = 0;
        int countDirs = 0;
        int64_t totalSize = 0;
        DebugSerial.println("🟡 Se Procedera a Contar los Archivos y Directorios");
        timeProcess = millis();
        compressUtils->countContents(inputDir, countFiles, countDirs, totalSize);
        DebugSerial.printf("🟡⏱️ El Tiempo de Conteo de Archivos/Directorio es = %d mSeg\n", millis() - timeProcess);
        DebugSerial.println();
        DebugSerial.println("📊 RESULTADOS:");
        DebugSerial.printf("  📁 Directorios: %d\n", countDirs);
        DebugSerial.printf("  📄 Archivos: %d\n", countFiles);
        DebugSerial.printf("  📦 Tamanio total: %lld bytes\n", totalSize);
        DebugSerial.printf("  💾 Tamanio total: %.2f KB\n", totalSize / 1024.0);
        DebugSerial.printf("  💾 Tamanio total: %.2f MB\n", totalSize / (1024.0 * 1024.0));
        break;
      }
      
      case '9': {
        DebugSerial.println();
        DebugSerial.println("=== LISTAR ARCHIVOS ZIP ===");
        printPrompt("Directorio a analizar");
        inputDir = readLine();
        if (inputDir.length() == 0) {
          DebugSerial.println("❌ Operacion cancelada");
          break;
        }
        
        if (!inputDir.startsWith("/")) inputDir = "/" + inputDir;
        
        if (!SD_MMC.exists(inputDir)) {
          DebugSerial.println("❌ El directorio no existe");
          break;
        }
        timeProcess = millis();
        std::vector<String> zipFiles = compressUtils->listZipFiles(inputDir);
        DebugSerial.printf("🟡⏱️ El Tiempo de Buscar todos los .zip es = %d mSeg\n", millis() - timeProcess);
        DebugSerial.println();
        if (zipFiles.size() == 0) {
          DebugSerial.println("No se encontraron archivos ZIP");
        } else {
          DebugSerial.printf("Archivos ZIP encontrados (%d):\n", zipFiles.size());
          for (size_t i = 0; i < zipFiles.size(); i++) {
            DebugSerial.printf("  %d - %s\n", i + 1, zipFiles[i].c_str());
          }
        }
        break;
      }
      
      default:
        if (cmd != ' ' && cmd != '\r' && cmd != '\n') {
          DebugSerial.printf("❌ Comando '%c' no reconocido\n", cmd);
          DebugSerial.println("Escribe 'h' para ver los comandos disponibles");
        }
        break;
    }
    
    delay(100);
  }
}

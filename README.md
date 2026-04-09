# ESP32-S3 eMMC ZIP Compression

Biblioteca y programa para comprimir directorios en archivos ZIP en una eMMC conectada a un ESP32-S3.

## Hardware

- **ESP32-S3**
- **eMMC** conectada por 4 bits de datos (SDMMC)

### Conexiones eMMC

| eMMC | ESP32-S3 |
|------|----------|
| CLK  | GPIO 16  |
| CMD  | GPIO 15  |
| D0   | GPIO 39  |
| D1   | GPIO 40  |
| D2   | GPIO 38  |
| D3   | GPIO 41  |

## Librerías Requeridas

```cpp
#include "FS.h"
#include "SD_MMC.h"
#include "CompressionUtils.h"
```

## Funcionalidades

1. **Comprimir directorio** - Genera archivo ZIP con nombre automático basado en rango de archivos
2. **Comprimir y eliminar original** - Comprime y borra los archivos originales
3. **Eliminar contenido de directorio** - Borra todos los archivos y subdirectorios
4. **Eliminar directorio completo** - Borra el directorio y todo su contenido
5. **Listar archivos en ZIP** - Muestra el contenido de un archivo ZIP
6. **Leer archivo del ZIP** - Extrae y muestra el contenido de un archivo específico
7. **Extraer archivo a eMMC** - Extrae un archivo del ZIP a una ubicación específica
8. **Contar archivos y directorios** - Muestra estadísticas de un directorio
9. **Listar archivos ZIP** - Lista todos los archivos .zip en un directorio

## Uso

### Inicialización

```cpp
#include "CompressionUtils.h"

CompressionUtils* compressUtils = nullptr;

// Inicializar eMMC primero
if (!SD_MMC.setPins(CLK, CMD, D0, D1, D2, D3)) return false;
if (!SD_MMC.begin("/sdcard", false, true, 10000, 5)) return false;

// Crear objeto CompressionUtils
compressUtils = new CompressionUtils(SD_MMC, "/");
```

### Menú Serial

Conecte el Serial Monitor a 115200 baudios:

```
=== MENU PRINCIPAL ===
1 - Comprimir directorio (conserva original)
2 - Comprimir y eliminar original
3 - Eliminar contenido del directorio
4 - Eliminar directorio completo
5 - Listar archivos en ZIP
6 - Leer archivo especifico del ZIP
7 - Extraer archivo del ZIP a eMMC
8 - Contar archivos y directorios
9 - Listar archivos ZIP en directorio
```

### API

```cpp
// Establecer directorio base
compressUtils->setBasePath("/Seniat/DNF");

// Comprimir directorio
bool success = compressUtils->compressDirectory("/Seniat/DNF.zip", false);

// Eliminar contenido de directorio
bool success = compressUtils->deleteDirectoryContents("/Seniat/DNF");

// Eliminar directorio completo
bool success = compressUtils->deleteAllInDirectory("/Seniat/DNF");

// Extraer archivo del ZIP a memoria
uint8_t* buffer = nullptr;
size_t fileSize = 0;
compressUtils->extractFileFromZip("/Seniat/DNF.zip", "carpeta/archivo", buffer, fileSize);

// Extraer archivo del ZIP a archivo
compressUtils->extractFileFromZipToFile("/Seniat/DNF.zip", "carpeta/archivo", "/Seniat/temp/archivo");

// Listar contenido del ZIP
std::vector<String> files = compressUtils->listFilesInZip("/Seniat/DNF.zip");

// Contar archivos y directorios
int files, dirs;
int64_t totalSize;
compressUtils->countContents("/Seniat/DNF", files, dirs, totalSize);

// Listar archivos ZIP en directorio
std::vector<String> zipFiles = compressUtils->listZipFiles("/Seniat");
```

## Formato de Nombre ZIP

El nombre del archivo ZIP se genera automáticamente basado en los archivos numéricos:

```
Directorio: /Seniat/DNF
Archivos: 00000001, 00000002, ..., 00042345

ZIP: DNF_00000001-00042345.zip
```

## Formato de Archivo ZIP

- Compresión: STORED (sin comprimir, para reducir uso de RAM)
- Compatible con: WinRAR, 7-Zip, Windows Explorer
- Estructura: Soporta subdirectorios

## Autor

## Licencia

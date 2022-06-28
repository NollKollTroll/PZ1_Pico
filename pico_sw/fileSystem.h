#ifndef FILESYSTEM_H
#define FILESYSTEM_H

enum class FILE_CMD 
{
    OPEN_R,
    OPEN_W,
    CLOSE,
    READ,
    WRITE,
    DIR,
    DIR_NEXT
};
enum class FILE_STATE 
{
    OPEN_R,
    OPEN_W,
    OPENED_R,
    OPENED_W,
    CLOSED,
    DIR
};
enum class FILE_STATUS 
{
    OK,
    NOK
};

File fileData;
Dir dir;
String dirData;
uint8_t dirDataPtr = 0;

char fileName[256];
uint8_t fileNameIndex;
FILE_STATUS fileStatus;
FILE_STATE fileState;

void fsInit()
{
    SPI.setRX(SPI_RX);
    SPI.setTX(SPI_TX);
    SPI.setSCK(SPI_CLK);
    SPI.setCS(SD_CS_N);
    digitalWrite(SD_CS_N, LOW);    
    if (!SDFS.begin()) 
    {
        Serial.println("\n*** FS open NOK!");
    }
    else
    {
        Serial.println("\n*** FS open OK");
    }
    fileStatus = FILE_STATUS::NOK;
    fileState = FILE_STATE::CLOSED;
    fileNameIndex = 0;
}

static inline void fsCmdWrite(uint8_t cpuData)    
{
    if ((FILE_CMD)cpuData == FILE_CMD::OPEN_R)
    {
        fileStatus = FILE_STATUS::OK;
        fileState = FILE_STATE::OPEN_R;
        fileNameIndex = 0;
    }
    else if ((FILE_CMD)cpuData == FILE_CMD::OPEN_W) 
    {
        fileStatus = FILE_STATUS::OK;
        fileState = FILE_STATE::OPEN_W;
        fileNameIndex = 0;
    }
    else if ((FILE_CMD)cpuData == FILE_CMD::CLOSE) 
    {
        fileData.close();
        fileStatus = FILE_STATUS::OK;
        fileState = FILE_STATE::CLOSED;
    }
    else if ((FILE_CMD)cpuData == FILE_CMD::DIR) 
    {        
        if (SDFS.exists("/")) 
        {
            dir = SDFS.openDir("/");
            fileStatus = FILE_STATUS::OK;
            fileState = FILE_STATE::DIR;
        }
        else 
        {
            fileStatus = FILE_STATUS::NOK;
            fileState = FILE_STATE::CLOSED;
        }
    }
    else if ((FILE_CMD)cpuData == FILE_CMD::DIR_NEXT) 
    {
        if (dir.next())
        {
            if (dir.isDirectory())
            {
                dirData = String('[') + dir.fileName() + String(']');
            }
            else
            {
                dirData = dir.fileName() + String(" @") + String((long unsigned int)dir.fileSize());
            }
            dirDataPtr = 0;
        }
        else
        {
            fileStatus = FILE_STATUS::NOK;
            fileState = FILE_STATE::CLOSED;
        }
    }
}

static inline uint8_t fsDataRead()
{
    uint8_t cpuData;
    if (fileState == FILE_STATE::OPENED_R) 
    {
        if (fileData.available()) 
        {
            fileStatus = FILE_STATUS::OK;
            cpuData = fileData.read();
        }
        else 
        {
            fileStatus = FILE_STATUS::NOK;     
            cpuData = 0;
        }
    }       
    else if (fileState == FILE_STATE::DIR) 
    {
        char retChar = dirData[dirDataPtr];
        if (retChar != 0) 
        {
            dirDataPtr++;
        }
        cpuData = retChar;
    }
    else 
    {
        fileStatus = FILE_STATUS::NOK;
        cpuData = 0;
    }
    return cpuData;
}

static inline void fsDataWrite(uint8_t cpuData)
{
    if(fileState == FILE_STATE::OPENED_W) 
    {
        fileData.write(cpuData);
    }
    else if ((fileState == FILE_STATE::OPEN_R) || 
             (fileState == FILE_STATE::OPEN_W)) 
    {
        if (cpuData == 0) { // if end of string 0-termination
            fileName[fileNameIndex] = 0;
            if (fileState == FILE_STATE::OPEN_W) 
            {
                fileData = SDFS.open(fileName, "r+");
                if (fileData) 
                {
                    fileStatus = FILE_STATUS::OK;
                    fileState = FILE_STATE::OPENED_W;
                }
                else 
                {
                    fileData.close();
                    fileStatus = FILE_STATUS::NOK;
                    fileState = FILE_STATE::CLOSED;
                }
            }                            
            else if (fileState == FILE_STATE::OPEN_R) 
            {
                fileData = SDFS.open(fileName, FILE_READ);
                if (fileData) 
                {
                    fileStatus = FILE_STATUS::OK;
                    fileState = FILE_STATE::OPENED_R;
                }
                else 
                {
                    fileData.close();
                    fileStatus = FILE_STATUS::NOK;
                    fileState = FILE_STATE::CLOSED;
                }
            }                            
        }
        else if (fileNameIndex < (sizeof(fileName) - 1)) 
        { // enter filename string
            fileName[fileNameIndex] = cpuData;
            fileNameIndex++;
        }
    }
}

#endif //FILESYSTEM_H

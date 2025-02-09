#pragma once

#include "GAFHeader.h"

NS_GAF_BEGIN

class GAFFile
{
private:
    unsigned char*        m_data;
    unsigned int          m_dataPosition;
    unsigned long         m_dataLen;
    GAFHeader             m_header;
private:
    ax::Data             _getData(const std::string& filename);
    bool                 _processOpen();
protected:
    void                 _readHeaderBegin(GAFHeader&);
public:
    GAFFile();
    ~GAFFile();

    unsigned char        read1Byte();
    unsigned short       read2Bytes();
    unsigned int         read4Bytes();
    unsigned long long   read8Bytes();
    float                readFloat();
    double               readDouble();

    bool                 isEOF() const;

    size_t               readString(std::string* dst); // function reads lenght prefixed string
    void                 readBytes(void* dst, unsigned int len);

    void                 close();

    // TODO: Provide error codes
    bool                 open(const std::string& filename);
    bool                 open(const unsigned char* data, size_t len);

    bool                 isOpened() const;

    const GAFHeader&     getHeader() const;
    GAFHeader&           getHeader();

    unsigned int         getPosition() const;
    void                 rewind(unsigned int newPos);
};

NS_GAF_END
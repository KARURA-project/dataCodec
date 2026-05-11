#ifndef _DATA_CODEC_H_
#define _DATA_CODEC_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifndef MAX_DATA_NUM
#define MAX_DATA_NUM 10
#endif
#define MAX_BIT (MAX_DATA_NUM * 32)
#define MAX_BYTE ((MAX_BIT + 6) / 7 + 4)
#ifndef MAX_PORT_NUM
#define MAX_PORT_NUM 10
#endif
#ifndef BUFFER_SIZE
#define BUFFER_SIZE 255
#endif
#define DATA_LENGTH_LIMIT (127 * 7)

enum class TYPE {
  BOOL, UINT, INT, FLOAT
};

enum class ERROR {
  OK,
  UNEDITABLE,
  INVALID_PARAM,
  OVERFLOW,
  NO_DATA,
  INVALID_HEADER,
  INCOMPLETE_PACKET,
  INVALID_LENGTH,
  INVALID_CHECKSUM,
  UNSET_PACKET,
  INVALID_TYPE,
  INVALID_SIZE
};

typedef struct {
  bool isActive;
  void *ptr;
  TYPE type;
  struct {
    uint8_t raw, encoded;
  } size;
  struct {
    uint32_t bits, encoded;
  } data;
} dataInfo_t;

typedef struct {
  bool isEditable;
  uint8_t id;
  dataInfo_t info[MAX_DATA_NUM];
  uint16_t bitLength;
} dataSet_t;

typedef struct {
  uint8_t id;
  uint8_t length;
  uint8_t data[MAX_BYTE];
} dataPacket_t;

class dataCodecBase {
protected:
  template <typename T>
  TYPE _identifyType();
  template <typename T>
  uint8_t _decideSize(TYPE type_, uint8_t size_);
};

#ifndef DATA_DECODER_ONLY
class dataEncoder : public dataCodecBase {
public:
  dataEncoder(uint8_t id_);
  template <typename T>
  ERROR append(uint8_t ord_, T *dataPtr_, uint8_t size_ = 32);
  ERROR set();
  ERROR encode();
  dataPacket_t getPacket();
private:
  dataSet_t _dataSet = {};
  dataPacket_t _dataPacket = {};
  bool _binary[MAX_BIT];
  template <typename T>
  uint32_t _getBitsData(uint8_t ord_);
  uint32_t _getEncodedData(uint8_t ord_);
  void _generateBinary();
};
#endif

#ifndef DATA_ENCODER_ONLY
class dataDecoder : public dataCodecBase {
public:
  dataDecoder(uint8_t id_ = 0);
  template <typename T>
  ERROR append(uint8_t ord_, T *dataPtr_, uint8_t size_ = 32);
  ERROR set();
  ERROR appendToBuffer(uint8_t data);
  ERROR decode();
  uint8_t getCurrentId();
private:
  dataSet_t _dataSet = {};
  dataPacket_t _dataPacket = {};  
  uint8_t _buffer[BUFFER_SIZE] = {};
  uint16_t _bufferIndex; 
  bool _binary[MAX_BIT];
  uint8_t _currentId;
  void _shiftLeftBuffer(uint16_t step_);
  ERROR _extractData();
  ERROR _generateBinary();
  ERROR _getEncodedData();
  ERROR _getBitsData();
  ERROR _restoreData();
};
#endif

#endif

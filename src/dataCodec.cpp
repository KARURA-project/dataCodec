#include "dataCodec.h"

template <> TYPE dataCodecBase::_identifyType<bool>() {return TYPE::BOOL;};
template <> TYPE dataCodecBase::_identifyType<uint8_t>() {return TYPE::UINT;};
template <> TYPE dataCodecBase::_identifyType<uint16_t>() {return TYPE::UINT;};
template <> TYPE dataCodecBase::_identifyType<uint32_t>() {return TYPE::UINT;};
template <> TYPE dataCodecBase::_identifyType<uint64_t>() {return TYPE::UINT;};
template <> TYPE dataCodecBase::_identifyType<int8_t>() {return TYPE::INT;};
template <> TYPE dataCodecBase::_identifyType<int16_t>() {return TYPE::INT;};
template <> TYPE dataCodecBase::_identifyType<int32_t>() {return TYPE::INT;};
template <> TYPE dataCodecBase::_identifyType<int64_t>() {return TYPE::INT;};
template <> TYPE dataCodecBase::_identifyType<float>() {return TYPE::FLOAT;};
template <> TYPE dataCodecBase::_identifyType<double>() {return TYPE::FLOAT;};

template <typename T>
uint8_t dataCodecBase::_decideSize(TYPE type_, uint8_t size_) {
  if (type_ == TYPE::BOOL) return 1;
  if (type_ == TYPE::FLOAT) return 32;
  return size_;
}
template uint8_t dataCodecBase::_decideSize<bool>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<uint8_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<uint16_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<uint32_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<uint64_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<int8_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<int16_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<int32_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<int64_t>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<float>(TYPE type_, uint8_t size_);
template uint8_t dataCodecBase::_decideSize<double>(TYPE type_, uint8_t size_);

#ifndef DATA_DECODER_ONLY
dataEncoder::dataEncoder(uint8_t id_){
  _dataSet.isEditable = true;
  _dataSet.id = id_;
  _dataPacket.id = id_;
};

template <typename T>
ERROR dataEncoder::append(uint8_t ord_, T *dataPtr_, uint8_t size_) {
  if (!_dataSet.isEditable) return ERROR::UNEDITABLE;
  if (MAX_DATA_NUM <= ord_) return ERROR::INVALID_PARAM;
  if (_dataSet.info[ord_].isActive) return ERROR::INVALID_PARAM;
  if (dataPtr_ == nullptr) return ERROR::INVALID_PARAM;
  if ((size_ == 0) || (32 < size_)) return ERROR::INVALID_PARAM;
  _dataSet.info[ord_].isActive = true;
  _dataSet.info[ord_].ptr = static_cast<void*>(dataPtr_);
  _dataSet.info[ord_].type = _identifyType<T>();
  _dataSet.info[ord_].size.raw = sizeof(T) * 8;
  _dataSet.info[ord_].size.encoded = _decideSize<T>(_dataSet.info[ord_].type, size_);
  return ERROR::OK;
}
template ERROR dataEncoder::append<bool>(uint8_t ord_, bool *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<uint8_t>(uint8_t ord_, uint8_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<uint16_t>(uint8_t ord_, uint16_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<uint32_t>(uint8_t ord_, uint32_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<uint64_t>(uint8_t ord_, uint64_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<int8_t>(uint8_t ord_, int8_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<int16_t>(uint8_t ord_, int16_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<int32_t>(uint8_t ord_, int32_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<int64_t>(uint8_t ord_, int64_t *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<float>(uint8_t ord_, float *dataPtr_, uint8_t size_);
template ERROR dataEncoder::append<double>(uint8_t ord_, double *dataPtr_, uint8_t size_);

ERROR dataEncoder::set() {
  _dataSet.bitLength = 0;
  for (uint8_t i = 0; i < MAX_DATA_NUM; i++) if (_dataSet.info[i].isActive) _dataSet.bitLength += _dataSet.info[i].size.encoded;
  if (_dataSet.bitLength > DATA_LENGTH_LIMIT) return ERROR::OVERFLOW;
  _dataSet.isEditable = false;
  return ERROR::OK;
}

ERROR dataEncoder::encode() {
  for (uint8_t i = 0; i < MAX_BYTE; i++) _dataPacket.data[i] = 0;
  _generateBinary();
  _dataPacket.data[0] = (0b10000000 | _dataSet.id);
  uint16_t idx = 0;
  for (uint16_t i = 0; i < _dataSet.bitLength; i++) {
    _dataPacket.data[idx + 1] |= (_binary[i] << (6 - (i % 7)));
    if ((i % 7) == 6) idx++;
  }
  if ((_dataSet.bitLength % 7) != 0) idx++;
  if ((idx / 64) > 1) return ERROR::OVERFLOW;
  uint16_t checkSum = 0;
  for (uint8_t i = 1; i < (idx + 1); i++) checkSum += _dataPacket.data[i];
  if (checkSum > 0x3FFF) return ERROR::OVERFLOW;
  _dataPacket.data[idx + 1] = ((checkSum >> 7) & 0x007F);
  _dataPacket.data[idx + 2] = (checkSum & 0x007F);
  _dataPacket.data[idx + 3] = (0b11000000 | ((idx % 64) & 0x003F));
  _dataPacket.length = idx + 4;
  return ERROR::OK;
}

dataPacket_t dataEncoder::getPacket() {return _dataPacket;}

template <typename T>
uint32_t dataEncoder::_getBitsData(uint8_t ord_) {
  T value;
  if (_dataSet.info[ord_].type == TYPE::BOOL) value = (T)*static_cast<bool*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::UINT) && (_dataSet.info[ord_].size.raw == 8)) value = (T)*static_cast<uint8_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::UINT) && (_dataSet.info[ord_].size.raw == 16)) value = (T)*static_cast<uint16_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::UINT) && (_dataSet.info[ord_].size.raw == 32)) value = (T)*static_cast<uint32_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::UINT) && (_dataSet.info[ord_].size.raw == 64)) value = (T)*static_cast<uint64_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::INT) && (_dataSet.info[ord_].size.raw == 8)) value = (T)*static_cast<int8_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::INT) && (_dataSet.info[ord_].size.raw == 16)) value = (T)*static_cast<int16_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::INT) && (_dataSet.info[ord_].size.raw == 32)) value = (T)*static_cast<int32_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::INT) && (_dataSet.info[ord_].size.raw == 64)) value = (T)*static_cast<int64_t*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::FLOAT) && (_dataSet.info[ord_].size.raw == 32)) value = (T)*static_cast<float*>(_dataSet.info[ord_].ptr);
  if ((_dataSet.info[ord_].type == TYPE::FLOAT) && (_dataSet.info[ord_].size.raw == 64)) value = (T)*static_cast<double*>(_dataSet.info[ord_].ptr);
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(uint32_t));
  return bits;
}
template uint32_t dataEncoder::_getBitsData<uint32_t>(uint8_t ord_);
template uint32_t dataEncoder::_getBitsData<int32_t>(uint8_t ord_);
template uint32_t dataEncoder::_getBitsData<float>(uint8_t ord_);

uint32_t dataEncoder::_getEncodedData(uint8_t ord_) {
  uint32_t encoded;
  uint32_t mask = 0;
  if (_dataSet.info[ord_].type == TYPE::INT) {
    for (uint8_t i = 0; i < (_dataSet.info[ord_].size.encoded - 1); i++) mask = ((mask << 1) | 0x00000001);
    bool signBit = ((_dataSet.info[ord_].data.bits >> 31) & 0x00000001);
    encoded = ((_dataSet.info[ord_].data.bits & mask) | (signBit << (_dataSet.info[ord_].size.encoded - 1)));
  }
  else {
    for (uint8_t i = 0; i < _dataSet.info[ord_].size.encoded; i++) mask = ((mask << 1) | 1);
    encoded = (_dataSet.info[ord_].data.bits & mask);
  }
  return encoded;
}

void dataEncoder::_generateBinary() {
  uint16_t idx = 0;
  for (uint8_t i = 0; i < MAX_DATA_NUM; i++) {
    if (_dataSet.info[i].isActive) {
      if (_dataSet.info[i].type == TYPE::INT) {
        _dataSet.info[i].data.bits = _getBitsData<int32_t>(i);
        _dataSet.info[i].data.encoded = _getEncodedData(i);
      }
      else if (_dataSet.info[i].type == TYPE::FLOAT) {
        _dataSet.info[i].data.bits = _getBitsData<float>(i);
        _dataSet.info[i].data.encoded = _getEncodedData(i);
      }
      else {
        _dataSet.info[i].data.bits = _getBitsData<uint32_t>(i);
        _dataSet.info[i].data.encoded = _getEncodedData(i);
      }
      for (uint8_t j = 0; j < _dataSet.info[i].size.encoded; j++) {
        _binary[idx] = (_dataSet.info[i].data.encoded >> (_dataSet.info[i].size.encoded - j - 1) & 0x00000001);
        idx++;
      }
    }
  }
}
#endif

#ifndef DATA_ENCODER_ONLY
dataDecoder::dataDecoder() {
  for (uint8_t i = 0; i < MAX_PORT_NUM; i++) {
    _dataSet[i].isEditable = true;
    _dataSet[i].id = i;
  }
  _bufferIndex = 0;
  _currentId = 255;
  for (uint16_t i = 0; i < BUFFER_SIZE; i++) _buffer[i] = 0;
}

template <typename T>
ERROR dataDecoder::append(uint8_t id_, uint8_t ord_, T *dataPtr_, uint8_t size_) {
  if (!_dataSet[id_].isEditable) return ERROR::UNEDITABLE;
  if (MAX_DATA_NUM <= ord_) return ERROR::INVALID_PARAM;
  if (_dataSet[id_].info[ord_].isActive) return ERROR::INVALID_PARAM;
  if (dataPtr_ == nullptr) return ERROR::INVALID_PARAM;
  if ((size_ == 0) || (32 < size_)) return ERROR::INVALID_PARAM;
  _dataSet[id_].info[ord_].isActive = true;
  _dataSet[id_].info[ord_].ptr = static_cast<void*>(dataPtr_);
  _dataSet[id_].info[ord_].type = _identifyType<T>();
  _dataSet[id_].info[ord_].size.raw = sizeof(T) * 8;
  _dataSet[id_].info[ord_].size.encoded = _decideSize<T>(_dataSet[id_].info[ord_].type, size_);
  return ERROR::OK;
}
template ERROR dataDecoder::append<bool>(uint8_t id_, uint8_t ord_, bool *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<uint8_t>(uint8_t id_, uint8_t ord_, uint8_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<uint16_t>(uint8_t id_, uint8_t ord_, uint16_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<uint32_t>(uint8_t id_, uint8_t ord_, uint32_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<uint64_t>(uint8_t id_, uint8_t ord_, uint64_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<int8_t>(uint8_t id_, uint8_t ord_, int8_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<int16_t>(uint8_t id_, uint8_t ord_, int16_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<int32_t>(uint8_t id_, uint8_t ord_, int32_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<int64_t>(uint8_t id_, uint8_t ord_, int64_t *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<float>(uint8_t id_, uint8_t ord_, float *dataPtr_, uint8_t size_);
template ERROR dataDecoder::append<double>(uint8_t id_, uint8_t ord_, double *dataPtr_, uint8_t size_);

ERROR dataDecoder::set() {
  for (uint8_t i = 0; i < MAX_PORT_NUM; i++) {
    _dataSet[i].bitLength = 0;
    for (uint8_t j = 0; j < MAX_DATA_NUM; j++) if (_dataSet[i].info[j].isActive) _dataSet[i].bitLength += _dataSet[i].info[j].size.encoded;
    if (_dataSet[i].bitLength > DATA_LENGTH_LIMIT) return ERROR::OVERFLOW;
    _dataSet[i].isEditable = false;
  }
  return ERROR::OK;
}

ERROR dataDecoder::appendToBuffer(uint8_t data) {
  if (_bufferIndex >= BUFFER_SIZE) return ERROR::OVERFLOW;
  _buffer[_bufferIndex++] = data;
  return ERROR::OK;
}

ERROR dataDecoder::decode() {
  ERROR error;
  error = _extractData();
  if (error != ERROR::OK) return error;
  error = _generateBinary();
  if (error != ERROR::OK) return error;
  error = _getEncodedData(_dataPacket.id);
  if (error != ERROR::OK) return error;
  error = _getBitsData(_dataPacket.id);
  if (error != ERROR::OK) return error;
  error = _restoreData(_dataPacket.id);
  if (error != ERROR::OK) return error;
  return ERROR::OK;
}

uint8_t dataDecoder::getCurrentId() {
  uint8_t id = _currentId;
  _currentId = 255;
  return id;
}

void dataDecoder::_shiftLeftBuffer(uint16_t step_) {
  for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
      if ((i + step_) < BUFFER_SIZE) _buffer[i] = _buffer[i + step_];
      else _buffer[i] = 0;
  }
  if (_bufferIndex >= step_) _bufferIndex -= step_;
  else _bufferIndex = 0;
}

ERROR dataDecoder::_extractData() {
  for (uint8_t i = 0; i < MAX_BYTE; i++) _dataPacket.data[i] = 0;

  if (_bufferIndex == 0) return ERROR::NO_DATA;

  if ((_buffer[0] & 0b11000000) != 0b10000000) {
    uint16_t idx = 1;

    while (idx < _bufferIndex && ((_buffer[idx] & 0b11000000) != 0b10000000)) {
      idx++;
    }

    if (idx >= _bufferIndex) {
      _shiftLeftBuffer(_bufferIndex);
      return ERROR::NO_DATA;
    }

    _shiftLeftBuffer(idx);
    return ERROR::INVALID_HEADER;
  }

  uint8_t len = 0;

  while (true) {
    if ((len + 3) >= _bufferIndex) return ERROR::INCOMPLETE_PACKET;

    if ((_buffer[len + 3] & 0b11000000) == 0b11000000) break;

    len++;

    if (len > DATA_LENGTH_LIMIT) return ERROR::INCOMPLETE_PACKET;
  }

  if ((len % 64) != (_buffer[len + 3] & 0b00111111)) {
    _shiftLeftBuffer(len + 3);
    return ERROR::INVALID_LENGTH;
  }

  uint32_t checkSum_calc = 0;
  for (uint8_t i = 0; i < len; i++) checkSum_calc += _buffer[i + 1];

  uint32_t checkSum_recv = ((uint16_t)(_buffer[len + 1] & 0b01111111) << 7) | (_buffer[len + 2] & 0b01111111);

  if (checkSum_calc != checkSum_recv) {
    _shiftLeftBuffer(1);
    return ERROR::INVALID_CHECKSUM;
  }

  _dataPacket.id = _buffer[0] & 0b00111111;
  _dataPacket.length = len;

  for (uint8_t i = 0; i < len; i++) _dataPacket.data[i] = _buffer[i + 1];

  _shiftLeftBuffer(len + 4);
  _currentId = _dataPacket.id;

  return ERROR::OK;
}

ERROR dataDecoder::_generateBinary() {
  if (_dataSet[_dataPacket.id].isEditable) return ERROR::UNSET_PACKET;
  for (uint16_t i = 0; i < MAX_BIT; i++) _binary[i] = 0;
  for (uint8_t i = 0; i < _dataPacket.length; i++) {
    for (uint8_t j = 0; j < 7; j++) _binary[i * 7 + j] = ((_dataPacket.data[i] >> (6 - j)) & 0x01);
  }
  return ERROR::OK;
}

ERROR dataDecoder::_getEncodedData(uint8_t id_) {
  uint16_t idx = 0;
  for (uint8_t i = 0; i < MAX_DATA_NUM; i++) {
    if (!_dataSet[id_].info[i].isActive) continue;
    _dataSet[id_].info[i].data.encoded = 0;
    for (uint8_t j = 0; j < _dataSet[id_].info[i].size.encoded; j++) {
      _dataSet[id_].info[i].data.encoded |= (_binary[idx] << (_dataSet[id_].info[i].size.encoded - 1 - j));
      idx++;
    }
  }
  return ERROR::OK;
}

ERROR dataDecoder::_getBitsData(uint8_t id_) {
  for (uint8_t i = 0; i < MAX_DATA_NUM; i++) {
    if (_dataSet[id_].info[i].type == TYPE::INT) {
      bool signBit = (bool)(_dataSet[id_].info[i].data.encoded >> (_dataSet[id_].info[i].size.encoded - 1) & 0x00000001);
      uint32_t mask = 0;
      for (uint8_t j = _dataSet[id_].info[i].size.encoded; j < 32; j++) mask |= (signBit << j);
      _dataSet[id_].info[i].data.bits = mask | _dataSet[id_].info[i].data.encoded;
    }
    else _dataSet[id_].info[i].data.bits = _dataSet[id_].info[i].data.encoded;
  }
  return ERROR::OK;
}

ERROR dataDecoder::_restoreData(uint8_t id_) {
  for (uint8_t i = 0; i < MAX_DATA_NUM; i++) {
    if (!_dataSet[id_].info[i].isActive) continue;
    switch (_dataSet[id_].info[i].type) {
      case (TYPE::BOOL): {
        uint32_t temp = static_cast<uint32_t>(_dataSet[id_].info[i].data.bits);
        *static_cast<bool*>(_dataSet[id_].info[i].ptr) = static_cast<bool>(temp);
        break;
      }
      case (TYPE::UINT): {
        uint32_t temp = static_cast<uint32_t>(_dataSet[id_].info[i].data.bits);
        switch (_dataSet[id_].info[i].size.raw) {
          case 8: *static_cast<uint8_t*>(_dataSet[id_].info[i].ptr) = static_cast<uint8_t>(temp); break;
          case 16: *static_cast<uint16_t*>(_dataSet[id_].info[i].ptr) = static_cast<uint16_t>(temp); break;
          case 32: *static_cast<uint32_t*>(_dataSet[id_].info[i].ptr) = static_cast<uint32_t>(temp); break;
          case 64: *static_cast<uint64_t*>(_dataSet[id_].info[i].ptr) = static_cast<uint64_t>(temp); break;
          default: return ERROR::INVALID_SIZE; break;
        }
        break;
      }
      case (TYPE::INT): {
        uint32_t temp = static_cast<uint32_t>(_dataSet[id_].info[i].data.bits);
        int32_t temps;
        memcpy(&temps, &temp, sizeof(temps));
        switch (_dataSet[id_].info[i].size.raw) {
          case 8: *static_cast<int8_t*>(_dataSet[id_].info[i].ptr) = static_cast<int8_t>(temps); break;
          case 16: *static_cast<int16_t*>(_dataSet[id_].info[i].ptr) = static_cast<int16_t>(temps); break;
          case 32: *static_cast<int32_t*>(_dataSet[id_].info[i].ptr) = static_cast<int32_t>(temps); break;
          case 64: *static_cast<int64_t*>(_dataSet[id_].info[i].ptr) = static_cast<int64_t>(temps); break;
          default: return ERROR::INVALID_SIZE; break;
        }
        break;
      }
      case (TYPE::FLOAT): {
        uint32_t temp = static_cast<uint32_t>(_dataSet[id_].info[i].data.bits);
        float tempf;
        memcpy(&tempf, &temp, sizeof(tempf));
        switch (_dataSet[id_].info[i].size.raw) {
          case 32: *static_cast<float*>(_dataSet[id_].info[i].ptr) = static_cast<float>(tempf); break;
          case 64: *static_cast<double*>(_dataSet[id_].info[i].ptr) = static_cast<double>(tempf); break;
          default: return ERROR::INVALID_SIZE; break;
        }
        break;
      }
      default: return ERROR::INVALID_TYPE; break;
    }
  }
  return ERROR::OK;
}
#endif

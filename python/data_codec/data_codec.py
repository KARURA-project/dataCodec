# data_codec.py
# C++ dataCodec 準拠版
#
# - dataEncoder: 1インスタンス = 1パケットID（id）
# - dataDecoder: 1インスタンス = 1固定パケットID
# - プロトコル:
#   header: 10iiiiii (i = id)
#   payload: 7bit pack (MSB→LSB, data[1] の bit6 から)
#   checksum: payload バイトの和の下位14bit → hi7 / lo7
#   tail: 11llllll (l = payload_length_bytes % 64)

from enum import Enum
from typing import Any, List, Optional
import struct

MAX_DATA_NUM = 10
MAX_BIT = MAX_DATA_NUM * 32
MAX_BYTE = (MAX_BIT + 6) // 7 + 4
BUFFER_SIZE = 4096
# ★DATA_LENGTH_LIMIT は「payload の最大バイト数」
DATA_LENGTH_LIMIT = 127

class TYPE(Enum):
    BOOL = 0
    UINT = 1
    INT = 2
    FLOAT = 3   # float / double（エンコード時は常に32bit）

class ERROR(Enum):
    OK = 0
    UNEDITABLE = 1
    INVALID_PARAM = 2
    OVERFLOW = 3
    NO_DATA = 4
    INVALID_HEADER = 5
    INCOMPLETE_PACKET = 6
    INVALID_LENGTH = 7
    INVALID_CHECKSUM = 8
    UNSET_PACKET = 9
    INVALID_TYPE = 10
    INVALID_SIZE = 11

class _DataInfo:
    def __init__(self):
        self.is_active = False
        self.ptr: Optional[List[Any]] = None
        self.type: Optional[TYPE] = None
        self.size = {"raw": 0, "encoded": 0}
        self.data = {"bits": 0, "encoded": 0}

class _DataSet:
    def __init__(self):
        self.is_editable = True
        self.id = 0
        self.info = [_DataInfo() for _ in range(MAX_DATA_NUM)]
        self.bit_length = 0     # 全データのエンコードビット長の合計
        self.payload_len = 0    # payload バイト長（bit_length から決まる）

class DataPacket:
    def __init__(self):
        self.id = 0
        # encoder 側では「フレーム全バイト数」、decoder 側では「header〜tail を含む長さ」を入れてよい
        self.length = 0
        self.data = bytearray(MAX_BYTE)

# ---------- Encoder (C++ dataEncoder 相当：1ポート専用) ----------

class DataCodecBaseEnc:
    def __init__(self, id_: int = 0):
        self._ds = _DataSet()
        self._ds.id = id_
        self._packet = DataPacket()
        self._packet.id = id_
        self._binary = [0]*MAX_BIT

    @staticmethod
    def _default_bits(tp: TYPE) -> int:
        if tp == TYPE.BOOL:  return 1
        if tp in (TYPE.UINT, TYPE.INT): return 32
        if tp == TYPE.FLOAT: return 32   # C++: FLOAT は常に32bit
        return 0

class DataEncoder(DataCodecBaseEnc):
    def __init__(self, id_: int = 0):
        super().__init__(id_)

    def append(self, ord_: int, ptr: List[Any], tp: TYPE, bits: Optional[int]=None) -> ERROR:
        if not (0 <= ord_ < MAX_DATA_NUM):
            return ERROR.INVALID_PARAM
        if not (isinstance(ptr, list) and len(ptr) == 1):
            return ERROR.INVALID_PARAM
        if not self._ds.is_editable:
            return ERROR.UNEDITABLE

        info = self._ds.info[ord_]
        if info.is_active:
            return ERROR.INVALID_PARAM

        info.is_active = True
        info.ptr = ptr
        info.type = tp

        # C++: FLOAT は常に32bit。それ以外は size or default
        if tp == TYPE.FLOAT:
            raw_bits = 32
        else:
            raw_bits = bits if (bits and bits > 0) else self._default_bits(tp)
        if raw_bits <= 0 or raw_bits > 64:
            return ERROR.INVALID_SIZE

        info.size["raw"] = raw_bits
        info.size["encoded"] = raw_bits
        info.data["bits"] = 0
        info.data["encoded"] = 0
        return ERROR.OK

    def set(self) -> ERROR:
        """bit ではなく payload バイト数で DATA_LENGTH_LIMIT をチェック"""
        total_bits = 0
        for i in range(MAX_DATA_NUM):
            if self._ds.info[i].is_active:
                total_bits += self._ds.info[i].size["encoded"]
        if total_bits == 0:
            return ERROR.NO_DATA

        payload_len = (total_bits + 6) // 7  # 7bit pack → バイト数

        if payload_len > DATA_LENGTH_LIMIT:
            return ERROR.OVERFLOW

        self._ds.bit_length = total_bits
        self._ds.payload_len = payload_len
        self._ds.is_editable = False
        return ERROR.OK

    def _gen_binary(self):
        idx = 0
        for i in range(MAX_DATA_NUM):
            inf = self._ds.info[i]
            if not inf.is_active:
                continue
            nbits = inf.size["encoded"]
            tp = inf.type
            if tp == TYPE.BOOL:
                u = 1 if bool(inf.ptr[0]) else 0
                width = 1
            elif tp == TYPE.UINT:
                u = int(inf.ptr[0]) & ((1 << nbits) - 1)
                width = nbits
            elif tp == TYPE.INT:
                u = int(inf.ptr[0]) & ((1 << nbits) - 1)
                width = nbits
            elif tp == TYPE.FLOAT:
                # float / double ともに float32 に丸めて送る（C++ヘッダ準拠）
                b = struct.pack('<f', float(inf.ptr[0]))
                u = int.from_bytes(b, 'little', signed=False)
                width = 32
            else:
                raise ValueError('invalid type')
            for j in range(width):
                bit = (u >> (width-1-j)) & 1
                self._binary[idx] = bit
                idx += 1

    def encode(self) -> ERROR:
        """C++ dataEncoder::encode() と同じフレーム構造で _packet を作る"""
        ds = self._ds
        if ds.is_editable or ds.bit_length == 0:
            return ERROR.UNSET_PACKET

        p = self._packet
        # 全クリア
        for i in range(MAX_BYTE):
            p.data[i] = 0

        # ビット列生成
        self._gen_binary()

        # header 10iiiiii
        p.data[0] = 0b10000000 | (ds.id & 0x3F)

        # payload (7bit pack)
        idx = 0  # payload index (data[idx+1])
        for i in range(ds.bit_length):
            bit = self._binary[i] & 1
            p.data[idx+1] |= (bit << (6 - (i % 7)))
            if (i % 7) == 6:
                idx += 1
        if (ds.bit_length % 7) != 0:
            idx += 1  # 最後の不完全バイト

        payload_len = idx  # payload バイト数

        # checksum（payloadのみの和）
        check = 0
        for i in range(1, payload_len+1):
            check += p.data[i]
        if check > 0x3FFF:
            return ERROR.OVERFLOW
        p.data[payload_len+1] = (check >> 7) & 0x7F
        p.data[payload_len+2] = check & 0x7F

        # tail 11llllll
        p.data[payload_len+3] = 0b11000000 | (payload_len % 64)

        # 総バイト数 = header(1) + payload + chk2 + tail(1)
        p.length = payload_len + 4
        return ERROR.OK

    def get_packet(self) -> DataPacket:
        """C++ の getPacket() と同様に DataPacket を返す"""
        return self._packet


# ---------- Decoder (C++ dataDecoder 相当：マルチポート) ----------

class DataDecoder:
    def __init__(self, id_: int = 0):
        # Single fixed packet ID decoder.
        self._data_set = _DataSet()
        self._data_set.id = id_
        self._data_set.is_editable = True

        self._packet = DataPacket()             # 直近のフレーム（header〜tail 含む）
        self._buffer = bytearray(BUFFER_SIZE)   # 受信バッファ
        self._buf_len = 0
        self._binary = [0]*MAX_BIT

    @staticmethod
    def _default_bits(tp: TYPE) -> int:
        if tp == TYPE.BOOL:  return 1
        if tp in (TYPE.UINT, TYPE.INT): return 32
        if tp == TYPE.FLOAT: return 32
        return 0

    def append(self, ord_: int, ptr: List[Any], tp: TYPE, bits: Optional[int]=None) -> ERROR:
        """Register a field for the fixed decoder ID."""
        if not (0 <= ord_ < MAX_DATA_NUM):
            return ERROR.INVALID_PARAM
        if not (isinstance(ptr, list) and len(ptr) == 1):
            return ERROR.INVALID_PARAM

        ds = self._data_set
        if not ds.is_editable:
            return ERROR.UNEDITABLE
        info = ds.info[ord_]
        if info.is_active:
            return ERROR.INVALID_PARAM

        info.is_active = True
        info.ptr = ptr
        info.type = tp

        if tp == TYPE.FLOAT:
            raw_bits = 32
        else:
            raw_bits = bits if (bits and bits > 0) else self._default_bits(tp)
        if raw_bits <= 0 or raw_bits > 64:
            return ERROR.INVALID_SIZE

        info.size["raw"] = raw_bits
        info.size["encoded"] = raw_bits
        info.data["bits"] = 0
        info.data["encoded"] = 0
        return ERROR.OK

    def set(self) -> ERROR:
        """Finalize the single decoder layout and validate payload size."""
        ds = self._data_set
        ds.bit_length = 0
        for i in range(MAX_DATA_NUM):
            if ds.info[i].is_active:
                ds.bit_length += ds.info[i].size["encoded"]

        if ds.bit_length == 0:
            ds.payload_len = 0
            ds.is_editable = False
            return ERROR.OK

        payload_len = (ds.bit_length + 6) // 7
        if payload_len > DATA_LENGTH_LIMIT:
            return ERROR.OVERFLOW

        ds.payload_len = payload_len
        ds.is_editable = False
        return ERROR.OK

    def get_current_id(self) -> int:
        """Return the last decoded packet ID."""
        return self._packet.id

    def append_to_buffer(self, b: int) -> ERROR:
        """Serial.read() 等で受けた1バイトを溜める"""
        if self._buf_len >= BUFFER_SIZE:
            # オーバーフロー時は半分だけ残して前半捨てるなどしても良いが，ここでは全クリア
            self._buf_len = 0
        self._buffer[self._buf_len] = b & 0xFF
        self._buf_len += 1
        return ERROR.OK

    def _shift_left(self, n: int):
        """バッファ先頭 n バイトを捨てる"""
        if n <= 0:
            return
        if n >= self._buf_len:
            self._buf_len = 0
            return
        remain = self._buf_len - n
        self._buffer[:remain] = self._buffer[n:self._buf_len]
        self._buf_len = remain

    def _extract_packet(self) -> ERROR:
        """バッファ先頭から1フレーム切り出して self._packet に格納"""
        if self._buf_len < 4:
            return ERROR.INCOMPLETE_PACKET

        # header 探索（10xxxxxx）
        if (self._buffer[0] & 0b11000000) != 0b10000000:
            idx = 1
            while idx < self._buf_len and (self._buffer[idx] & 0b11000000) != 0b10000000:
                idx += 1
            if idx == self._buf_len:
                # すべて捨てる
                self._buf_len = 0
                return ERROR.NO_DATA
            self._shift_left(idx)
            if self._buf_len < 4:
                return ERROR.INCOMPLETE_PACKET

        # tail 探索（11xxxxxx）
        length = 0
        while True:
            if 3 + length >= self._buf_len:
                return ERROR.INCOMPLETE_PACKET
            if (self._buffer[3+length] & 0b11000000) == 0b11000000:
                break
            length += 1
            if length > DATA_LENGTH_LIMIT:
                # 先頭1バイトだけ捨てて再同期
                self._shift_left(1)
                return ERROR.OVERFLOW

        # tail の l チェック
        tail = self._buffer[3+length]
        if (tail & 0x3F) != (length % 64):
            self._shift_left(1)
            return ERROR.INVALID_LENGTH

        # checksum (payload の和)
        check_calc = 0
        for i in range(length):
            check_calc += self._buffer[1+i]
        check_calc &= 0x3FFF
        c_hi = self._buffer[1+length] & 0x7F
        c_lo = self._buffer[2+length] & 0x7F
        check_recv = ((c_hi << 7) | c_lo) & 0x3FFF
        if check_calc != check_recv:
            self._shift_left(1)
            return ERROR.INVALID_CHECKSUM

        # フレーム確定：header〜tail をそのまま packet.data にコピー
        frame_len = length + 4
        self._packet.length = frame_len
        for i in range(frame_len):
            self._packet.data[i] = self._buffer[i]

        # id 抽出
        self._packet.id = self._buffer[0] & 0x3F

        # バッファからフレームを捨てる
        self._shift_left(frame_len)
        return ERROR.OK

    def _payload_bit_unpack(self):
        """payload 部を bit 列に戻す（_binary に格納）"""
        ds = self._data_set
        idx = 0
        for i in range(ds.bit_length):
            bindex = 1 + (i // 7)       # header の次から payload
            bitpos = 6 - (i % 7)
            bit = (self._packet.data[bindex] >> bitpos) & 1
            self._binary[idx] = bit
            idx += 1

    def _collect_encoded_words(self):
        """_binary から各データの encoded 値を復元"""
        ds = self._data_set
        idx = 0
        for i in range(MAX_DATA_NUM):
            inf = ds.info[i]
            if not inf.is_active:
                continue
            n = inf.size["encoded"]
            acc = 0
            for j in range(n):
                acc |= (self._binary[idx] << (n-1-j))
                idx += 1
            inf.data["encoded"] = acc

    def _get_bits_data(self) -> ERROR:
        """INT 用に符号拡張して bits を作る"""
        ds = self._data_set
        for i in range(MAX_DATA_NUM):
            inf = ds.info[i]
            if not inf.is_active:
                continue
            nraw = inf.size["raw"]
            enc = inf.data["encoded"]

            if inf.type == TYPE.INT:
                # 任意ビット幅の2の補数（1..64bit）
                if not (1 <= nraw <= 64):
                    return ERROR.INVALID_SIZE
                sign_bit = 1 << (nraw - 1)
                if nraw == 64:
                    mask = 0xFFFFFFFFFFFFFFFF
                else:
                    mask = (1 << nraw) - 1
                val = enc & mask
                if val & sign_bit:
                    full = val | (~mask)
                else:
                    full = val
                inf.data["bits"] = full
            else:
                inf.data["bits"] = enc
        return ERROR.OK

    def _restore_values(self) -> ERROR:
        """実際の変数に値を書き戻す"""
        ds = self._data_set
        for i in range(MAX_DATA_NUM):
            inf = ds.info[i]
            if not inf.is_active:
                continue
            nraw = inf.size["raw"]
            bits = inf.data["bits"]

            if inf.type == TYPE.BOOL:
                inf.ptr[0] = bool(bits & 1)

            elif inf.type == TYPE.UINT:
                if not (1 <= nraw <= 64):
                    return ERROR.INVALID_SIZE
                if nraw == 64:
                    mask = 0xFFFFFFFFFFFFFFFF
                else:
                    mask = (1 << nraw) - 1
                inf.ptr[0] = int(bits & mask)

            elif inf.type == TYPE.INT:
                if not (1 <= nraw <= 64):
                    return ERROR.INVALID_SIZE
                if nraw == 64:
                    mask = 0xFFFFFFFFFFFFFFFF
                else:
                    mask = (1 << nraw) - 1
                sign_bit = 1 << (nraw - 1)
                val = bits & mask
                if val & sign_bit:
                    full = val | (~mask)
                else:
                    full = val
                inf.ptr[0] = int(full)

            elif inf.type == TYPE.FLOAT:
                # 常に float32 から復元（double も float32 に丸めた値）
                temp = bits & 0xFFFFFFFF
                f = struct.unpack('<f', struct.pack('<I', temp))[0]
                if nraw in (32, 64):
                    inf.ptr[0] = float(f)
                else:
                    return ERROR.INVALID_SIZE

            else:
                return ERROR.INVALID_TYPE
        return ERROR.OK

    def decode(self) -> ERROR:
        """バッファから1フレーム分をデコードして，対応する id の変数群に書き込む"""
        err = self._extract_packet()
        if err != ERROR.OK:
            return err

        if self._packet.id != self._data_set.id:
            return ERROR.INVALID_PARAM

        ds = self._data_set
        if ds.is_editable or ds.bit_length == 0:
            return ERROR.UNSET_PACKET

        self._payload_bit_unpack()
        self._collect_encoded_words()
        err = self._get_bits_data()
        if err != ERROR.OK:
            return err
        err = self._restore_values()
        return err
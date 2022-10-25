from typing import Dict
import logging
import serial
import construct

log = logging.getLogger("PylonToMQTT")

class HexToByte(construct.Adapter):
    def _decode(self, obj, context, path) -> bytes:
        hexstr = ''.join([chr(x) for x in obj])
        return bytes.fromhex(hexstr)

class JoinBytes(construct.Adapter):
    def _decode(self, obj, context, path) -> bytes:
        return ''.join([chr(x) for x in obj]).encode()

class DivideBy1000(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return obj / 1000

class DivideBy100(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return obj / 100

class ToVolt(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return round((obj / 1000), 3)

class ToAmp(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return round((obj / 100), 2)
    
class Round1(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return round((obj), 1)

class Round2(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return round((obj), 2)

class ToCelsius(construct.Adapter):
    def _decode(self, obj, context, path) -> float:
        return round(((obj - 2730) / 10.0), 2)  # in Kelvin*10

class Pylontech:

    get_alarm_fmt = construct.Struct(
        "NumberOfModule" / construct.Byte,
        "NumberOfCells" / construct.Int8ub,
        "CellState" / construct.Array(construct.this.NumberOfCells, construct.Int8ub),
        "NumberOfTemperatures" / construct.Int8ub,
        "CellsTemperatureStates" / construct.Array(construct.this.NumberOfTemperatures, construct.Int8ub),
        "_UserDefined1" / construct.Int8ub,
        "CurrentState" / construct.Int8ub,
        "VoltageState" / construct.Int8ub,
    )

    pack_count_fmt = construct.Struct(
        "PackCount" / construct.Byte,
    )

    get_analog_fmt = construct.Struct(
        "NumberOfModule" / construct.Byte,
        "NumberOfCells" / construct.Int8ub,
        "CellVoltages" / construct.Array(construct.this.NumberOfCells, ToVolt(construct.Int16sb)),
        "NumberOfTemperatures" / construct.Int8ub,
        "GroupedCellsTemperatures" / construct.Array(construct.this.NumberOfTemperatures, ToCelsius(construct.Int16sb)),
        "Current" / ToAmp(construct.Int16sb),
        "Voltage" / ToVolt(construct.Int16ub),
        "Power" / Round1(construct.Computed(construct.this.Current * construct.this.Voltage)),
        "_RemainingCapacity" / construct.Int16ub,
        "RemainingCapacity" / DivideBy100(construct.Computed(construct.this._RemainingCapacity)),
        "_UserDefinedItems" / construct.Int8ub,
        "TotalCapacity" / DivideBy100(construct.Int16ub),
        "CycleNumber" / construct.Int16ub,
        "StateOfCharge" / Round1(construct.Computed(construct.this._RemainingCapacity / construct.this.TotalCapacity)),
    )

    def __init__(self, serial_port='/dev/ttyUSB0', baudrate=9600):
        self.s = serial.Serial(serial_port, baudrate, bytesize=8, parity=serial.PARITY_NONE, stopbits=1, timeout=10)

    @staticmethod
    def get_frame_checksum(frame: bytes):
        assert isinstance(frame, bytes)
        sum = 0
        for byte in frame:
            sum += byte
        sum = ~sum
        sum %= 0x10000
        sum += 1
        return sum

    @staticmethod
    def get_info_length(info: bytes) -> int:
        lenid = len(info)
        if lenid == 0:
            return 0
        lenid_sum = (lenid & 0xf) + ((lenid >> 4) & 0xf) + ((lenid >> 8) & 0xf)
        lenid_modulo = lenid_sum % 16
        lenid_invert_plus_one = 0b1111 - lenid_modulo + 1
        return (lenid_invert_plus_one << 12) + lenid

    def send_cmd(self, address: int, cmd, info: bytes = b''):
        raw_frame = self._encode_cmd(address, cmd, info)
        self.s.write(raw_frame)

    def _encode_cmd(self, address: int, cid2: int, info: bytes = b''):
        cid1 = 0x46
        info_length = Pylontech.get_info_length(info)
        frame = "{:02X}{:02X}{:02X}{:02X}{:04X}".format(0x20, address, cid1, cid2, info_length).encode()
        frame += info
        frame_chksum = Pylontech.get_frame_checksum(frame)
        whole_frame = (b"~" + frame + "{:04X}".format(frame_chksum).encode() + b"\r")
        return whole_frame

    def _decode_hw_frame(self, raw_frame: bytes) -> bytes:
        # XXX construct
        frame_data = raw_frame[1:len(raw_frame) - 5]
        frame_chksum = raw_frame[len(raw_frame) - 5:-1]
        got_frame_checksum = Pylontech.get_frame_checksum(frame_data)
        assert got_frame_checksum == int(frame_chksum, 16)
        return frame_data

    def _decode_frame(self, frame):
        format = construct.Struct(
            "ver" / HexToByte(construct.Array(2, construct.Byte)),
            "adr" / HexToByte(construct.Array(2, construct.Byte)),
            "cid1" / HexToByte(construct.Array(2, construct.Byte)),
            "cid2" / HexToByte(construct.Array(2, construct.Byte)),
            "infolength" / HexToByte(construct.Array(4, construct.Byte)),
            "info" / HexToByte(construct.GreedyRange(construct.Byte)),
        )
        return format.parse(frame)

    def read_frame(self):
        raw_frame = self.s.read_until(b'\r')
        log.debug("read_frame: {}".format(raw_frame.hex()))
        f = self._decode_hw_frame(raw_frame=raw_frame)
        parsed = self._decode_frame(f)
        return parsed

    def get_pack_count(self):
        self.send_cmd(0, 0x90)
        f =  self.read_frame()
        return self.pack_count_fmt.parse(f.info)

    def get_version_info(self, dev_id):
        bdevid = "{:02X}".format(dev_id).encode()
        self.send_cmd(0, 0xC1, bdevid)
        f = self.read_frame()
        version_info_fmt = construct.Struct(
            "Version" / construct.CString("utf8")
        )
        return version_info_fmt.parse(f.info)
    
    def get_barcode(self, dev_id):
        bdevid = "{:02X}".format(dev_id).encode()
        self.send_cmd(0, 0xC2, bdevid)
        f = self.read_frame()
        version_info_fmt = construct.Struct(
            "Barcode" / construct.PaddedString(15, "utf8")
        )
        return version_info_fmt.parse(f.info)

    def get_alarm_info(self, dev_id):
        bdevid = "{:02X}".format(dev_id).encode()
        self.send_cmd(dev_id, 0x44, bdevid)
        f = self.read_frame()

        return self.get_alarm_fmt.parse(f.info[1:])

    def get_values_single(self, dev_id):
        bdevid = "{:02X}".format(dev_id).encode()
        self.send_cmd(dev_id, 0x42, bdevid)
        f = self.read_frame()

        d = self.get_analog_fmt.parse(f.info[1:])
        return d


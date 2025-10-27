from enum import Enum, auto
import ctypes

class Node(Enum):
    SOURCE_FILE = auto()
    IF_STMT = auto()
    EXPR = auto()

class IotaLib:
    lib: ctypes.CDLL

    def __init__(self, sopath: str):
        self.lib = ctypes.cdll.LoadLibrary(sopath)
        self.lib.ffi_parse.restype = ctypes.c_void_p
        self.lib.ffi_parse.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]
        self.lib.free.restype = None
        self.lib.free.argtypes = [ctypes.c_void_p]

    def parse_with(self, fn, src: str, delim: bool = False) -> str:
        rbuf = self.lib.ffi_parse(fn, src.encode("UTF-8"), delim)
        ast = ctypes.cast(rbuf, ctypes.c_char_p).value
        self.lib.free(rbuf)
        return ast.decode()

    def parse_as(self, t: Node, src: str) -> str:
        match t:
            case Node.SOURCE_FILE:
                return self.parse_with(self.lib.parse_source_file, src);
            case Node.IF_STMT:
                return self.parse_with(self.lib.parse_if_stmt, src);
            case Node.EXPR:
                return self.parse_with(self.lib.parse_expr, src, delim=True);

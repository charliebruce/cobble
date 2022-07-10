import zipfile
import json
from cobble import cobble
from time import sleep
from enum import IntEnum
from binascii import crc32
from tqdm import tqdm

# Implementation of Nordic Secure BLE using Cobble
# Legacy has subtle differences.

dfu_sv_uuid = "0000FE59-0000-1000-8000-00805F9B34FB"
dfu_ctrl_uuid = "8EC90001-F315-4F60-9FB8-838830DAEA50"
dfu_data_uuid = "8EC90002-F315-4F60-9FB8-838830DAEA50"

legacy_sv_uuid = "00001530-1212-EFDE-1523-785FEABCD123"
legacy_ctrl_uuid = "00001531-1212-EFDE-1523-785FEABCD123"
legacy_data_uuid = "00001532-1212-EFDE-1523-785FEABCD123"
legacy_revision_uuid = "00001534-1212-EFDE-1523-785FEABCD123"

# Refer to dfu/nrf_dfu_types.h
class NRF_DFU_OBJ_TYPE(IntEnum):
    INVALID = 0,                   #!< Invalid object type.
    COMMAND = 1,                   #!< Command object.
    DATA = 2                       #!< Data object.

class NRF_DFU_OP(IntEnum):
    PROTOCOL_VERSION     = 0x00      #!< Retrieve protocol version.
    OBJECT_CREATE        = 0x01      #!< Create selected object.
    RECEIPT_NOTIF_SET    = 0x02      #!< Set receipt notification.
    CRC_GET              = 0x03      #!< Request CRC of selected object.
    OBJECT_EXECUTE       = 0x04      #!< Execute selected object.
    OBJECT_SELECT        = 0x06      #!< Select object.
    MTU_GET              = 0x07      #!< Retrieve MTU size.
    OBJECT_WRITE         = 0x08      #!< Write selected object. DATA characteristic handles this automatically, no need to add the opcode, just write data
    PING                 = 0x09      #!< Ping.
    HARDWARE_VERSION     = 0x0A      #!< Retrieve hardware version.
    FIRMWARE_VERSION     = 0x0B      #!< Retrieve firmware version.
    ABORT                = 0x0C      #!< Abort the DFU procedure.
    RESPONSE             = 0x60      #!< Response.
    INVALID              = 0xFF,

class NRF_DFU_RES_CODE(IntEnum):
    INVALID                 = 0x00     #!< Invalid opcode.
    SUCCESS                 = 0x01     #!< Operation successful.
    OP_CODE_NOT_SUPPORTED   = 0x02     #!< Opcode not supported.
    INVALID_PARAMETER       = 0x03     #!< Missing or invalid parameter value.
    INSUFFICIENT_RESOURCES  = 0x04     #!< Not enough memory for the data object.
    INVALID_OBJECT          = 0x05     #!< Data object does not match the firmware and hardware requirements, the signature is wrong, or parsing the command failed.
    UNSUPPORTED_TYPE        = 0x07     #!< Not a valid object type for a Create request.
    OPERATION_NOT_PERMITTED = 0x08     #!< The state of the DFU process does not allow this operation.
    OPERATION_FAILED        = 0x0A     #!< Operation failed.
    EXT_ERROR               = 0x0B     #!< Extended error. The next byte of the response contains the error code of the extended error (see @ref code_t.

class NRF_DFU_FIRMWARE_TYPE(IntEnum):
    SOFTDEVICE    = 0x00
    APPLICATION   = 0x01
    BOOTLOADER    = 0x02
    UNKNOWN       = 0xFF

class NRF_DFU_EXT_ERROR(IntEnum):
    NO_ERROR                  = 0x00     # No extended error code has been set. This error indicates an implementation problem. */
    INVALID_ERROR_CODE        = 0x01     # Invalid error code. This error code should never be used outside of development. */
    WRONG_COMMAND_FORMAT      = 0x02     # The format of the command was incorrect. This error code is not used in the
                                         #                    current implementation, because @ref NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED
                                         #                    and @ref NRF_DFU_RES_CODE_INVALID_PARAMETER cover all
                                         #                    possible format errors. */
    UNKNOWN_COMMAND           = 0x03     # The command was successfully parsed, but it is not supported or unknown. */
    INIT_COMMAND_INVALID      = 0x04     # The init command is invalid. The init packet either has
                                         #                    an invalid update type or it is missing required fields for the update type
                                         #                    (for example, the init packet for a SoftDevice update is missing the SoftDevice size field). */
    FW_VERSION_FAILURE        = 0x05     # The firmware version is too low. For an application or SoftDevice, the version must be greater than
                                         #                    or equal to the current version. For a bootloader, it must be greater than the current version.
                                         #                    to the current version. This requirement prevents downgrade attacks.*/
    HW_VERSION_FAILURE        = 0x06     # The hardware version of the device does not match the required
                                         #                    hardware version for the update. */
    SD_VERSION_FAILURE        = 0x07     # The array of supported SoftDevices for the update does not contain
                                         #                    the FWID of the current SoftDevice or the first FWID is '0' on a
                                         #                    bootloader which requires the SoftDevice to be present. */
    SIGNATURE_MISSING         = 0x08     # The init packet does not contain a signature. This error code is not used in the
                                         #                    current implementation, because init packets without a signature
                                         #                    are regarded as invalid. */
    WRONG_HASH_TYPE           = 0x09     # The hash type that is specified by the init packet is not supported by the DFU bootloader. */
    HASH_FAILED               = 0x0A     # The hash of the firmware image cannot be calculated. */
    WRONG_SIGNATURE_TYPE      = 0x0B     # The type of the signature is unknown or not supported by the DFU bootloader. */
    VERIFICATION_FAILED       = 0x0C     # The hash of the received firmware image does not match the hash in the init packet. */
    INSUFFICIENT_SPACE        = 0x0D     # The available space on the device is insufficient to hold the firmware. */

# Refer to components/ble/ble_services/ble_dfu/ble_dfu.h and ble_dfu.c
class NRF_LEGACY_DFU_OP(IntEnum):
    START_DFU = 1
    INIT_DFU_PARAMS = 2
    RECV_FW_IMG = 3
    VALIDATE_FW = 4
    ACTIVATE_IMG_RST = 5
    RESET = 6
    REPORT_SIZE = 7
    RECEIPT_REQ = 8
    RESPONSE = 16
    RECEIPT_NOTIF = 17


class NRF_LEGACY_DFU_RESPONSE(IntEnum):
    SUCCESS = 1
    INVALID_STATE = 2
    NOT_SUPPORTED = 3
    DATA_LIMIT_EXCEEDED = 4
    CRC_ERROR = 5
    OPERATION_FAILED = 6

class LEGACY_PROCEDURES(IntEnum):
    BLE_DFU_START_PROCEDURE        = 1                                 #/**< DFU Start procedure.*/
    BLE_DFU_INIT_PROCEDURE         = 2                                 #/**< DFU Initialization procedure.*/
    BLE_DFU_RECEIVE_APP_PROCEDURE  = 3                                 #/**< Firmware receiving procedure.*/
    BLE_DFU_VALIDATE_PROCEDURE     = 4                                 #/**< Firmware image validation procedure .*/
    BLE_DFU_PKT_RCPT_REQ_PROCEDURE = 8                                 #/**< Packet receipt notification request procedure. */


def load_firmware(fname):
    with zipfile.ZipFile(fname) as zf:
        with zf.open("manifest.json") as mf:
            js = json.loads(mf.read())
            bin_file = zf.open(js['manifest']['application']['bin_file'], 'r').read()
            dat_file = zf.open(js['manifest']['application']['dat_file'], 'r').read()

    print("Loaded FW OK")
    return (bin_file, dat_file)


def set_data(data, chunk_size=244):
    # When reviewing bootloader code - note that these are still handled by nrf_dfu_req_handler
    # They just have the type NRF_DFU_OP_OBJECT_WRITE appended automatically
    bytes_sent = 0
    while(bytes_sent < len(data)):
        # Take the next chunk of data
        next_size = min(chunk_size, len(data)-bytes_sent)
        segment = data[bytes_sent : bytes_sent + next_size]
        cobble.write(dfu_data_uuid, segment)
        sleep(0.1) # TODO: Use receipt request rather than fixed delay for speed?
        bytes_sent += next_size

def set_legacy_data(data, chunk_size=20):
    # When reviewing bootloader code - note that these are still handled by nrf_dfu_req_handler
    # They just have the type NRF_DFU_OP_OBJECT_WRITE appended automatically
    bytes_sent = 0
    while(bytes_sent < len(data)):
        # Take the next chunk of data
        next_size = min(chunk_size, len(data)-bytes_sent)
        segment = data[bytes_sent : bytes_sent + next_size]
        cobble.write(legacy_data_uuid, segment)
        sleep(0.1) # TODO: Use receipt request rather than fixed delay for speed?
        bytes_sent += next_size



def set_control(data):
    cobble.write(dfu_ctrl_uuid, bytes(data))

def set_legacy_control(data):
    cobble.write(legacy_ctrl_uuid, bytes(data))
    
def expect_response(legacy = False):

    # Await a notification
    while (notif := cobble.get_updatevalue()) == None:
        sleep(0.01)

    # Response data
    rd = notif[1]

    # TODO: Handle errors for legacy DFU
    if legacy:
        return rd

    # Confirm that it's a response to a command
    op = NRF_DFU_OP(rd[0])
    assert op == NRF_DFU_OP.RESPONSE, f"Expecting a response but got operation code {op}:{op.name}"

    # Check that we're dealing with a command we understand
    cmd = NRF_DFU_OP(rd[1])

    # Confirm that it's a success
    res = NRF_DFU_RES_CODE(rd[2])

    # Handle errors
    if(res == NRF_DFU_RES_CODE.EXT_ERROR):
        ext = NRF_DFU_EXT_ERROR(rd[3])
        assert False, f"Device reported extended error {ext}:{ext.name}"
    assert res == NRF_DFU_RES_CODE.SUCCESS, f"Device replied with an error response {res}:{res.name}"

    return rd

def u32le(i):
    return ((i[0]<<0) | (i[1]<<8) | (i[2]<<16) | (i[3]<<24))

def u32tole(i):
    return [(i & 0xFF), ((i >> 8) & 0xFF), ((i >> 16) & 0xFF), ((i >> 24) & 0xFF)]

def u16le(i):
    return ((i[0]<<0) | (i[1]<<8))

def u16tole(i):
    return [(i & 0xFF), ((i >> 8) & 0xFF)]

# Wireshark capture shows:
# 1: M->S: Control: Start DFU (0x01), Application (0x04)
# 2: M->S: Packet: 000000000000000000ec0100
# 3: S->M: Control: Response (0x10), Start DFU (0x01), Success (0x01)
# 4: M->S: Control: Init DFU (0x02), Receive Init Packet (0x00)
# 5: M->S: Packet: ffffffffffffffff0100feff20b0
# 6: M->S: Control: Init DFU (0x02), Finish Init Packet (0x01)
# 7: S->M: Control: Response (0x10), Init DFU params (0x02), Success (0x01)
# 8: M->S: Control: Please Give Me Receipts (0x08), Every 12 packets (0x0c 0x00)
# 9: M->S: Control: Receive FW Image (0x03)
# (14 times): M->S: Packet: (20 bytes)
# S->M: Control: Receipt (0x11), 240 bytes received (0xf0 0x00 0x00 0x00)
# (14 times): M->S: Packet: (20 bytes)
# S->M: Control: Receipt (0x11), 480 bytes received (0xe0 0x01 0x00 0x00)
# (repeats above 2 steps until all data sent)
# S->M: Control: Response (0x10), Receive FW Image (0x03), Success (0x03)
# M->S (Validate)
# S->M (Validate is success)
# M->S (Activate and Reset)
# (Connection terminates)

# Why do we write 14 packets * 20 bytes = 280 bytes, but report 240 bytes received?


def do_legacy_update(fname, identifier):
    # Legacy DFU was a prototype / early implementation of DFU up until around 2016. It was
    # replaced with Secure DFU around 2017 or so. Older devices (around SDK 10) will still
    # require this legacy approach to be used. The structure is broadly similar.

    bin_file, dat_file = load_firmware(fname)

    cobble.connect(identifier)
    # TODO: Await connection event rather than just sleeping
    sleep(2)

    assert len(cobble.characteristics) > 0, "Failed to find DFU characteristics in time"

    from pprint import pprint

    assert (legacy_sv_uuid, legacy_ctrl_uuid) in cobble.characteristics, "Missing DFU Control characteristic"
    assert (legacy_sv_uuid, legacy_data_uuid) in cobble.characteristics, "Missing DFU Packet characteristic"

    # DFU Process
    print("Subscribing...")

    # Listen for changes on the DFU Control characteristic
    cobble.subscribe(legacy_ctrl_uuid)
    sleep(4)

    print("Initialising Legacy DFU...")

    # Start DFU process
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.START_DFU, 0x04])) # Op 1

    # Tell bootloader expected DFU size and other config
    bl_cfg = bytes([0x00] * 8 + u32tole(len(bin_file)))
    set_legacy_data(bl_cfg) # Handled by start_data_process - Op 2

    rd = expect_response(legacy=True) # Op 3
    pprint(rd) # If we're resuming, the next assert will fail
    assert rd == bytes([NRF_LEGACY_DFU_OP.RESPONSE, LEGACY_PROCEDURES.BLE_DFU_START_PROCEDURE, NRF_LEGACY_DFU_RESPONSE.SUCCESS]), "Got a bad response"

    # Send the init packet
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.INIT_DFU_PARAMS, 0x00])) # DFU_INIT_RX - Op 4
    set_legacy_data(dat_file) # Handled by init_data_process # Op 5
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.INIT_DFU_PARAMS, 0x01])) # DFU_INIT_COMPLETE - Op 6
    
    rd = expect_response(legacy=True) # Op 7
    pprint(rd) # If we're resuming, the next assert will fail
    assert rd == bytes([NRF_LEGACY_DFU_OP.RESPONSE, LEGACY_PROCEDURES.BLE_DFU_INIT_PROCEDURE, NRF_LEGACY_DFU_RESPONSE.SUCCESS]), "Got a bad response"
    
    print("Init OK...")

    # Requiest delivery receipts every N slices of data
    receipt_interval = 12
    interval = u16tole(receipt_interval)
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.RECEIPT_REQ, interval[0], interval[1]])) # Op 8
    # No response if OK. Will respond with BLE_DFU_PKT_RCPT_REQ_PROCEDURE, BLE_DFU_RESP_VAL_NOT_SUPPORTED if we send an invalid command (ie wrong length).

    # Tell the target to expect data
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.RECV_FW_IMG])) # Op 9
    # No response expected.

    print("Transferring data in slices...")
    # Send the data in slices of 20 bytes
    size = 20
    slices = [bin_file[i:i+size] for i in range(0, len(bin_file), size)]
    sent = 0
    for s in tqdm(slices):

        set_legacy_data(s) # handled by app_data_process
        sent += 1

        # We expect this to fire once every receipt_interval times
        # TODO: Why do we request 12 but get every 14?!
        # Note - we skip the last one, because it's not a receipt but a success result...
        if sent % 14 == 0 and (sent < len(slices)): 
            rd = expect_response(legacy=True)
            pprint(rd)
            #assert rd[0] == NRF_LEGACY_DFU_OP.RECEIPT_NOTIF, "Bad response, expected a receipt"
            # TODO: Verify the remaining 4 bytes are equal to num_of_firmware_bytes_rcvd
            sleep(1)

            # Keep alive by requesting the size every so often???
            set_legacy_control(bytes([NRF_LEGACY_DFU_OP.REPORT_SIZE]))
            rd = expect_response(legacy=True)
            pprint("Size report incoming: ")
            pprint(rd)

    # Verify that the firmware thinks the transfer is complete
    rd = expect_response(legacy=True)
    pprint(rd)
    assert rd == bytes([NRF_LEGACY_DFU_OP.RESPONSE, LEGACY_PROCEDURES.BLE_DFU_VALIDATE_PROCEDURE, NRF_LEGACY_DFU_RESPONSE.SUCCESS]), "Firmware doesn't think transfer is complete"

    # Validate
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.VALIDATE_FW]))
    rd = expect_response()
    assert rd == bytes([NRF_LEGACY_DFU_OP.RESPONSE, LEGACY_PROCEDURES.BLE_DFU_VALIDATE_PROCEDURE, NRF_LEGACY_DFU_RESPONSE.SUCCESS]), "Got a bad response"

    # Wait whilst target commits the data
    sleep(1)
    
    # Activate and reset
    set_legacy_control(bytes([NRF_LEGACY_DFU_OP.ACTIVATE_IMG_RST]))
    
    print("DONE")
    pass

def do_buttonless_entry(identifier):
    raise NotImplementedError

def do_secure_update(fname, identifier):

    bin_file, dat_file = load_firmware(fname)

    cobble.connect(identifier)
    # TODO: Await connection event rather than just sleeping
    sleep(2)

    assert len(cobble.characteristics) > 0, "Failed to find DFU characteristics in time"

    assert (dfu_sv_uuid, dfu_ctrl_uuid) in cobble.characteristics, "Missing DFU Control characteristic"
    assert (dfu_sv_uuid, dfu_data_uuid) in cobble.characteristics, "Missing DFU Packet characteristic"

    # DFU Process
    print("Subscribing...")

    # Listen for changes on the DFU Control characteristic
    cobble.subscribe(dfu_ctrl_uuid)
    sleep(4)

    print("Initialising DFU...")

    # TODO: For speed, request maximum MTU?  MTU 247 (244 usable bytes) better than MTU 23 (20 usable bytes)
    # TODO: Exchange any information we might be interested in.

    # Flow as in:
    # https:#infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v14.1.0%2Flib_bootloader_dfu_process.html

    # Select the init packet, see if it matches our expectations
    set_control(bytes([NRF_DFU_OP.OBJECT_SELECT, NRF_DFU_OBJ_TYPE.COMMAND]))
    rd = expect_response()
    maxSize = u32le(rd[3:7])
    offset = u32le(rd[7:11])
    crc = u32le(rd[11:15])

    # Are the values what we'd expect? If so, we're resuming a faulty transfer
    if(crc != crc32(dat_file)) or (offset != len(dat_file)):

        # Tell the DFU to expect a new init packet
        initlen = u32tole(len(dat_file))
        set_control(bytes([NRF_DFU_OP.OBJECT_CREATE, NRF_DFU_OBJ_TYPE.COMMAND, initlen[0], initlen[1], initlen[2], initlen[3]]))
        rd = expect_response()

        # Send the init data
        set_data(dat_file)

        # CRC it
        set_control(bytes([NRF_DFU_OP.CRC_GET]))
        rd = expect_response()
        assert NRF_DFU_OP(rd[1]) == NRF_DFU_OP.CRC_GET, "Received weird response"
        offset = u32le(rd[3:7])
        crc = u32le(rd[7:11])
        assert offset == len(dat_file), "Device received fewer bytes than we sent"
        assert crc == crc32(dat_file), f"CRC check failed: Our file is {crc32(dat_file)}, but device got {crc}"

        # Prevalidate (execute)
        set_control(bytes([NRF_DFU_OP.OBJECT_EXECUTE]))
        rd = expect_response()

    # Device is now stuck in DFU mode until an update completes.
    print("Initialised OK, now sending data...")

    # Select the data packet
    set_control(bytes([NRF_DFU_OP.OBJECT_SELECT, NRF_DFU_OBJ_TYPE.DATA]))
    rd = expect_response()
    maxSize = u32le(rd[3:7])
    offset = u32le(rd[7:11])
    crc = u32le(rd[11:15])

    assert offset==0, "Resuming not supported!"

    # The device can't accept the entire firmware in one transfer, it needs chunks (in the case of nRF52832, 4096 bytes max)
    # Smaller chunks also gives us more opportunities to catch issues with the CRC and retry without needing to
    # re-send the whole image.
    num_data_objects = ((len(bin_file) + maxSize - 1) // maxSize)
    print(f"We will send {num_data_objects} data objects of size {maxSize} to transfer the needed {len(bin_file)} bytes")

    bytes_remaining = len(bin_file)
    progress = tqdm(total = bytes_remaining)
    while(bytes_remaining):

        # TODO: Can we CRC this block without writing, to save time if resuming a disrupted upload?
        
        transfer_size = min(bytes_remaining, maxSize)
        data_start_idx = len(bin_file) - bytes_remaining
        data_end_idx = data_start_idx + transfer_size
        chunk = bin_file[data_start_idx : data_end_idx]

        # Create data object
        datalen = u32tole(transfer_size)
        set_control(bytes([NRF_DFU_OP.OBJECT_CREATE, NRF_DFU_OBJ_TYPE.DATA, datalen[0], datalen[1], datalen[2], datalen[3]]))
        rd = expect_response()

        # Transfer firmware data
        set_data(chunk)

        # CRC
        set_control(bytes([NRF_DFU_OP.CRC_GET]))
        rd = expect_response()
        assert NRF_DFU_OP(rd[1]) == NRF_DFU_OP.CRC_GET, "Received weird response"
        offset = u32le(rd[3:7])
        crc = u32le(rd[7:11])
        expected_offset = len(bin_file) - bytes_remaining + transfer_size
        expected_crc = crc32(bin_file[:data_end_idx])
        assert offset == expected_offset, f"Device thinks offset is {offset}, we think {expected_offset}"

        # Execute
        set_control(bytes([NRF_DFU_OP.OBJECT_EXECUTE]))
        rd = expect_response()

        # Update progress
        progress.update(transfer_size)
        bytes_remaining -= transfer_size

    # Postvalidate and boot happens automatically on completion

    # Progress bar needs closing
    progress.close()

    print("DONE")

